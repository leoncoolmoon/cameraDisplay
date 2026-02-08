#define SPI_FREQUENCY  40000000
#define S2_MINI//FEATHER32_S3
#ifdef FEATHER32_S3
#define TFT_MISO 37  // Automatically assigned with ESP8266 if not defined
#define TFT_MOSI 35  // Automatically assigned with ESP8266 if not defined
#define TFT_SCLK 36  // Automatically assigned with ESP8266 if not defined
#define TFT_BL 45    // LED back-light control pin
#define TFT_CS 7     // Chip select control pin D8
#define TFT_DC 39    // Data Command control pin
#define TFT_RST 40   // Reset pin (could connect to NodeMCU RST, see next line)
#elif defined (S2_MINI)
#define TFT_MISO 37  // Automatically assigned with ESP8266 if not defined // not in use
#define TFT_MOSI 35  // Automatically assigned with ESP8266 if not defined
#define TFT_SCLK 36  // Automatically assigned with ESP8266 if not defined
#define TFT_BL 38    // LED back-light control pin // not in use
#define TFT_CS 34     // Chip select control pin D8
#define TFT_DC 39    // Data Command control pin
#define TFT_RST 40   // Reset pin (could connect to NodeMCU RST, see next line)
#endif