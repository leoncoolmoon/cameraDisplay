# cameraDisplay
Developed under Arduino IDE 2.3.7
library:
	esp-arduino-libs/ESP32_USB_STREAM@^0.1.0
	bitbank2/bb_spi_lcd@^2.9.7
	bitbank2/JPEGDEC@^1.8.4
Hardware:

wemos s2 mini + gmt020 + power bank module +102540 battery

Pin:

#define TFT_MOSI 35  
#define TFT_SCLK 36  
#define TFT_CS 34    
#define TFT_DC 39    
#define TFT_RST 40   
#define LED_FRAME_IND 15

!!important
Need to use the arduino_config.h to replace the ~\Arduino\libraries\ESP32_USB_STREAM\src\original\include\arduino_config.h
At monument only support UVC mjpeg 320x240 format

Aim:

 make a battery powered screen can
 1. plug usb camera and display the image
 
TODO:
 1. OTA
 2. can stream in/out with wifi or ESPNOW
 3. add tf card and can record/ play from sdCard


You can check my https://www.thingiverse.com/ page for the model preview

[![preview](https://img.youtube.com/vi/0k7rjw5rCmI/maxresdefault.jpg)](https://www.youtube.com/shorts/0k7rjw5rCmI)
