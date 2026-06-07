/**
 * plat_display.ino — ESP32 Dev Board (Control Node)
 *
 * Responsibilities:
 *   - Receives plate + status from XIAO via HTTP POST /entry
 *   - Displays plate number and status on OLED 1
 *   - Opens entry barrier (Servo 1) if verified
 *   - Receives close signal via HTTP POST /close-entry
 *   - Closes entry barrier (Servo 1)
 *   - Monitors 3 PIR sensors for parking lot availability
 *   - Displays parking availability on OLED 2
 *   - Reads exit flex sensor — opens/closes exit barrier (Servo 2)
 *
 * Libraries required (install via Arduino Library Manager):
 *   - Adafruit SSD1306
 *   - Adafruit GFX Library
 *   - ESP32Servo
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>

// ── Configuration — update these before uploading ─────────────
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
// ──────────────────────────────────────────────────────────────

// ── Pin Definitions ───────────────────────────────────────────
#define SERVO_ENTRY_PIN   13
#define SERVO_EXIT_PIN    14
#define PIR_LOT1_PIN      25
#define PIR_LOT2_PIN      26
#define PIR_LOT3_PIN      27
#define FLEX_EXIT_PIN     34   // Input-only pin — analog read

// ── OLED Settings ─────────────────────────────────────────────
#define OLED_WIDTH   128
#define OLED_HEIGHT   64
#define OLED1_ADDR   0x3C   // Plate status display
#define OLED2_ADDR   0x3D   // Parking availability display

// Threshold for exit flex sensor analog read (0–4095)
// Adjust based on your flex sensor and circuit values
#define FLEX_EXIT_THRESHOLD  1500

// ──────────────────────────────────────────────────────────────

Adafruit_SSD1306 oled1(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
Adafruit_SSD1306 oled2(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

Servo servoEntry;
Servo servoExit;
WebServer server(80);

bool exitFlexPressed = false;

// Track last PIR state to avoid unnecessary OLED redraws
bool lastLot[3] = {false, false, false};

// ──────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  Serial.println("\n[DISPLAY] ESP32 Dev Board starting...");

  // Servos — start closed
  servoEntry.attach(SERVO_ENTRY_PIN);
  servoExit.attach(SERVO_EXIT_PIN);
  servoEntry.write(0);
  servoExit.write(0);

  // PIR sensors
  pinMode(PIR_LOT1_PIN, INPUT);
  pinMode(PIR_LOT2_PIN, INPUT);
  pinMode(PIR_LOT3_PIN, INPUT);

  // OLEDs
  Wire.begin();
  if (!oled1.begin(SSD1306_SWITCHCAPVCC, OLED1_ADDR)) {
    Serial.println("[DISPLAY] OLED 1 not found — check wiring, confirm address 0x3C");
  }
  if (!oled2.begin(SSD1306_SWITCHCAPVCC, OLED2_ADDR)) {
    Serial.println("[DISPLAY] OLED 2 not found — check A0 jumper soldered for address 0x3D");
  }

  showStandby();
  updateParkingOLED(true);

  // Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[DISPLAY] Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[DISPLAY] Connected: " + WiFi.localIP().toString());
  Serial.println("[DISPLAY] >>> Copy this IP into plat_camera.ino as DEVBOARD_IP <<<");

  // Web server routes
  server.on("/entry",       HTTP_POST, handleEntry);
  server.on("/close-entry", HTTP_POST, handleCloseEntry);
  server.begin();
  Serial.println("[DISPLAY] Web server ready on port 80");
}

void loop() {
  server.handleClient();
  handleExitBarrier();
  updateParkingOLED(false);
}

// ── Entry: receive plate + verified from XIAO ─────────────────
void handleEntry() {
  String plate  = server.arg("plate");
  bool verified = server.arg("verified") == "1";

  Serial.printf("[DISPLAY] Entry — plate: %s | verified: %s\n",
                plate.c_str(), verified ? "YES" : "NO");

  showPlateResult(plate, verified);

  if (verified) {
    servoEntry.write(90);
    Serial.println("[DISPLAY] Entry barrier OPEN (90deg)");
  }

  server.send(200, "text/plain", "OK");
}

// ── Close entry barrier ───────────────────────────────────────
void handleCloseEntry() {
  servoEntry.write(0);
  Serial.println("[DISPLAY] Entry barrier CLOSED (0deg)");
  showStandby();
  server.send(200, "text/plain", "OK");
}

// ── Exit barrier — flex sensor controls open/close ────────────
void handleExitBarrier() {
  int flexVal = analogRead(FLEX_EXIT_PIN);
  bool pressed = flexVal > FLEX_EXIT_THRESHOLD;

  if (pressed && !exitFlexPressed) {
    servoExit.write(90);
    exitFlexPressed = true;
    Serial.printf("[DISPLAY] Exit barrier OPEN (90deg) — flex val: %d\n", flexVal);
  }

  if (!pressed && exitFlexPressed) {
    servoExit.write(0);
    exitFlexPressed = false;
    Serial.printf("[DISPLAY] Exit barrier CLOSED (0deg) — flex val: %d\n", flexVal);
  }
}

// ── OLED 1 — Plate Status ─────────────────────────────────────
void showStandby() {
  oled1.clearDisplay();
  oled1.setTextColor(SSD1306_WHITE);
  oled1.setTextSize(1);
  oled1.setCursor(24, 18);
  oled1.println("PLAT System");
  oled1.setCursor(30, 34);
  oled1.println("Ready...");
  oled1.display();
}

void showPlateResult(String plate, bool verified) {
  oled1.clearDisplay();
  oled1.setTextColor(SSD1306_WHITE);

  // Plate number — large text, centred
  oled1.setTextSize(2);
  String display = plate.isEmpty() ? "------" : plate;
  int16_t x = max(0, (OLED_WIDTH - (int16_t)(display.length() * 12)) / 2);
  oled1.setCursor(x, 6);
  oled1.print(display);

  // Divider line
  oled1.drawFastHLine(0, 30, OLED_WIDTH, SSD1306_WHITE);

  // Status
  oled1.setTextSize(1);
  if (verified) {
    oled1.setCursor(26, 40);
    oled1.println(">> VERIFIED <<");
  } else {
    oled1.setCursor(12, 40);
    oled1.println(">> NOT VERIFIED <<");
  }
  oled1.display();
}

// ── OLED 2 — Parking Availability ────────────────────────────
void updateParkingOLED(bool force) {
  // PIR HIGH = car detected = lot FULL
  bool lot[3] = {
    digitalRead(PIR_LOT1_PIN) == HIGH,
    digitalRead(PIR_LOT2_PIN) == HIGH,
    digitalRead(PIR_LOT3_PIN) == HIGH
  };

  // Only redraw when state changes (or forced on startup)
  if (!force && lot[0] == lastLot[0] && lot[1] == lastLot[1] && lot[2] == lastLot[2]) return;

  lastLot[0] = lot[0];
  lastLot[1] = lot[1];
  lastLot[2] = lot[2];

  oled2.clearDisplay();
  oled2.setTextColor(SSD1306_WHITE);

  oled2.setTextSize(1);
  oled2.setCursor(20, 0);
  oled2.println("- PARKING -");
  oled2.drawFastHLine(0, 10, OLED_WIDTH, SSD1306_WHITE);

  const char* labels[3] = {"Lot 1", "Lot 2", "Lot 3"};
  for (int i = 0; i < 3; i++) {
    oled2.setCursor(0, 16 + i * 16);
    oled2.print(labels[i]);
    oled2.print(": ");
    oled2.println(lot[i] ? "FULL" : "FREE");
  }

  oled2.display();
}
