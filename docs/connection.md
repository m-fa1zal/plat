# Connection List

---

# XIAO ESP32-S3 Sense (Camera Node — Entry)

All sensors connected to the XIAO ESP32-S3 Sense.

---

## Pin Summary

| GPIO | Board Label | Type | Connected To |
|---|---|---|---|
| GPIO1 | D0 | Analog Input / RTC Wake | Entry Flex Sensor |
| 3.3V | 3V3 | Power | Flex Sensor |
| GND | GND | Ground | Flex Sensor, Resistor |
| — | Built-in | Camera | OV3640 (no wiring needed) |

---

## Detailed Wiring

### Entry Flex Sensor

Used to detect a vehicle at the entry lane and wake the ESP32-S3 from deep sleep via **ext0 wake-up** on GPIO1.

| Connection | XIAO ESP32-S3 Sense Pin |
|---|---|
| One end of flex sensor | 3.3V |
| Other end of flex sensor | D0 (GPIO1) |
| 10kΩ resistor (between D0 and GND) | GND |

```
3.3V ──── [Flex Sensor] ──── D0 (GPIO1)
                                  │
                              [10kΩ]
                                  │
                                GND
```

> GPIO1 is an RTC-capable GPIO, required for ext0 deep sleep wake-up. When the flex sensor bends under a vehicle, voltage on GPIO1 rises above the threshold and wakes the ESP32-S3.

---

### Built-in Camera (OV3640)

No external wiring required. The OV3640 camera is integrated onto the XIAO ESP32-S3 Sense module and accessed directly in firmware via the ESP32 Camera library.

---

## Power

| Pin | Purpose |
|---|---|
| USB-C | Programming and main power supply |
| 3.3V | Powers the flex sensor circuit |
| GND | Common ground |

---

## Notes

- GPIO1 (D0) must be used for ext0 wake-up — not all GPIO pins support RTC wake-up
- The 10kΩ pull-down resistor ensures GPIO1 stays LOW when no vehicle is present (no false wake-up)
- Deep sleep current on XIAO ESP32-S3 is ~14µA — effectively zero power until triggered

---
---

# ESP32 Dev Board (Control Node)

All sensors and actuators connected to the ESP32 Dev Board (Control Node).

---

## Pin Summary

| GPIO | Type | Connected To |
|---|---|---|
| GPIO13 | PWM Output | Servo 1 — Entry Barrier (Signal) |
| GPIO14 | PWM Output | Servo 2 — Exit Barrier (Signal) |
| GPIO21 | I2C SDA | OLED 1 + OLED 2 (shared bus) |
| GPIO22 | I2C SCL | OLED 1 + OLED 2 (shared bus) |
| GPIO25 | Digital Input | PIR Sensor 1 — Parking Lot 1 |
| GPIO26 | Digital Input | PIR Sensor 2 — Parking Lot 2 |
| GPIO27 | Digital Input | PIR Sensor 3 — Parking Lot 3 |
| GPIO34 | Analog Input | Exit Flex Sensor |
| 3.3V | Power | OLED 1, OLED 2, PIR x3, Flex Sensor |
| 5V (VIN) | Power | Servo 1, Servo 2 |
| GND | Ground | All components |

---

## Detailed Wiring

### Servo 1 — Entry Barrier

| Servo Wire | ESP32 Dev Board Pin |
|---|---|
| Signal (Orange) | GPIO13 |
| VCC (Red) | 5V (VIN) |
| GND (Brown) | GND |

> Servo must be powered from 5V, not 3.3V. Rotates from 0° (closed) to 90° (open) when plate is verified. Returns to 0° (closed) once the entry flex sensor is released — car has passed through.

---

### Servo 2 — Exit Barrier

| Servo Wire | ESP32 Dev Board Pin |
|---|---|
| Signal (Orange) | GPIO14 |
| VCC (Red) | 5V (VIN) |
| GND (Brown) | GND |

> Rotates from 0° (closed) to 90° (open) when exit flex sensor is triggered. Returns to 0° (closed) once the exit flex sensor is released — car has passed through.

---

### OLED 1 — Plate Number & Status Display

I2C Address: **0x3C**

| OLED Pin | ESP32 Dev Board Pin |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SCL | GPIO22 |
| SDA | GPIO21 |

> Displays the recognised plate number and verification status (Verified / Not Verified).

---

### OLED 2 — Parking Availability Display

I2C Address: **0x3D**

| OLED Pin | ESP32 Dev Board Pin |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SCL | GPIO22 |
| SDA | GPIO21 |

> Shares the same I2C bus as OLED 1. Must have a different I2C address — solder the **A0 jumper** on the back of the OLED module to change address from 0x3C to 0x3D.

---

### PIR Sensor 1 — Parking Lot 1 (HC-SR501)

| PIR Pin | ESP32 Dev Board Pin |
|---|---|
| VCC | 3.3V |
| OUT | GPIO25 |
| GND | GND |

---

### PIR Sensor 2 — Parking Lot 2 (HC-SR501)

| PIR Pin | ESP32 Dev Board Pin |
|---|---|
| VCC | 3.3V |
| OUT | GPIO26 |
| GND | GND |

---

### PIR Sensor 3 — Parking Lot 3 (HC-SR501)

| PIR Pin | ESP32 Dev Board Pin |
|---|---|
| VCC | 3.3V |
| OUT | GPIO27 |
| GND | GND |

> Allow 30–60 seconds warm-up time after powering on before PIR sensors give accurate readings.

---

### Exit Flex Sensor

| Connection | ESP32 Dev Board Pin |
|---|---|
| One end of flex sensor | 3.3V |
| Other end of flex sensor | GPIO34 |
| 10kΩ resistor (between GPIO34 and GND) | GND |

> GPIO34 is input-only — suitable for analog reading. When a vehicle presses the flex sensor, resistance drops and voltage on GPIO34 changes, triggering the exit barrier.

---

## Power Distribution

| Rail | Supplies |
|---|---|
| 3.3V | OLED 1, OLED 2, PIR Sensor x3, Flex Sensor |
| 5V (VIN) | Servo 1 (Entry), Servo 2 (Exit) |
| GND | All components (common ground) |

> Use an external 5V power supply for servos if both run simultaneously — drawing too much current from VIN can cause the ESP32 to reset.

---

## I2C Bus Diagram

```
ESP32 GPIO21 (SDA) ──┬── OLED 1 SDA (0x3C)
                     └── OLED 2 SDA (0x3D)

ESP32 GPIO22 (SCL) ──┬── OLED 1 SCL (0x3C)
                     └── OLED 2 SCL (0x3D)
```

---

## Notes

- Do not power servos from the 3.3V pin — they require 5V and draw high current
- GPIO34, 35, 36, 39 on ESP32 Dev Board are **input-only** — do not use them as outputs
- All GND pins must share a common ground, including the external 5V supply for servos
