#include <Arduino.h>
#include <LittleFS.h>
#include <bb_spi_lcd.h>
#include "JPEGDEC.h"
#include "USB_STREAM.h"
#include "esp_heap_caps.h"

#include "pin.h"        // TFT 引脚定义（S2_MINI / FEATHER32_S3）
#include "config.h"     // 宏常量
#include "qRcode.h"     // 内嵌 QR 码 JPEG（fallback）
#include "wifi_ap.h"    // WiFi / DNS / HTTP server（含 web_handlers, ota_handler）

/*
  依赖库：
    esp-arduino-libs/ESP32_USB_STREAM@^0.1.0
    bitbank2/bb_spi_lcd@^2.9.7
    bitbank2/JPEGDEC@^1.8.4

  Board: ESP32S2 Dev Module
    USB CDC On Boot      : Disabled
    CPU Frequency        : 240MHz (WiFi)
    Flash Mode           : QIO 80MHz
    Flash Size           : 4MB (32Mb)
    PSRAM                : Enabled
    Upload Mode          : UART0
    Partition Scheme     : Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)
*/

// ============================================================
// 硬件对象
// ============================================================
JPEGDEC    jpeg;
BB_SPI_LCD lcd;
USB_STREAM *usb = nullptr;

// ============================================================
// 全局状态
// ============================================================
bool wifiEnabled  = false;  // 默认不开启，短按再打开
bool lcdBlacked   = false;  // 有客户端连上后 LCD 黑屏，之后不再更新
bool showingQR    = false;  // WiFi 首次打开且无人连时显示过 QR 后置 true
bool streamPaused = false;  // OTA / 文件上传期间暂停推流
bool hasStation   = false;  // 是否有客户端连接

// ============================================================
// 帧缓冲（摄像头回调 ↔ Web 流共享）
// ============================================================
uint8_t          *webFrameBuffer = nullptr;
volatile size_t   webFrameLen    = 0;
SemaphoreHandle_t frameMutex     = nullptr;

// ============================================================
// 按钮中断（GPIO 0）
// 使用 esp_timer_get_time() 避免 millis() 在 ISR 中的不稳定性
// ============================================================
volatile int64_t buttonPressTime   = 0;
volatile int64_t lastInterruptTime = 0;
volatile bool    buttonPending     = false;

void IRAM_ATTR onButtonChange() {
  int64_t now = esp_timer_get_time();               // 微秒，ISR 安全
  if (now - lastInterruptTime < BTN_DEBOUNCE_US) return;
  lastInterruptTime = now;

  if (digitalRead(BUTTON_PIN) == LOW) {
    buttonPressTime = now;    // 按下：记录时间
  } else {
    buttonPending = true;     // 松开：通知主循环处理
  }
}

// ============================================================
// LCD 工具
// ============================================================
int drawMCUs(JPEGDRAW *pDraw) {
  lcd.setAddrWindow(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);
  lcd.pushPixels(pDraw->pPixels, pDraw->iWidth * pDraw->iHeight,
                 DRAW_TO_LCD | DRAW_WITH_DMA);
  return 1;
}

void lcdBlackScreen() {
  lcd.fillScreen(TFT_BLACK);
  lcdBlacked = true;
}

// yOffset: 顶部文字占用的像素高度，JPEG 从该 y 坐标开始绘制
void drawJPEG(uint8_t *buf, size_t size, int yOffset) {
  if (jpeg.openRAM(buf, size, drawMCUs)) {
    jpeg.setPixelType(RGB565_BIG_ENDIAN);
    jpeg.decode(0, yOffset, 0);  // decode(x, y, flags)
    jpeg.close();
  }
}

void drawJPEGFromFS(const char *path, int yOffset = 0) {
  if (LittleFS.exists(path)) {
    File f = LittleFS.open(path, "r");
    size_t size = f.size();
    uint8_t *buf = (uint8_t *)malloc(size);
    if (buf) {
      f.read(buf, size);
      drawJPEG(buf, size, yOffset);
      free(buf);
    }
    f.close();
  } else {
    // 回退：使用内嵌 QRCODE[]
    Serial.println("[LCD] QRcode.jpg not found, using built-in");
    drawJPEG((uint8_t *)QRCODE, sizeof(QRCODE), yOffset);
  }
}

// ============================================================
// 摄像头帧回调（USB_STREAM 内部 Task 调用）
// ============================================================
static void onCameraFrameCallback(uvc_frame *frame, void *) {
  digitalWrite(LED_FRAME_IND, HIGH);

  if (wifiEnabled && !hasStation) {
    // WiFi 开启但无人连接：保持 QR 显示，跳过帧处理
    digitalWrite(LED_FRAME_IND, LOW);
    return;
  }

  if (hasStation) {
    // 有客户端：只更新 webBuffer，不碰 LCD
    if (!streamPaused && frameMutex && webFrameBuffer) {
      if (xSemaphoreTake(frameMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        size_t len = min((size_t)frame->data_bytes, (size_t)UVC_BUF_SIZE);
        memcpy(webFrameBuffer, frame->data, len);
        webFrameLen = len;
        xSemaphoreGive(frameMutex);
      }
    }
  } else {
    // WiFi 关闭：解码显示到 LCD
    if (jpeg.openFLASH((uint8_t *)frame->data, frame->data_bytes, drawMCUs)) {
      jpeg.setPixelType(RGB565_BIG_ENDIAN);
      jpeg.decode(DRAW_X, DRAW_Y, DEC_PREM);
      jpeg.close();
    }
  }

  digitalWrite(LED_FRAME_IND, LOW);
}

// ============================================================
// HTTP 流推送（WebServer 的 handler，在主 loop 上下文调用）
// OTA / 上传期间 streamPaused=true，不推帧但保持连接
// ============================================================
void handleStream() {
  static bool isStreaming = false;
  if (isStreaming) {
    server.send(429, "text/plain", "Busy");
    return;
  }
  isStreaming = true;

  WiFiClient client = server.client();
  client.print("HTTP/1.1 200 OK\r\n"
               "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n");

  while (client.connected()) {
    // 让 HTTP server 在流循环中继续处理其他请求（OTA / 文件上传）
    server.handleClient();
    dnsServer.processNextRequest();

    if (!streamPaused && webFrameBuffer && webFrameLen > 0) {
      if (xSemaphoreTake(frameMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                      (unsigned)webFrameLen);
        client.write(webFrameBuffer, webFrameLen);
        client.print("\r\n");
        xSemaphoreGive(frameMutex);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(50));  // ~20fps 上限
  }
  isStreaming = false;
}

// ============================================================
// USB Host Task（独立 Task，避免 connectWait 卡死主线程）
// ============================================================
void usbHostTask(void *) {
  Serial.println("[USB] Task started");
  usb->start();
  usb->connectWait(portMAX_DELAY);  // 无限等待相机枚举完成
  Serial.println("[USB] Camera ready");
  vTaskDelete(nullptr);             // 完成后自行销毁
}

// ============================================================
// 按钮事件处理（在 loop 中调用，非 ISR）
// ============================================================
void handleButtonEvent() {
  if (!buttonPending) return;
  buttonPending = false;

  int64_t duration = esp_timer_get_time() - buttonPressTime;  // 微秒

  if (duration >= BTN_LONG_US) {
    // 长按 5s：格式化 LittleFS 并重启
    Serial.println("[BTN] Long press → format & restart");
    lcd.fillScreen(TFT_BLACK);
    lcd.setTextColor(TFT_RED, TFT_BLACK);
    lcd.setFont(FONT_12x16);
    lcd.setCursor(10, 100);
    lcd.println("Formatting FS...");
    LittleFS.format();
    ESP.restart();

  } else {
    // 短按：切换 WiFi AP
    wifiEnabled = !wifiEnabled;
    if (wifiEnabled) {
      wifiApStart();
      // 重置 QR 标志，让本次 WiFi 开启后能在无连接时显示 QR
      showingQR  = false;
      lcdBlacked = false;
    } else {
      wifiApStop();
      // WiFi 关闭：LCD 重新允许显示摄像头画面
      lcdBlacked = false;
      showingQR  = false;
    }
  }
}

// ============================================================
// Setup
// ============================================================
void setup() {
  Serial.begin(115200);  // 专用调试线，默认引脚（GPIO43/44 on S2）

  pinMode(LED_FRAME_IND, OUTPUT);
  digitalWrite(LED_FRAME_IND, LOW);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(BUTTON_PIN, onButtonChange, CHANGE);

  // ── LCD ──────────────────────────────────────────────────
  lcd.begin(LCD_ST7789, FLAGS_NONE, SPI_FREQUENCY,
            TFT_CS, TFT_DC, TFT_RST, TFT_BL,
            TFT_MISO, TFT_MOSI, TFT_SCLK);
  lcd.setFont(FONT_6x8);
  lcd.setTextColor(TFT_GREEN, TFT_BLACK);
  lcd.fillScreen(TFT_BLACK);
  lcd.setCursor(10, 10);
  lcd.println("Booting...");

  // ── LittleFS ─────────────────────────────────────────────
  if (!LittleFS.begin(true)) {
    Serial.println("[FS] Mount failed");
    lcd.println("FS Error!");
  } else {
    Serial.println("[FS] Mounted");
    lcd.println("FS OK");
  }

  // ── HTTP Server 路由提前注册（server.begin 在首次开启 WiFi 时才调用）
  wifiServerRegisterRoutes();

  // ── 帧缓冲 + 互斥锁 ──────────────────────────────────────
  frameMutex     = xSemaphoreCreateMutex();
  webFrameBuffer = (uint8_t *)heap_caps_malloc(UVC_BUF_SIZE, MALLOC_CAP_SPIRAM);
  if (!webFrameBuffer) {
    Serial.println("[MEM] webFrameBuffer PSRAM alloc FAILED, try SRAM");
    webFrameBuffer = (uint8_t *)malloc(UVC_BUF_SIZE);
  }
  if (!webFrameBuffer) {
    Serial.println("[MEM] webFrameBuffer ALLOC FAILED");
    lcd.println("BUF ALLOC FAIL!");
  }

  // ── USB Stream ───────────────────────────────────────────
  usb = new USB_STREAM();

  uint8_t *xferA  = (uint8_t *)malloc(UVC_BUF_SIZE);
  uint8_t *xferB  = (uint8_t *)malloc(UVC_BUF_SIZE);
  uint8_t *frameb = (uint8_t *)malloc(UVC_BUF_SIZE);
  assert(xferA && xferB && frameb);

  Serial.printf("[MEM] Free heap: %d bytes\n", esp_get_free_heap_size());
  lcd.println("Memory OK");

  usb->uvcConfiguration(UVC_WIDTH, UVC_HEIGHT, FRAME_INTERVAL_FPS_15,
                         UVC_BUF_SIZE, xferA, xferB,
                         UVC_BUF_SIZE, frameb);
  usb->uvcCamRegisterCb(&onCameraFrameCallback, nullptr);

  // USB Host 放独立 Task，不阻塞主线程
  xTaskCreate(usbHostTask, "usb_host", 4096, nullptr, 5, nullptr);

  lcd.println("USB init async...");
  lcd.println("Press BTN to enable WiFi");
  Serial.println("[MAIN] Setup done — WiFi off by default, press BTN to enable");
}

// ============================================================
// Loop
// ============================================================
void loop() {
  // 0. 更新状态
  if (wifiEnabled) {
    hasStation = (WiFi.softAPgetStationNum() > 0);
  } else {
    hasStation = false;
  }

  // 1. 按钮事件
  handleButtonEvent();

  // 2. WiFi 首次打开且无人连接 → 显示 QR（每次 WiFi 打开仅一次）
  if (wifiEnabled && !showingQR && !hasStation) {
    showingQR = true;
    lcd.fillScreen(TFT_BLACK);
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    lcd.setFont(FONT_12x16);
    // FONT_12x16 高度 16px，y=2 留少量上边距
    lcd.setCursor(2, 2);
    lcd.printf("SSID: %s", WIFI_SSID);
    // 文字区：16px 高 + 4px 间距 = 从 y=20 开始画图
    drawJPEGFromFS("/QRcode.jpg", 20);
  }

  // 3. 有客户端连上 → 确保 LCD 黑屏（只触发一次）
  if (wifiEnabled && hasStation && !lcdBlacked) {
    lcdBlackScreen();
  }

  // 4. WiFi 网络处理
  if (wifiEnabled) {
    wifiProcess();  // dnsServer + server.handleClient()
  }

  vTaskDelay(pdMS_TO_TICKS(10));
}
