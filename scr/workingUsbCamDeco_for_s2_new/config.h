#pragma once

// ── WiFi AP ──────────────────────────────────────────────────
#define WIFI_SSID       "ESP32-Camera-AP"
#define WIFI_PASSWORD   "1234567890"
#define WIFI_HIDDEN     true   // 隐藏 SSID

// ── GPIO ─────────────────────────────────────────────────────
#define BUTTON_PIN      0
#define LED_FRAME_IND   15

// ── UVC / 帧缓冲 ─────────────────────────────────────────────
#define UVC_BUF_SIZE    (30 * 1024)   // 30KB，与 xfer buffer 一致
#define UVC_WIDTH       240
#define UVC_HEIGHT      320

// ── LCD 绘制偏移 ─────────────────────────────────────────────
#define DRAW_X          0
#define DRAW_Y          0
#define DEC_PREM        JPEG_USES_DMA

// ── 按钮防抖（微秒，使用 esp_timer_get_time()）───────────────
#define BTN_DEBOUNCE_US 50000LL   // 50ms
#define BTN_LONG_US     5000000LL // 5s
