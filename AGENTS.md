# AGENTS: Project Notes & How-To

## Overview
- **Project**: ESPHome custom component for **M5Stack M5 Dial** (ESP32-S3).
- **Build System**: ESPHome (Python CLI) using PlatformIO with **Arduino framework**.
- **Repo**: `https://github.com/SmartHome-yourself/m5-dial-for-esphome/` (external component).

## Build & Run
- **Activate venv** (PowerShell):
  ```ps1
  ..\esphome\venv\Scripts\Activate.ps1
  ```
- **Compile**:
  ```ps1
  esphome compile .\shys-m5-dial.yaml
  ```
- **Flash/Logs** (typical ESPHome flows):
  ```ps1
  esphome run .\shys-m5-dial.yaml   # compile + upload (OTA/serial)
  esphome logs .\shys-m5-dial.yaml  # live logs
  ```
- **Note**: `esphome` CLI is installed in `..\esphome\venv` (sibling repo `G:\GitHub\esphome`).

## Key Config (`shys-m5-dial.yaml`)
- **Board**: `esp32-s3-devkitc-1`, `framework: arduino`, `flash_size: 8MB`.
- **Pins/Libraries**:
  - `Wire`, `EEPROM`
  - `bblanchon/ArduinoJson@7.4.2`
  - `m5stack/M5Unified@0.2.13`
  - `m5stack/M5Dial@1.0.3`
- **UI**:
  ```yaml
  ui_theme:
    background: yellow
    foreground: white
    accent: orange
    text: black
  ui_transitions: false          # set true to enable crossfade transitions
  ui_transition_duration_ms: 300
  ui_animations: true            # smooth progress arcs
  ```
- **Extra include paths** (fix for `esp_lcd_panel_io.h`):
  ```yaml
  build_flags:
    - -I$PROJECT_PACKAGES_DIR/framework-espidf/components/esp_lcd/include
    - -I$PROJECT_PACKAGES_DIR/framework-arduinoespressif32/tools/sdk/esp32s3/include/esp_lcd
  ```
- **External component**:
  ```yaml
  external_components:
    - source:
        type: git
        url: https://github.com/SmartHome-yourself/m5-dial-for-esphome/
        ref: main
      # For local dev, uncomment:
      # uri: ./components/
      # components: [shys_m5_dial]
  ```

## Known Warnings / Notes
- ESPHome warns if Wi-Fi AP is configured without `captive_portal`/`web_server`.
- **ESPHome version** used: `2026.2.0-dev` (from venv).
- **Toolchains** pulled: Arduino-ESP32 3.3.6, ESP-IDF 5.5.2.

## Pending Code Improvements (not required to build)
- Replace `#include "esphome.h"` with explicit headers in component sources.
- Update Home Assistant subscription callbacks to new signature (`HomeAssistantStateResponse`).
- Adjust ArduinoJson usage to v7 API (explicit allocator when needed).
- Minor fixes: `M5DialDisplay::setBackgroundColor` assignment; null-check `api::global_api_server` before use.

## Troubleshooting
- **`esp_lcd_panel_io.h` missing**: ensure `build_flags` includes esp_lcd paths (as above).
- If `esphome` is not found: activate venv or `python -m pip install esphome` in your environment.
- Use `esphome compile -v` for verbose build logs.

## Quick Status
- ✅ Build succeeds with current pins and include paths.
- ℹ️ External component still references upstream; switch to local for code edits.
