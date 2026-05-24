#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <Update.h>
#include <bb_spi_lcd.h>
#include "JPEGDEC.h"
#include "USB_STREAM.h"
#include "pin.h"

/*lib
	esp-arduino-libs/ESP32_USB_STREAM@^0.1.0
	bitbank2/bb_spi_lcd@^2.9.7
	bitbank2/JPEGDEC@^1.8.4
*/

/*
Board: "Adafruit Feather ESP32-S3 TFT"

//----------------------------s3
USB CDC On Boot: "Disabled"
CPU Frequency: "240MHz (WiFi)"
Core Debug Level: "None"
USB DFU On Boot: "Disabled"
Erase All Flash Before Sketch Upload: "Disabled"
Events Run On: "Core 1"
Flash Mode: "QIO 80MHz"
Flash Size: "4MB (32Mb)"
Arduino Runs On: "Core 1"
USB Firmware MSC On Boot: "Disabled"
Partition Scheme: "TinyUF2 4MB (1.3MB APP/960KB FATFS)"
PSRAM: "QSPI PSRAM"
Upload Mode: "USB-OTG CDC (TinyUSB)"
Upload Speed: "921600"
USB Mode: "USB-OTG (TinyUSB)"
Zigbee Mode: "Disabled"
//-----------------------------s2
Board: "ESP32S2 Dev Module"

USB CDC On Boot: "Disabled"
CPU Frequency: "240MHz (WiFi)"
Core Debug Level: "Info"
USB DFU On Boot: "Disabled"
Erase All Flash Before Sketch Upload: "Disabled"
Flash Frequency: "80MHz"
Flash Mode: "QIO"
Flash Size: "4MB (32Mb)"
JTAG Adapter: "Disabled"
USB Firmware MSC On Boot: "Disabled"
Partition Scheme: "Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)"
PSRAM: "Enabled"
Upload Mode: "UARTO"
Upload Speed: "921600"
Zigbee Mode: "Disabled"


*/


JPEGDEC jpeg;
BB_SPI_LCD lcd;
bool ledup;

// WiFi and WebServer
const char *ssid = "ESP32-Camera-AP";
IPAddress apIP(192.168.4.1);
DNSServer dnsServer;
WebServer server(80);
bool wifiEnabled = true;

// GPIO 0 Button
#define BUTTON_PIN 0
unsigned long buttonPressTime = 0;
bool lastButtonState = HIGH;

// Default embedded webpage
const char* DEFAULT_INDEX_HTML =
"<html><head><title>ESP32 Camera Stream</title></head><body>"
"<h1>ESP32 Camera Stream (Default)</h1>"
"<img src=\"/stream\" style=\"width:100%; max-width:640px;\">"
"<br><hr><h3>File Management</h3>"
"<form method='POST' action='/upload' enctype='multipart/form-data'>Upload index.html/QRcode.jpg: <input type='file' name='upload'><input type='submit' value='Upload'></form>"
"<form method='POST' action='/delete'>Delete file: <input type='text' name='filename'><input type='submit' value='Delete'></form>"
"<br><hr><h3>OTA Update</h3>"
"<form method='POST' action='/update' enctype='multipart/form-data'>Firmware: <input type='file' name='update'><input type='submit' value='Update'></form>"
"</body></html>";

// Shared frame buffer for Web Server
uint8_t *webFrameBuffer = NULL;
size_t webFrameLen = 0;
SemaphoreHandle_t frameMutex = NULL;

//位移
#define DRAW_Y 0  //40
#define DRAW_X 0  //7
//#define DEC_PREM JPEG_SCALE_HALF | JPEG_USES_DMA
#define DEC_PREM JPEG_USES_DMA
#define LED_FRAME_IND 15

int drawMCUs(JPEGDRAW *pDraw) {
  int iCount;
  iCount = pDraw->iWidth * pDraw->iHeight;
  lcd.setAddrWindow(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);
  lcd.pushPixels(pDraw->pPixels, iCount, DRAW_TO_LCD | DRAW_WITH_DMA);
  return 1;
}

void handleRoot() {
  if (LittleFS.exists("/index.html")) {
    File file = LittleFS.open("/index.html", "r");
    server.streamFile(file, "text/html");
    file.close();
  } else {
    server.send(200, "text/html", DEFAULT_INDEX_HTML);
  }
}

void handleFileUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (filename != "index.html" && filename != "QRcode.jpg") {
       Serial.println("Invalid filename: " + filename);
       return;
    }
    File file = LittleFS.open("/" + filename, "w");
    file.close();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (upload.filename != "index.html" && upload.filename != "QRcode.jpg") return;
    File file = LittleFS.open("/" + upload.filename, "a");
    file.write(upload.buf, upload.currentSize);
    file.close();
  } else if (upload.status == UPLOAD_FILE_END) {
    server.send(200, "text/plain", "File Uploaded: " + upload.filename);
  }
}

void handleFileDelete() {
  String filename = server.arg("filename");
  if (LittleFS.exists("/" + filename)) {
    LittleFS.remove("/" + filename);
    server.send(200, "text/plain", "File Deleted");
  } else {
    server.send(404, "text/plain", "File Not Found");
  }
}

void handleUpdate() {
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
  ESP.restart();
}

void handleUpdateUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("Update: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  }
}

void handleStream() {
  WiFiClient client = server.client();
  String response = "HTTP/1.1 200 OK\r\n"
                    "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  client.print(response);

  while (client.connected()) {
    dnsServer.processNextRequest();

    // Allow button logic to run during streaming
    bool currentButtonState = digitalRead(BUTTON_PIN);
    if (lastButtonState == HIGH && currentButtonState == LOW) {
      buttonPressTime = millis();
    } else if (lastButtonState == LOW && currentButtonState == HIGH) {
      unsigned long duration = millis() - buttonPressTime;
      if (duration >= 5000) {
        Serial.println("Force format from stream loop...");
        LittleFS.format();
        ESP.restart();
      }
    }
    lastButtonState = currentButtonState;

    if (webFrameBuffer != NULL && webFrameLen > 0) {
      xSemaphoreTake(frameMutex, portMAX_DELAY);
      client.printf("--frame\r\n"
                     "Content-Type: image/jpeg\r\n"
                     "Content-Length: %u\r\n\r\n", webFrameLen);
      client.write(webFrameBuffer, webFrameLen);
      client.print("\r\n");
      xSemaphoreGive(frameMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(50)); // ~20fps max
  }
}

void handleNotFound() {
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void drawJPEGFromFS(const char* path) {
  if (LittleFS.exists(path)) {
    File file = LittleFS.open(path, "r");
    size_t size = file.size();
    uint8_t* buf = (uint8_t*)malloc(size);
    if (buf) {
      file.read(buf, size);
      if (jpeg.openRAM(buf, size, drawMCUs)) {
        jpeg.setPixelType(RGB565_BIG_ENDIAN);
        jpeg.decode(0, 0, 0);
        jpeg.close();
      }
      free(buf);
    }
    file.close();
  }
}

static void onCameraFrameCallback(uvc_frame *frame, void *user_ptr) {
  if (wifiEnabled && WiFi.softAPgetStationNum() == 0) {
    // Skip camera display if showing QR Code
    return;
  }
  Serial.println(">>> FRAME RECEIVED <<<");
  Serial.printf("Size: %dx%d, Bytes: %u\n",
                frame->width, frame->height, frame->data_bytes);
  digitalWrite(LED_FRAME_IND, ledup);
  ledup = !ledup;
  if (jpeg.openFLASH((uint8_t *)frame->data, frame->data_bytes, drawMCUs)) {
    jpeg.setPixelType(RGB565_BIG_ENDIAN);  // The SPI LCD wants the 16-bit pixels in big-endian order
    jpeg.decode(DRAW_Y, DRAW_X, DEC_PREM);
    jpeg.close();
  }

  // Update shared buffer for Web Server
  if (frameMutex != NULL && webFrameBuffer != NULL) {
    if (xSemaphoreTake(frameMutex, 0) == pdTRUE) {
      // Bounds check to prevent buffer overflow (webFrameBuffer is 64KB)
      if (frame->data_bytes <= 64 * 1024) {
        memcpy(webFrameBuffer, frame->data, frame->data_bytes);
        webFrameLen = frame->data_bytes;
      }
      xSemaphoreGive(frameMutex);
    }
  }
}

void setup() {

  Serial.begin(9600, SERIAL_8N1, 1, 2);
  // 打印内存信息
  //Serial.printf("Free Heap: %d bytes\n", esp_get_free_heap_size());
  //Serial.printf("Min Free Heap: %d bytes\n", esp_get_minimum_free_heap_size());
  pinMode(LED_FRAME_IND, OUTPUT);

  // Initialize WiFi AP
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // Initialize DNS Server
  dnsServer.start(53, "*", apIP);

  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
  }

  // Initialize GPIO 0
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Initialize Web Server
  server.on("/", handleRoot);
  server.on("/stream", handleStream);
  server.on("/upload", HTTP_POST, []() { /* server.send is handled in handleFileUpload UPLOAD_FILE_END */ }, handleFileUpload);
  server.on("/delete", HTTP_POST, handleFileDelete);
  server.on("/update", HTTP_POST, handleUpdate, handleUpdateUpload);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");

  // Initialize Shared Buffer and Mutex
  frameMutex = xSemaphoreCreateMutex();
  // Using 64KB for webFrameBuffer as per CONFIG_UVC_MAX_FRAME_BUFFER_SIZE in arduino_config.h
  webFrameBuffer = (uint8_t *)malloc(64 * 1024);
  if (webFrameBuffer == NULL) {
    Serial.println("Failed to allocate webFrameBuffer!");
  }

#ifdef ARDUINO_USB_MODE
  Serial.printf("ARDUINO_USB_MODE defined: %d\n", ARDUINO_USB_MODE);
#else
  Serial.println("ARDUINO_USB_MODE not defined");
#endif

#ifdef ARDUINO_USB_CDC_ON_BOOT
  Serial.printf("ARDUINO_USB_CDC_ON_BOOT: %d\n", ARDUINO_USB_CDC_ON_BOOT);
#else
  Serial.println("ARDUINO_USB_CDC_ON_BOOT not defined");
#endif

#ifdef CONFIG_TINYUSB_ENABLED
  Serial.println("TinyUSB is ENABLED");
#else
  Serial.println("TinyUSB is DISABLED");
#endif
  Serial.println("==================================\n");
  USB_STREAM *usb = new USB_STREAM();
  Serial.println("init USB");
  lcd.begin(LCD_ST7789, FLAGS_NONE, SPI_FREQUENCY, TFT_CS, TFT_DC, TFT_RST, TFT_BL, TFT_MISO, TFT_MOSI, TFT_SCLK);
  lcd.setFont(FONT_6x8);
  lcd.setTextColor(TFT_GREEN, TFT_BLACK);
  lcd.setCursor(20, 20);
  Serial.println("init LCD");
  //  lcd.begin(LCD_ST7789_135, FLAGS_NONE, 40000000, 7, 39, 40, 45, 37, 35, 36);
  lcd.fillScreen(TFT_BLACK);
  lcd.println("LCD initiated.");
  Serial.println("init Mem");
  // allocate memory
  uint32_t bufSize = 30 * 1024;
  uint8_t *_xferBufferA = (uint8_t *)malloc(bufSize);
  assert(_xferBufferA != NULL);
  uint8_t *_xferBufferB = (uint8_t *)malloc(bufSize);
  assert(_xferBufferB != NULL);
  uint8_t *_frameBuffer = (uint8_t *)malloc(bufSize);
  assert(_frameBuffer != NULL);
  Serial.printf("Memory allocated successfully. Free Heap now: %d\n", esp_get_free_heap_size());
  lcd.println("Memory allocated successfully.");
  Serial.println("2. Configuring UVC");
  // Config the parameter
  usb->uvcConfiguration(240, 320, FRAME_INTERVAL_FPS_15, bufSize, _xferBufferA, _xferBufferB, bufSize, _frameBuffer);
  usb->uvcCamRegisterCb(&onCameraFrameCallback, NULL);
  //esp_panic_handler(esp_panic_handler);
  Serial.println("3. Starting USB");
  usb->start();
  lcd.println("USB Starting successfully.");
  Serial.println("4. Waiting connecting\n");
  usb->connectWait(10000);
  Serial.printf("5. Setup done,\n awaiting camera initialization");
  lcd.println("Awaiting for connection and camera initialization.");

  // usb->uvcCamSuspend(NULL);
  // delay(5000);

  // usb->uvcCamResume(NULL);

  /*Dont forget to free the allocated memory*/
  // free(_xferBufferA);
  // free(_xferBufferB);
  // free(_frameBuffer);
}

bool showingQRCode = false;

void loop() {
  if (wifiEnabled && WiFi.softAPgetStationNum() == 0) {
    if (!showingQRCode) {
      lcd.fillScreen(TFT_BLACK);
      lcd.setTextColor(TFT_WHITE, TFT_BLACK);
      lcd.setFont(FONT_12x16);
      lcd.setCursor(10, 10);
      lcd.printf("SSID: %s", ssid);
      drawJPEGFromFS("/QRcode.jpg");
      showingQRCode = true;
    }
  } else {
    showingQRCode = false;
  }

  // GPIO 0 Button Logic
  bool currentButtonState = digitalRead(BUTTON_PIN);
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    buttonPressTime = millis();
  } else if (lastButtonState == LOW && currentButtonState == HIGH) {
    unsigned long duration = millis() - buttonPressTime;
    if (duration < 5000) {
      // Short press: Toggle WiFi
      wifiEnabled = !wifiEnabled;
      if (wifiEnabled) {
        WiFi.softAP(ssid);
        dnsServer.start(53, "*", apIP);
        Serial.println("WiFi AP Enabled");
      } else {
        WiFi.softAPdisconnect(true);
        dnsServer.stop();
        Serial.println("WiFi AP Disabled");
      }
    } else {
      // Long press: Format LittleFS and Reboot
      Serial.println("Formatting LittleFS...");
      LittleFS.format();
      ESP.restart();
    }
  }
  lastButtonState = currentButtonState;

  if (wifiEnabled) {
    dnsServer.processNextRequest();
    server.handleClient();
  }
  //static int count = 0;
  //Serial.printf("Loop %d\n", count++);
  Serial.printf(".");
  vTaskDelay(pdMS_TO_TICKS(10));
}
