# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Flash

```bash
# Build firmware
pio run

# Build and upload to connected ESP-07
pio run --target upload

# Upload LittleFS image (data/index.html → flash)
pio run --target uploadfs

# Monitor serial output (115200 baud)
pio device monitor

# Build + upload + monitor in one step
pio run --target upload && pio device monitor
```

## Testing the frontend locally

```bash
python3 frontend-test-server.py   # serves on http://localhost:8378
```

Emulates `GET /api/freq`, `POST /api/freq`, `GET /api/multiplier`, `POST /api/multiplier`.

## Architecture

Firmware for controlling an **AD9852 DDS (Direct Digital Synthesis)** chip via a browser-accessible HTTP interface, running on an **ESP-07 (ESP8266)** module.

**Two-layer design:**

- `src/ad9852.cpp` / `include/ad9852.hpp` — hardware driver: GPIO init, master reset, software SPI (bit-bang, mode 0, MSB-first), FTW calculation, register writes, runtime multiplier control.
- `src/main.cpp` — Arduino sketch: connects to Wi-Fi (STA mode), hosts an HTTP server on port 80. Credentials in `secrets.hpp` (copy from `secrets.example.hpp`).
- `data/index.html` — single-page control UI served from LittleFS.

**HTTP API:**

| Method | Path              | Body / params   | Description                          |
|--------|-------------------|-----------------|--------------------------------------|
| GET    | `/`               | —               | Serves `data/index.html` from LittleFS |
| GET    | `/api/freq`       | —               | Returns current frequency (Hz, plain text) |
| POST   | `/api/freq`       | `freq=<hz>`     | Sets output frequency                |
| GET    | `/api/multiplier` | —               | Returns current PLL multiplier (1–15) |
| POST   | `/api/multiplier` | `mult=<n>`      | Sets multiplier; re-tunes FTW to keep output frequency constant |

**DDS frequency math:**
- REFCLK = 20 MHz (fed to AD9852 reference input)
- SYSCLK = REFCLK × multiplier (PLL enabled, multiplier 1–15, valid hardware range 4–20)
- 48-bit FTW = `round(freq_hz × 2^48 / SYSCLK)`
- `FQ = 2^48 / REFCLK_HZ` (precomputed constant); actual FTW = `round(FQ / mult × freq_hz)`
- Output clamped to `SYSCLK × 0.4`; frontend caps display at 99,999,999 Hz

**GPIO mapping (ESP-07 → AD9852):**

| AD9852 signal (serial / parallel) | GPIO | Notes                                                                                                       |
|------------------------------------|------|-------------------------------------------------------------------------------------------------------------|
| MASTER_RESET                       | 12   | Active HIGH; no boot constraint                                                                             |
| SCB / RDB (chip select)            | 15   | Active LOW; GPIO15 pull-down holds it LOW at boot (chip "selected") — harmless since SCLK won't toggle      |
| IO_RESET / A2                      | 13   | Active HIGH pulse; no boot constraint                                                                       |
| SCLK / WRB                         | 2    | GPIO2 pull-up holds HIGH at boot (safe); UART1 TX glitch during reset is cleared by subsequent MASTER_RESET |
| I/O UD                             | 4    | Active HIGH pulse; no boot constraint                                                                       |
| SDIO / A0                          | 5    | Write-only; no MISO                                                                                         |

**SPI transaction sequence:** `chip_select()` → `io_reset()` → write address byte → write data bytes → `io_update()` → `chip_release()`.

**Multiplier change sequence:** write new control register → 50 ms PLL relock delay → rewrite FTW computed for new SYSCLK.

**Flash layout:**
- Firmware: uploaded via `pio run --target upload`
- LittleFS: uploaded separately via `pio run --target uploadfs`; contains `data/index.html`
- `platformio.ini` sets `board_build.filesystem = littlefs`

## Notes

- Directory is named `ad9854-esp8266` but the chip and all code target the **AD9852**.
- `secrets.hpp` is gitignored. Copy `secrets.example.hpp`, fill in Wi-Fi SSID/password and OTA password.
- Do not use `static const` arrays in driver code — the ESP8266 linker places them in IROM (flash), and byte-level access via a pointer causes Exception 3. Use plain local arrays instead.
- The `ctrl[]` array in `write_control()` is intentionally a stack-local (DRAM) variable for this reason.