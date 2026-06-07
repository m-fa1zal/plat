/**
 * test_display.ino — ESP32 Dev Board (Display Test)
 *
 * Test only — no OLED, no servos, no PIR sensors.
 *
 * Flow:
 *   1. Connects to Wi-Fi and starts web server on port 80
 *   2. Prints IP address to Serial Monitor — enter this as DEVBOARD_IP in test_capture.ino
 *   3. Waits for HTTP POST /entry from XIAO (test_capture.ino)
 *   4. Prints plate number and verified status to Serial Monitor
 */

#include <WiFi.h>
#include <WebServer.h>

// ── Configuration — update before uploading ───────────────────
const char* WIFI_SSID     = "ssid";
const char* WIFI_PASSWORD = "password";
// ──────────────────────────────────────────────────────────────

WebServer server(80);

void setup() {
  Serial.begin(115200);
  Serial.println("\n[DISP-TEST] ESP32 Display Test starting...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[DISP-TEST] Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[DISP-TEST] Connected: " + WiFi.localIP().toString());
  Serial.println("[DISP-TEST] >>> Copy this IP into test_capture.ino as DEVBOARD_IP <<<");

  server.on("/entry", HTTP_POST, handleEntry);
  server.begin();
  Serial.println("[DISP-TEST] Web server ready on port 80\n");
}

void loop() {
  server.handleClient();
}

// ── Entry: receive plate + verified from XIAO ─────────────────
void handleEntry() {
  String plate  = server.arg("plate");
  bool verified = server.arg("verified") == "1";

  Serial.println("-----------------------------");
  Serial.println("[DISP-TEST] Plate  : " + plate);
  Serial.println("[DISP-TEST] Status : " + String(verified ? "VERIFIED — entry allowed" : "NOT VERIFIED — entry denied"));
  Serial.println("-----------------------------\n");

  server.send(200, "text/plain", "OK");
}
