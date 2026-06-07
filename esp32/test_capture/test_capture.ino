/**
 * test_capture.ino — XIAO ESP32-S3 Sense (Camera Test)
 *
 * Test only — no deep sleep, no Dev Board communication.
 *
 * Flow (repeats every TEST_INTERVAL_MS):
 *   1. Captures a JPEG image (2 warm-up frames discarded, 1 used)
 *   2. POST raw JPEG bytes to server /recognize
 *   3. Prints plate number and verified status to Serial Monitor
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"

// ── Configuration — update before uploading ───────────────────
const char* WIFI_SSID     = "payazal-2.4G";
const char* WIFI_PASSWORD = "P@y@z@l83";
const char* SERVER_URL    = "http://192.168.0.2:8000/recognize"; // laptop IP

#define TEST_INTERVAL_MS  10000   // ms between each capture+send cycle
// ──────────────────────────────────────────────────────────────

// XIAO ESP32-S3 Sense camera pin definitions
#define PWDN_GPIO_NUM   -1
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM   10
#define SIOD_GPIO_NUM   40
#define SIOC_GPIO_NUM   39
#define Y9_GPIO_NUM     48
#define Y8_GPIO_NUM     11
#define Y7_GPIO_NUM     12
#define Y6_GPIO_NUM     14
#define Y5_GPIO_NUM     16
#define Y4_GPIO_NUM     18
#define Y3_GPIO_NUM     17
#define Y2_GPIO_NUM     15
#define VSYNC_GPIO_NUM  38
#define HREF_GPIO_NUM   47
#define PCLK_GPIO_NUM   13

// ──────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=============================");
  Serial.println("  PLAT — Camera Test Sketch  ");
  Serial.println("=============================\n");

  connectWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[TEST] Cannot continue without Wi-Fi. Check SSID/password.");
    while (true) delay(1000);
  }

  if (!initCamera()) {
    Serial.println("[TEST] Camera init failed. Check board selection (XIAO_ESP32S3).");
    while (true) delay(1000);
  }

  Serial.println("[TEST] Ready. Starting capture loop...\n");
}

void loop() {
  Serial.println("-----------------------------");
  Serial.println("[TEST] Capturing image...");

  camera_fb_t* fb = captureFrame();
  if (!fb) {
    Serial.println("[TEST] Capture failed — retrying next cycle");
    delay(TEST_INTERVAL_MS);
    return;
  }

  Serial.printf("[TEST] Image captured: %d bytes\n", fb->len);
  Serial.println("[TEST] Sending to server...");

  sendAndPrint(fb);

  delay(TEST_INTERVAL_MS);
}

// ── Wi-Fi ─────────────────────────────────────────────────────
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[TEST] Connecting to Wi-Fi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[TEST] Connected: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n[TEST] Wi-Fi connection failed");
  }
}

// ── Camera init ───────────────────────────────────────────────
bool initCamera() {
  camera_config_t cfg;
  cfg.ledc_channel = LEDC_CHANNEL_0;
  cfg.ledc_timer   = LEDC_TIMER_0;
  cfg.pin_d0       = Y2_GPIO_NUM;
  cfg.pin_d1       = Y3_GPIO_NUM;
  cfg.pin_d2       = Y4_GPIO_NUM;
  cfg.pin_d3       = Y5_GPIO_NUM;
  cfg.pin_d4       = Y6_GPIO_NUM;
  cfg.pin_d5       = Y7_GPIO_NUM;
  cfg.pin_d6       = Y8_GPIO_NUM;
  cfg.pin_d7       = Y9_GPIO_NUM;
  cfg.pin_xclk     = XCLK_GPIO_NUM;
  cfg.pin_pclk     = PCLK_GPIO_NUM;
  cfg.pin_vsync    = VSYNC_GPIO_NUM;
  cfg.pin_href     = HREF_GPIO_NUM;
  cfg.pin_sccb_sda = SIOD_GPIO_NUM;
  cfg.pin_sccb_scl = SIOC_GPIO_NUM;
  cfg.pin_pwdn     = PWDN_GPIO_NUM;
  cfg.pin_reset    = RESET_GPIO_NUM;
  cfg.xclk_freq_hz = 20000000;
  cfg.pixel_format = PIXFORMAT_JPEG;
  cfg.frame_size   = FRAMESIZE_VGA;  // 640x480
  cfg.jpeg_quality = 12;             // 0–63, lower = better quality
  cfg.fb_count     = 1;

  if (esp_camera_init(&cfg) != ESP_OK) return false;
  Serial.println("[TEST] Camera ready");
  return true;
}

// ── Capture one frame (with 2 warm-up frames discarded) ───────
camera_fb_t* captureFrame() {
  // Discard 2 warm-up frames so exposure and white balance settle
  for (int i = 0; i < 2; i++) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb) esp_camera_fb_return(fb);
    delay(200);
  }
  return esp_camera_fb_get();
}

// ── Send JPEG to server, print result ─────────────────────────
void sendAndPrint(camera_fb_t* fb) {
  HTTPClient http;
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "image/jpeg");
  http.setTimeout(20000);

  int code = http.POST((uint8_t*)fb->buf, fb->len);
  esp_camera_fb_return(fb);

  if (code != 200) {
    Serial.printf("[TEST] Server error: HTTP %d\n", code);
    http.end();
    return;
  }

  String response = http.getString();
  http.end();

  Serial.println("[TEST] Raw response: " + response);

  String plate   = extractJsonString(response, "plate");
  bool verified  = response.indexOf("\"verified\":true") != -1;

  Serial.println("-----------------------------");
  if (plate.length() == 0) {
    Serial.println("[TEST] Result   : No plate detected");
  } else {
    Serial.println("[TEST] Plate    : " + plate);
    Serial.println("[TEST] Verified : " + String(verified ? "YES — entry allowed" : "NO  — entry denied"));
  }
  Serial.println("-----------------------------\n");
}

// ── JSON helper ───────────────────────────────────────────────
String extractJsonString(String json, String key) {
  String search = "\"" + key + "\":\"";
  int start = json.indexOf(search);
  if (start == -1) return "";
  start += search.length();
  int end = json.indexOf("\"", start);
  if (end == -1) return "";
  return json.substring(start, end);
}
