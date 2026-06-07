# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

PLAT is a smart parking and license plate recognition system built for a diploma student project. It uses a XIAO ESP32-S3 Sense camera to capture still images, a laptop server (Docker) to run OCR and whitelist checking, and an ESP32 Dev Board to control entry/exit barriers, display results, and monitor parking availability.

## Repository Structure

```
plat/
├── server/
│   ├── main.py              # FastAPI — OCR, whitelist check, web dashboard routes, login auth
│   ├── database.py          # SQLite CRUD — cars + users tables, bcrypt password hashing
│   ├── requirements.txt     # Python dependencies
│   └── templates/
│       ├── login.html       # Login page
│       └── index.html       # Car registry dashboard — view, add, edit, delete cars
├── esp32/
│   ├── plat_camera/
│   │   └── plat_camera.ino  # XIAO ESP32-S3 Sense firmware
│   └── plat_display/
│       └── plat_display.ino # ESP32 Dev Board firmware
├── docs/
│   ├── setup.md             # Project description, architecture, BOM
│   ├── instruction.md       # Step-by-step setup guide
│   └── connection.md        # Full pin wiring for both boards
├── Dockerfile               # Docker image for FastAPI server
├── docker-compose.yml       # Starts server + mounts SQLite volume
└── .gitignore
```

## Architecture

Two ESP32 boards + one laptop server, all on the same local Wi-Fi network.

```
[Entry Flex Sensor] → [XIAO ESP32-S3 Sense]
                              │ 5 frames captured (400ms apart)
                              │ sharpest JPEG sent via HTTP POST
                              ↓
                       [Laptop - Docker]
                       FastAPI + EasyOCR + SQLite
                              │ {plate, verified} returned to XIAO
                              ↓
                       XIAO forwards result via HTTP POST
                              ↓
                       [ESP32 Dev Board]
                       OLED 1, Servo 1 (entry)
                       PIR x3, OLED 2 (parking)
                       Exit Flex Sensor, Servo 2 (exit)
```

## System Logic — Critical Rules

### Image Capture (XIAO ESP32-S3 Sense)
- Captures **5 still frames** at **400ms intervals** — not a video stream
- Discards 2 warm-up frames first to let exposure and white balance settle
- Selects the frame with the **largest JPEG size** (proxy for sharpness)
- Sends only the best frame to the server as raw bytes (`Content-Type: image/jpeg`)
- Configurable via `CAPTURE_COUNT` and `CAPTURE_INTERVAL_MS` in firmware

### Entry Flow (XIAO → Server → XIAO → Dev Board)
- XIAO sends image to server `/recognize` → receives `{plate, verified}`
- XIAO POSTs result to Dev Board `/entry`
- Dev Board opens Servo 1 if `verified = true`
- XIAO waits for flex sensor to release (car passed) → POSTs `/close-entry` to Dev Board
- Dev Board closes Servo 1
- XIAO returns to deep sleep

### Entry Barrier (Servo 1 — GPIO13)
- Opens **0° → 90°** when Dev Board receives `verified = true` from XIAO
- Closes **90° → 0°** when Dev Board receives `/close-entry` from XIAO

### Exit Barrier (Servo 2 — GPIO14)
- Opens **0° → 90°** when exit flex sensor analog read exceeds `FLEX_EXIT_THRESHOLD` (default 1500)
- Closes **90° → 0°** when exit flex sensor is released
- Handled entirely on ESP32 Dev Board — no server or XIAO involved

### Deep Sleep (XIAO ESP32-S3 Sense)
- Wakes via **ext0 on GPIO1 (D0)** when entry flex sensor goes HIGH
- Returns to deep sleep after `/close-entry` is sent to Dev Board
- Pull-down 10kΩ on GPIO1 prevents false wake-up

### Parking Monitoring
- 3x PIR sensors on GPIO25, GPIO26, GPIO27 run continuously on ESP32 Dev Board
- OLED 2 redraws only when PIR state changes — prevents flicker

## Hardware Pin Reference

### XIAO ESP32-S3 Sense
| GPIO | Label | Connected To |
|---|---|---|
| GPIO1 | D0 | Entry flex sensor + 10kΩ pull-down |
| Built-in | — | OV3640 camera |

### ESP32 Dev Board
| GPIO | Connected To |
|---|---|
| GPIO13 | Servo 1 — entry barrier signal |
| GPIO14 | Servo 2 — exit barrier signal |
| GPIO21 | I2C SDA — OLED 1 (0x3C) + OLED 2 (0x3D) |
| GPIO22 | I2C SCL — OLED 1 (0x3C) + OLED 2 (0x3D) |
| GPIO25 | PIR Sensor 1 — Parking Lot 1 |
| GPIO26 | PIR Sensor 2 — Parking Lot 2 |
| GPIO27 | PIR Sensor 3 — Parking Lot 3 |
| GPIO34 | Exit flex sensor + 10kΩ pull-down (input-only pin) |

Power: OLEDs + PIR sensors → 3.3V. Servos → 5V (VIN). Never power servos from 3.3V.

## Server API

| Route | Method | Auth | Used By |
|---|---|---|---|
| `/recognize` | POST | None | XIAO — sends raw JPEG bytes (`Content-Type: image/jpeg`), receives `{plate, verified}` |
| `/entry` | POST | None | XIAO — forwards `plate` + `verified` to Dev Board web server (port 80) |
| `/close-entry` | POST | None | XIAO — tells Dev Board to close entry barrier |
| `/login` | GET / POST | None | Browser — login page; POST validates credentials and starts session |
| `/logout` | GET | None | Browser — clears session, redirects to `/login` |
| `/` | GET | Required | Browser — car registry dashboard |
| `/cars/add` | POST | Required | Browser — add car (plate, matrix, first_name, last_name, manufacturer, model, colour) |
| `/cars/edit/{plate}` | POST | Required | Browser — edit an existing car record |
| `/cars/delete/{plate}` | POST | Required | Browser — delete a car record |

## Server Implementation Notes

- `/recognize` accepts raw bytes via `request.body()` — not multipart
- `TemplateResponse` uses the new Starlette API: `templates.TemplateResponse(request=request, name="index.html", context={...})` — the `request` is a keyword arg, not inside the context dict
- EasyOCR reader is initialised once at startup via `lifespan` — do not move it into the route handler
- Session auth uses `SessionMiddleware` (itsdangerous) — `request.session["username"]` set on login, cleared on logout
- Password hashing uses `bcrypt` directly — **do not use `passlib`** (incompatible with bcrypt 4.x)
- `database.py` exposes `hash_password(plain)` and `verify_password(plain, hashed)` — use these, not bcrypt calls directly
- Default user `admin` / `admin@1234` is created on first `init_db()` call if the users table is empty
- `cars` table columns: `id, plate, matrix, first_name, last_name, manufacturer, model, colour, created_at`
- `users` table columns: `id, username, password_hash, first_name, last_name, phone, created_at`
- If the database was created before login was added, rebuild with `docker compose down -v && docker compose up --build` to recreate it

## Firmware Configuration

Both sketches have a config block at the top. Update before uploading:

**`plat_camera.ino`**
```cpp
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* SERVER_URL    = "http://<laptop-ip>:8000/recognize";
const char* DEVBOARD_IP   = "<esp32-devboard-ip>";  // from Dev Board Serial Monitor
#define CAPTURE_COUNT       5
#define CAPTURE_INTERVAL_MS 400
```

**`plat_display.ino`**
```cpp
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
#define FLEX_EXIT_THRESHOLD 1500  // adjust if exit barrier triggers incorrectly
```

## Development Commands

```bash
# Start server
docker compose up

# Start server in background
docker compose up -d

# Stop server
docker compose down

# Rebuild after code changes
docker compose up --build

# View server logs
docker compose logs -f
```

## Key Constraints

- **Still image only** — do not switch to MJPEG/video stream; deep sleep cycle is incompatible with streaming
- **Both OLEDs share I2C bus** — OLED 1: `0x3C`, OLED 2: `0x3D`. Solder A0 jumper on OLED 2 to change its address
- **GPIO34 is input-only** on ESP32 Dev Board — never assign as output
- **GPIO1 must be RTC-capable** for ext0 deep sleep wake-up on XIAO ESP32-S3
- **Servos require 5V** — 3.3V will not drive them reliably
- **All GNDs must share common ground**, including external 5V supply for servos
- **EasyOCR downloads ~500MB of models on first Docker run** — internet required for first startup
- All three devices must be on the **same Wi-Fi network**
- Flash `plat_display.ino` first — copy its IP from Serial Monitor into `DEVBOARD_IP` in `plat_camera.ino` before flashing XIAO
