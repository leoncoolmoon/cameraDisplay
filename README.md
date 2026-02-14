# 📷 cameraDisplay

A portable USB camera display solution developed under **Arduino IDE 2.3.7**. This project allows you to plug a UVC camera into an ESP32-S2 and display the live feed on a small TFT screen.

## 📺 Demo & Preview
[![Watch the video](https://img.youtube.com/vi/0k7rjw5rCmI/maxresdefault.jpg)](https://www.youtube.com/shorts/0k7rjw5rCmI)
*Check out the 3D models on my [Thingiverse](https://www.thingiverse.com/thing:7290425) page.*
The current design can continually run on battery for over 1 hour.
Ideal for USB cameras with low resolutions. i.e.,a thermal camera like Haikang 4117 as per demo
---

## 🛠 Hardware Requirement

| Component | Specification |
| :--- | :--- |
| **Microcontroller** | Wemos S2 Mini (ESP32-S2) |
| **Display** | GMT020 (TFT LCD) |
| **Power** | Power bank module + 102540 Li-ion Battery |
| **Switch**| SS12F15VG5 5MM/3MM |

### Pin Mapping
| Function | GPIO Pin |
| :--- | :--- |
| `TFT_MOSI` | 35 |
| `TFT_SCLK` | 36 |
| `TFT_CS` | 34 |
| `TFT_DC` | 39 |
| `TFT_RST` | 40 |
| `LED_FRAME_IND`| 15 |

---

## 📚 Libraries Used
- `esp-arduino-libs/ESP32_USB_STREAM @ ^0.1.0`
- `bitbank2/bb_spi_lcd @ ^2.9.7`
- `bitbank2/JPEGDEC @ ^1.8.4`

---

## ⚠️ Important Installation Steps

> [!IMPORTANT]
> You **MUST** replace the original configuration file to enable the USB stream:
> 
> **Replace:** `~\Arduino\libraries\ESP32_USB_STREAM\src\original\include\arduino_config.h`
> **With:** The `arduino_config.h` provided in this repository.

**Current Limitations:**
- Currently only supports **UVC MJPEG 320x240** format.

---

## 🎯 Project Goals
- [x] Battery-powered portable screen.
- [x] Plug-and-play USB camera image display.

## 📝 TODO List
- [ ] **OTA:** Support Over-the-Air updates.
- [ ] **Wireless:** Stream in/out via Wi-Fi or ESP-NOW.
- [ ] **Storage:** Add TF card support for recording and playback.
