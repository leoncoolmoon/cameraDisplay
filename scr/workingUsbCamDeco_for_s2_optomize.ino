#include <Arduino.h>
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

static void onCameraFrameCallback(uvc_frame *frame, void *user_ptr) {
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
}

void setup() {

  Serial.begin(9600, SERIAL_8N1, 1, 2);
  // 打印内存信息
  //Serial.printf("Free Heap: %d bytes\n", esp_get_free_heap_size());
  //Serial.printf("Min Free Heap: %d bytes\n", esp_get_minimum_free_heap_size());
  pinMode(LED_FRAME_IND, OUTPUT);



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

void loop() {
  //static int count = 0;
  //Serial.printf("Loop %d\n", count++);
  Serial.printf(".");
  vTaskDelay(100);
}
