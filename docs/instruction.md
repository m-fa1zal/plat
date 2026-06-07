# PLAT System — Step-by-Step Setup Guide

This guide allows anyone to duplicate this project from scratch.

---

## Requirements

### Hardware
- Seeed Studio XIAO ESP32-S3 Sense (entry camera node)
- ESP32 Dev Board (control node)
- 2x Flex sensor (entry detection + exit detection)
- 2x Servo motor (entry barrier + exit barrier)
- 2x OLED display I2C 128x64 (plate status display + parking availability display)
- 3x PIR sensor (one per parking lot)
- USB-C cable (for flashing XIAO ESP32-S3 Sense)
- USB cable (for flashing ESP32 Dev Board)
- Laptop (Windows/Mac/Linux) with Wi-Fi

### Software
- [Arduino IDE](https://www.arduino.cc/en/software) (version 2.x recommended)
- [Docker Desktop](https://www.docker.com/products/docker-desktop/) — runs the server (no Python install needed)
- [Git](https://git-scm.com/downloads) — for cloning the repository

---

## Part 1 — GitHub Setup

### Step 1: Install Git
Download and install from https://git-scm.com/downloads

### Step 2: Clone the repository
Open Command Prompt and run:
```bash
git clone https://github.com/YOUR_USERNAME/plat.git
cd plat
```

### Step 3: Basic Git workflow
```bash
git add .
git commit -m "your message"
git push
```

---

## Part 2 — Laptop Server Setup (Docker)

### Step 1: Install Docker Desktop
1. Download from https://www.docker.com/products/docker-desktop/
2. Install and launch Docker Desktop
3. Verify installation:
   ```
   docker --version
   ```

### Step 2: Find your laptop's local IP address
Open Command Prompt:
```
ipconfig
```
Look for **IPv4 Address** under your Wi-Fi adapter, e.g., `192.168.1.10`

> This IP goes into both ESP32 firmware files.

### Step 3: Start the server
From the project root folder:
```bash
docker compose up
```
- First run downloads dependencies and EasyOCR models (~500MB) — this takes a few minutes
- Subsequent runs start instantly
- SQLite database is stored in a Docker volume — data persists across restarts

### Step 4: Stop the server
```bash
docker compose down
```

### Step 5: Allow port 8000 through Windows Firewall
1. Open **Windows Defender Firewall > Advanced Settings**
2. **Inbound Rules > New Rule > Port > TCP > 8000 > Allow**

---

## Part 3 — XIAO ESP32-S3 Sense Setup (Entry Camera Node)

### Step 1: Install Arduino IDE
Download from https://www.arduino.cc/en/software

### Step 2: Add ESP32 board support
1. **File > Preferences** → add to "Additional Board Manager URLs":
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
2. **Tools > Board > Boards Manager** → search `esp32` → install **esp32 by Espressif Systems**

### Step 3: Select board
- **Tools > Board > esp32 > XIAO_ESP32S3**

### Step 4: Wire the entry flex sensor

| Flex Sensor Pin | XIAO ESP32-S3 Pin |
|---|---|
| One end | 3.3V |
| Other end | GPIO1 (+ 10kΩ resistor to GND) |

GPIO1 is configured as the **ext0 wake-up pin** from deep sleep.

### Step 5: Configure firmware
Open `esp32/plat_camera/plat_camera.ino` and update:
```cpp
const char* WIFI_SSID   = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* SERVER_URL  = "http://192.168.1.10:8000/recognize"; // your laptop IP
const char* DEVBOARD_IP = "192.168.1.20";  // get this from ESP32 Dev Board Serial Monitor
```

### Step 6: Upload firmware
1. Connect XIAO ESP32-S3 Sense via USB-C
2. Select correct **Port** under Tools
3. Click **Upload**

---

## Part 4 — ESP32 Dev Board Setup (Control Node)

### Step 1: Select board
- **Tools > Board > esp32 > ESP32 Dev Module**

### Step 2: Install required Arduino libraries
Go to **Tools > Manage Libraries** and install:
- `Adafruit SSD1306`
- `Adafruit GFX Library`
- `ESP32Servo`

### Step 3: Wire all components

**OLED 1 — Plate Status Display (I2C address: 0x3C)**
| OLED Pin | ESP32 Pin |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SCL | GPIO22 |
| SDA | GPIO21 |

**OLED 2 — Parking Availability Display (I2C address: 0x3D)**
| OLED Pin | ESP32 Pin |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SCL | GPIO22 |
| SDA | GPIO21 |

> Both OLEDs share the same I2C bus but use different I2C addresses (0x3C and 0x3D).

**Servo 1 — Entry Barrier**
| Servo Pin | ESP32 Pin |
|---|---|
| Signal | GPIO13 |
| VCC | 5V (external or VIN) |
| GND | GND |

**Servo 2 — Exit Barrier**
| Servo Pin | ESP32 Pin |
|---|---|
| Signal | GPIO14 |
| VCC | 5V (external or VIN) |
| GND | GND |

**3x PIR Sensors — Parking Lot Monitoring**
| PIR Sensor | ESP32 Pin |
|---|---|
| PIR 1 (Lot 1) | GPIO25 |
| PIR 2 (Lot 2) | GPIO26 |
| PIR 3 (Lot 3) | GPIO27 |
| VCC (all) | 3.3V |
| GND (all) | GND |

**Exit Flex Sensor**
| Flex Sensor Pin | ESP32 Pin |
|---|---|
| One end | 3.3V |
| Other end | GPIO34 (+ 10kΩ resistor to GND) |

### Step 4: Configure firmware
Open `esp32/plat_display/plat_display.ino` and update:
```cpp
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
```

### Step 5: Upload firmware
1. Connect ESP32 Dev Board via USB
2. Select correct **Port**
3. Click **Upload**

---

## Part 5 — Wi-Fi Network

- Connect **laptop**, **XIAO ESP32-S3 Sense**, and **ESP32 Dev Board** to the **same Wi-Fi network**
- After flashing `plat_display.ino`, open Serial Monitor (baud: 115200) — the Dev Board prints its IP:
  ```
  [DISPLAY] >>> Copy this IP into plat_camera.ino as DEVBOARD_IP <<<
  ```
- Copy that IP into `DEVBOARD_IP` in `plat_camera.ino` before flashing the XIAO

---

## Part 6 — Testing the System

### Test server and web dashboard
Open browser:
```
http://localhost:8000
```
You will be redirected to the login page. Log in with:
- **Username:** `admin`
- **Password:** `admin@1234`

You should see the car registry dashboard. Try adding a car with all fields (matrix, name, plate, manufacturer, model, colour), editing it, and deleting it to confirm everything works.

To view FastAPI auto docs:
```
http://localhost:8000/docs
```

### Test entry flow
1. Open Serial Monitor for XIAO ESP32-S3 (baud: 115200)
2. Bend the entry flex sensor to simulate a vehicle
3. Confirm in Serial Monitor:
   - `[CAM] Woke from deep sleep`
   - `[CAM] Connected: 192.168.x.x`
   - `[CAM] Frame 1/5 — XXXX bytes` (5 lines)
   - `[CAM] Best frame selected: XXXX bytes`
   - `[CAM] Server response: {"plate":"...","verified":...}`
   - `[CAM] Dev Board /entry → HTTP 200`
4. Check OLED 1 on ESP32 Dev Board — plate number and status displayed
5. If plate is verified → Servo 1 rotates to 90° (barrier opens)
6. Release the entry flex sensor to simulate car passing → Servo 1 returns to 0° (barrier closes)

### Test parking availability
1. Wave hand in front of each PIR sensor
2. Check OLED 2 updates correctly for each lot

### Test exit flow
1. Bend the exit flex sensor to simulate a vehicle at exit
2. Servo 2 should rotate to 90° (exit barrier opens)
3. Release the exit flex sensor to simulate car passing → Servo 2 returns to 0° (exit barrier closes)

### Full expected flow
```
Entry: vehicle presses flex sensor → XIAO ESP32-S3 wakes from deep sleep →
       captures 5 frames (400ms apart) → selects sharpest frame →
       sends JPEG to laptop OCR → whitelist check →
       XIAO forwards plate + status to ESP32 Dev Board →
       OLED 1 shows result → if verified: Servo 1 opens (0°→90°) →
       vehicle passes, flex sensor released → XIAO sends close signal →
       Servo 1 closes (90°→0°) → XIAO returns to deep sleep

Parking: PIR 1/2/3 continuously detect cars → OLED 2 updates availability

Exit: vehicle presses exit flex sensor → Servo 2 opens (0°→90°) →
      vehicle passes, flex sensor released → Servo 2 closes (90°→0°)
```

---

## Part 7 — Project Folder Structure

```
plat/
├── server/
│   ├── main.py              # FastAPI — OCR, whitelist check, web dashboard routes, login auth
│   ├── database.py          # SQLite setup — cars + users tables, CRUD, password hashing
│   ├── requirements.txt     # Python dependencies for Docker
│   └── templates/
│       ├── login.html       # Login page
│       └── index.html       # Car registry dashboard — view, add, edit, delete cars
├── esp32/
│   ├── plat_camera/
│   │   └── plat_camera.ino  # XIAO ESP32-S3 Sense — deep sleep, 5-frame capture, send to server
│   └── plat_display/
│       └── plat_display.ino # ESP32 Dev Board — OLED x2, servo x2, PIR x3, exit flex sensor
├── docs/
│   ├── setup.md             # Project description and architecture
│   ├── instruction.md       # This file
│   └── connection.md        # Full pin wiring for both boards
├── Dockerfile               # Docker image for the FastAPI server
├── docker-compose.yml       # Starts server, mounts SQLite volume
├── .gitignore               # Excludes cars.db, __pycache__, .env
└── CLAUDE.md                # AI assistant guidance
```

---

## Troubleshooting

| Problem | Solution |
|---|---|
| Dashboard shows 500 error on first load | Starlette version mismatch — confirm `TemplateResponse` uses `request=request, name=` syntax |
| Can't log in / login page shows error | Default credentials are `admin` / `admin@1234` — rebuild with `docker compose down -v && docker compose up --build` to recreate the database if it was created before login was added |
| `bcrypt` error on startup | Ensure `requirements.txt` uses `bcrypt` not `passlib[bcrypt]` — rebuild the Docker image |
| `docker compose up` fails | Make sure Docker Desktop is running before the command |
| Port 8000 already in use | Stop other services using port 8000 or change port in `docker-compose.yml` |
| EasyOCR models not downloading | Check internet connection on first run; models are ~500MB |
| XIAO ESP32-S3 won't wake from deep sleep | Check entry flex sensor wiring and 10kΩ resistor |
| ESP32 can't connect to Wi-Fi | Double-check SSID and password in firmware |
| Server not reachable | Ensure all devices on same Wi-Fi, check firewall port 8000 |
| OLED shows nothing | Check I2C wiring; confirm address (0x3C or 0x3D) with I2C scanner sketch |
| Both OLEDs show same content | Confirm they have different I2C addresses (solder A0 jumper on one) |
| Servo not moving | Servo needs 5V — do not power from 3.3V pin |
| PIR sensor always triggered | Allow 30–60s warm-up time after powering on |
| Exit barrier won't open | Check exit flex sensor wiring on GPIO34 |
| EasyOCR not reading plate | Improve lighting; ensure plate fully visible and not blurry |
| All 5 frames are small / blurry | Camera exposure needs more time — increase `CAPTURE_INTERVAL_MS` in firmware |
| XIAO can't reach Dev Board | Confirm `DEVBOARD_IP` in `plat_camera.ino` matches Dev Board's Serial Monitor output |
