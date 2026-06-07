# Project: PLAT

## Purpose

PLAT is a license plate recognition system using the Seeed Studio XIAO ESP32-S3 Sense camera module. The system detects vehicle presence, captures a still image, recognises the license plate number, checks it against a verified list, and controls a barrier automatically.

## Hardware

### XIAO ESP32-S3 Sense (Camera Node — Entry)
- Built-in OV3640 camera — captures still images (5 frames, sharpest selected)
- Flex sensor — detects vehicle at entry, wakes ESP32 from deep sleep

### ESP32 Dev Board (Control Node)
- **Entry side:** OLED 1 (plate + status), Servo 1 (entry barrier)
- **Parking monitoring:** 3x PIR sensors (one per lot), OLED 2 (availability display)
- **Exit side:** Flex sensor (exit detection), Servo 2 (exit barrier)

### Laptop
- Runs Python FastAPI server inside Docker
- Handles plate detection, OCR, whitelist check, and web dashboard

## System Flow

### Entry Flow
1. XIAO ESP32-S3 sits in **deep sleep** to save power
2. Entry **flex sensor** detects a vehicle → wakes up XIAO ESP32-S3
3. XIAO ESP32-S3 activates camera and captures **5 still frames** at 400ms intervals
4. Sharpest frame is selected and sent to laptop server via Wi-Fi (HTTP POST)
5. Laptop server detects plate region, runs **OCR** to extract plate number
6. Server **checks plate number** against whitelist (verified / not verified)
7. Server returns **plate number + status** to XIAO ESP32-S3
8. XIAO ESP32-S3 forwards result to ESP32 Dev Board
9. ESP32 Dev Board shows plate number and status on **OLED 1**
10. If status is **verified** → Servo 1 rotates **0° to 90°** (entry barrier opens)
11. Entry flex sensor is **no longer pressed** (car has passed) → XIAO sends close signal → Servo 1 returns **90° to 0°** (entry barrier closes)
12. XIAO ESP32-S3 returns to **deep sleep**

### Parking Availability Flow
1. 3x PIR sensors continuously monitor 3 parking lots
2. ESP32 Dev Board reads each PIR sensor state
3. **OLED 2** updates to show which lots are available or occupied

### Exit Flow
1. Exit **flex sensor** detects a vehicle leaving
2. ESP32 Dev Board triggers Servo 2 to rotate **0° to 90°** (exit barrier opens)
3. Exit flex sensor is **no longer pressed** (car has passed) → Servo 2 returns **90° to 0°** (exit barrier closes)

## Architecture

```
[Vehicle arrives at entry]
        │
        ▼
[Entry Flex Sensor] ──wake──► [XIAO ESP32-S3 Sense]
                                      │ best JPEG from 5 frames (Wi-Fi HTTP POST)
                                      ▼
                               [Laptop Server - Docker]
                               - Plate detection (OpenCV)
                               - OCR (EasyOCR)
                               - Whitelist check (SQLite)
                               - Web dashboard (FastAPI)
                                      │ plate number + status (Wi-Fi HTTP)
                                      ▼
                            ┌──────────────────────────┐
                            │      ESP32 Dev Board      │
                            │                           │
                            │  OLED 1 (plate + status)  │
                            │  Servo 1 (entry barrier)  │
                            │  0°→90° open, 90°→0° close│
                            │                           │
                            │  PIR 1 → Parking Lot 1    │
                            │  PIR 2 → Parking Lot 2    │
                            │  PIR 3 → Parking Lot 3    │
                            │  OLED 2 (availability)    │
                            │                           │
                            │  Exit Flex Sensor         │
                            │  Servo 2 (exit barrier)   │
                            │  0°→90° open, 90°→0° close│
                            └──────────────────────────┘
```

## Tech Stack

- **Firmware (XIAO ESP32-S3 Sense)**: Arduino (C/C++) — deep sleep, flex sensor wake, 5-frame capture, best frame sent to server
- **Firmware (ESP32 Dev Board)**: Arduino (C/C++) — OLED x2, servo x2, PIR x3, flex sensor (exit)
- **Communication**: Wi-Fi HTTP — XIAO streams frames to laptop; laptop sends result to ESP32 Dev Board
- **Server**: Python (FastAPI) running in Docker on the laptop
- **Plate Detection & OCR**: OpenCV + EasyOCR inside Docker container
- **Database**: SQLite — two tables: `cars` (plate, matrix, owner details, vehicle info) and `users` (login accounts); persisted via Docker volume
- **Web Dashboard**: FastAPI serves browser UI with login auth to view, add, edit, and delete registered cars
- **Containerisation**: Docker + Docker Compose — single `docker compose up` to start server
- **Version Control**: GitHub

## Web Dashboard

Accessible from any browser on the same network at `http://<laptop-ip>:8000`.  
Login required — default credentials: **admin / admin@1234**

| Route | Function |
|---|---|
| `GET /login` | Login page |
| `GET /logout` | Logout and return to login page |
| `GET /` | Car registry dashboard (protected) |
| `POST /cars/add` | Add a new car record (plate, matrix, name, vehicle, colour) |
| `POST /cars/edit/{plate}` | Edit an existing car record |
| `POST /cars/delete/{plate}` | Delete a car record |
| `POST /recognize` | ESP32-S3 sends image here for OCR + whitelist check (no auth) |

## Development Setup

- All devices (XIAO ESP32-S3, ESP32 Dev Board, laptop) must be on the **same Wi-Fi network**
- Laptop IP found using `ipconfig` in Command Prompt
- Server starts with `docker compose up` — port 8000
- SQLite database persists across container restarts via Docker volume
- Source code managed on GitHub — clone and run `docker compose up` to replicate

## Bill of Materials

### Microcontrollers

| No. | Component | Qty | Purpose |
|---|---|---|---|
| 1 | Seeed Studio XIAO ESP32-S3 Sense | 1 | Camera node — captures 5 frames, sends sharpest to server |
| 2 | ESP32 Dev Board | 1 | Control node — display, barriers, parking monitoring |

### Sensors

| No. | Component | Qty | Purpose |
|---|---|---|---|
| 3 | Flex Sensor | 2 | Entry: wakes XIAO ESP32-S3 from deep sleep; Exit: triggers exit barrier |
| 4 | PIR Sensor (HC-SR501) | 3 | One per parking lot — detects car presence |

### Actuators

| No. | Component | Qty | Purpose |
|---|---|---|---|
| 5 | Servo Motor (SG90) | 2 | Entry barrier + Exit barrier (0° to 90°) |

### Display

| No. | Component | Qty | Purpose |
|---|---|---|---|
| 6 | OLED Display 128x64 I2C (0.96") | 2 | OLED 1: plate number + status; OLED 2: parking availability |

### Passive Components & Accessories

| No. | Component | Qty | Purpose |
|---|---|---|---|
| 7 | Resistor 10kΩ | 2 | Pull-down resistor for each flex sensor |
| 8 | Breadboard | 2 | Prototyping connections |
| 9 | Jumper Wires (M-M, M-F) | 40+ | Connecting components |
| 10 | USB-C Cable | 1 | Flashing XIAO ESP32-S3 Sense |
| 11 | USB-A to Micro-USB Cable | 1 | Flashing ESP32 Dev Board |
| 12 | 5V External Power Supply | 1 | Powering servo motors (do not use 3.3V pin) |

### Software & Infrastructure

| No. | Item | Qty | Purpose |
|---|---|---|---|
| 13 | Laptop (Windows/Mac/Linux) | 1 | Runs Docker server — plate recognition + web dashboard |
| 14 | Wi-Fi Router | 1 | Local network connecting all devices |
