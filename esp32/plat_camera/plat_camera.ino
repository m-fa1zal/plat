/**
 * plat_camera.ino — XIAO ESP32-S3 Sense (Entry Camera Node)
 *
 * Flow:
 *   1. Wakes from deep sleep when entry flex sensor bends (GPIO1 HIGH)
 *   2. Connects to Wi-Fi
 *   3. Captures JPEG image from OV2640 camera
 *   4. POST image to laptop server → receives {plate, verified}
 *   5. POST result to ESP32 Dev Board → OLED + entry barrier
 *   6. Waits for flex sensor to release (car has passed)
 *   7. POST close signal to ESP32 Dev Board → barrier closes
 *   8. Returns to deep sleep
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include "esp_sleep.h"

// ── Configuration — update these before uploading ─────────────
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* SERVER_URL    = "http://192.168.1.10:8000/recognize"; // laptop IP
const char* DEVBOARD_IP   = "192.168.1.20";                       // ESP32 Dev Board IP

#define CAPTURE_COUNT       5     // number of frames to capture
#define CAPTURE_INTERVAL_MS 400   // milliseconds between each capture
// ──────────────────────────────────────────────────────────────

#define FLEX_PIN  GPIO_NUM_1   // Entry flex sensor — ext0 deep sleep wake-up

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
  delay(300);
  Serial.println("\n[CAM] Woke from deep sleep");

  connectWiFi();
  if (WiFi.status() != WL_CONNECTED) goToSleep();

  if (!initCamera()) {
    Serial.println("[CAM] Camera init failed");
    goToSleep();
  }

  String plate   = "";
  bool verified  = false;
  bool recognised = captureAndRecognise(plate, verified);

  if (recognised) {
    String body = "plate=" + plate + "&verified=" + (verified ? "1" : "0");
    postToDevBoard("/entry", body);
  }

  // Wait until car has fully passed (flex releases)
  Serial.println("[CAM] Waiting for car to pass...");
  while (digitalRead(FLEX_PIN) == HIGH) {
    delay(100);
  }
  Serial.println("[CAM] Car passed — sending close signal");
  postToDevBoard("/close-entry", "");

  goToSleep();
}

void loop() {}

// ── Wi-Fi ─────────────────────────────────────────────────────
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[CAM] Connecting to WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[CAM] Connected: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n[CAM] WiFi failed");
  }
}

// ── Camera ────────────────────────────────────────────────────
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
  cfg.frame_size   = FRAMESIZE_VGA;   // 640x480 — good balance of detail vs size
  cfg.jpeg_quality = 12;              // 0–63, lower = better quality
  cfg.fb_count     = 1;

  if (esp_camera_init(&cfg) != ESP_OK) {
    Serial.println("[CAM] Camera error");
    return false;
  }

  sensor_t* s = esp_camera_sensor_get();
  if (s) s->set_hmirror(s, 1);  // correct hardware mirror on XIAO ESP32-S3 Sense

  Serial.println("[CAM] Camera ready");
  return true;
}

// ── Capture Best Frame ────────────────────────────────────────
// Takes CAPTURE_COUNT frames at equal CAPTURE_INTERVAL_MS intervals.
// Returns the sharpest frame — larger JPEG size = more detail = sharper image.
camera_fb_t* getBestFrame() {
  // Discard 2 warm-up frames so exposure and white balance settle
  for (int i = 0; i < 2; i++) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb) esp_camera_fb_return(fb);
    delay(CAPTURE_INTERVAL_MS);
  }

  camera_fb_t* best    = nullptr;
  size_t       bestLen = 0;

  for (int i = 0; i < CAPTURE_COUNT; i++) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
      Serial.printf("[CAM] Frame %d/%d failed — skipping\n", i + 1, CAPTURE_COUNT);
      delay(CAPTURE_INTERVAL_MS);
      continue;
    }

    Serial.printf("[CAM] Frame %d/%d — %d bytes\n", i + 1, CAPTURE_COUNT, fb->len);

    if (fb->len > bestLen) {
      if (best) esp_camera_fb_return(best);  // free previous best
      best    = fb;
      bestLen = fb->len;
    } else {
      esp_camera_fb_return(fb);              // discard inferior frame
    }

    if (i < CAPTURE_COUNT - 1) delay(CAPTURE_INTERVAL_MS);
  }

  if (best) {
    Serial.printf("[CAM] Best frame selected: %d bytes\n", best->len);
  } else {
    Serial.println("[CAM] All frames failed");
  }
  return best;
}

// ── Capture + Recognise ───────────────────────────────────────
bool captureAndRecognise(String &plate, bool &verified) {
  camera_fb_t* fb = getBestFrame();
  if (!fb) {
    Serial.println("[CAM] No usable frame captured");
    return false;
  }

  HTTPClient http;
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "image/jpeg");
  http.setTimeout(20000);

  int code = http.POST((uint8_t*)fb->buf, fb->len);
  esp_camera_fb_return(fb);

  if (code != 200) {
    Serial.printf("[CAM] Server error: HTTP %d\n", code);
    http.end();
    return false;
  }

  String response = http.getString();
  http.end();
  Serial.println("[CAM] Server response: " + response);

  // Parse {"plate":"ABC1234","verified":true}
  plate    = extractJsonString(response, "plate");
  verified = response.indexOf("\"verified\":true") != -1;

  Serial.printf("[CAM] Plate: %s | Verified: %s\n",
                plate.c_str(), verified ? "YES" : "NO");
  return true;
}

// ── Post to Dev Board ─────────────────────────────────────────
void postToDevBoard(String path, String body) {
  String url = "http://" + String(DEVBOARD_IP) + path;
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.setTimeout(5000);
  int code = http.POST(body);
  http.end();
  Serial.printf("[CAM] Dev Board %s → HTTP %d\n", path.c_str(), code);
}

// ── Deep Sleep ────────────────────────────────────────────────
void goToSleep() {
  Serial.println("[CAM] Entering deep sleep — wake on flex HIGH (GPIO1)");
  esp_camera_deinit();
  WiFi.disconnect(true);
  delay(100);
  // Wake when flex sensor is pressed (GPIO1 goes HIGH)
  esp_sleep_enable_ext0_wakeup(FLEX_PIN, HIGH);
  esp_deep_sleep_start();
}

// ── JSON Helper ───────────────────────────────────────────────
String extractJsonString(String json, String key) {
  String search = "\"" + key + "\":\"";
  int start = json.indexOf(search);
  if (start == -1) return "";
  start += search.length();
  int end = json.indexOf("\"", start);
  if (end == -1) return "";
  return json.substring(start, end);
}
