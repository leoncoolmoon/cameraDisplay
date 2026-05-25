#pragma once

#define SPI_FREQUENCY  40000000
#define S2_MINI  // FEATHER32_S3

#ifdef FEATHER32_S3
  #define TFT_MISO 37
  #define TFT_MOSI 35
  #define TFT_SCLK 36
  #define TFT_BL   45
  #define TFT_CS    7
  #define TFT_DC   39
  #define TFT_RST  40
#elif defined(S2_MINI)
  #define TFT_MISO 37  // not in use
  #define TFT_MOSI 35
  #define TFT_SCLK 36
  #define TFT_BL   38  // not in use
  #define TFT_CS   34
  #define TFT_DC   39
  #define TFT_RST  40
#endif
