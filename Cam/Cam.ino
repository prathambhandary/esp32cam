#include "esp_camera.h"
#include "FS.h"
#include "SD_MMC.h"
#include <WiFi.h>
#include <WebServer.h>
#include "esp_system.h"
#include <HTTPClient.h> 
#include <time.h>

// ---------------- AI Thinker ESP32-CAM pin map ----------------
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ---------------- Wi-Fi ----------------
const char* ssid = "O2065";
const char* password = "oppo1234";

// ---------------- Static IP ----------------
IPAddress local_IP(192, 168, 43, 84);
IPAddress gateway(192, 168, 43, 1);
IPAddress subnet(255, 255, 255, 0);

// ---------------- Globals ----------------
WebServer server(80);

unsigned long lastCapture = 0;
const unsigned long interval = 20000;

unsigned long lastReset = 0;
const unsigned long RESET_INTERVAL = 30UL * 60UL * 1000UL;

// 🔥 NEW: latest image state
time_t latestEpoch = 0;
String latestName = "";

// ---------------- Camera ----------------
void setupCamera() {
  camera_config_t c;
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer   = LEDC_TIMER_0;
  c.pin_d0 = Y2_GPIO_NUM;
  c.pin_d1 = Y3_GPIO_NUM;
  c.pin_d2 = Y4_GPIO_NUM;
  c.pin_d3 = Y5_GPIO_NUM;
  c.pin_d4 = Y6_GPIO_NUM;
  c.pin_d5 = Y7_GPIO_NUM;
  c.pin_d6 = Y8_GPIO_NUM;
  c.pin_d7 = Y9_GPIO_NUM;
  c.pin_xclk = XCLK_GPIO_NUM;
  c.pin_pclk = PCLK_GPIO_NUM;
  c.pin_vsync = VSYNC_GPIO_NUM;
  c.pin_href = HREF_GPIO_NUM;
  c.pin_sccb_sda = SIOD_GPIO_NUM;
  c.pin_sccb_scl = SIOC_GPIO_NUM;
  c.pin_pwdn = PWDN_GPIO_NUM;
  c.pin_reset = RESET_GPIO_NUM;

  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_JPEG;
  c.frame_size = FRAMESIZE_VGA;
  c.jpeg_quality = 10;
  c.fb_count = 1;

  esp_camera_init(&c);
}

// ---------------- Capture ----------------
void takePhoto() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return;

  time_t now;
  time(&now);
  latestEpoch = now;

  struct tm *t = localtime(&now);

  char name[32];
  sprintf(name, "%04d%02d%02d_%02d%02d%02d.jpg",
          t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
          t->tm_hour, t->tm_min, t->tm_sec);

  // Save archive image
  File f = SD_MMC.open(String("/idle/") + name, FILE_WRITE);
  if (f) {
    f.write(fb->buf, fb->len);
    f.close();
  }

  // 🔥 Overwrite latest.jpg
  File latest = SD_MMC.open("/latest.jpg", FILE_WRITE);
  if (latest) {
    latest.write(fb->buf, fb->len);
    latest.close();
  }

  latestName = name;
  Serial.println(String("Captured: ") + name);

  esp_camera_fb_return(fb);
}

// ---------------- HTTP handlers ----------------
void handleLatestImage() {
  File f = SD_MMC.open("/latest.jpg");
  if (!f) {
    server.send(404, "text/plain", "No image yet");
    return;
  }
  server.streamFile(f, "image/jpeg");
  f.close();
}

void handleLastTimestamp() {
  String json = "{";
  json += "\"epoch\":" + String(latestEpoch) + ",";
  json += "\"name\":\"" + latestName + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

void handleImage() {
  String name = server.uri().substring(7); // "/image/"
  File f = SD_MMC.open("/idle/" + name);
  if (!f) {
    server.send(404, "text/plain", "Not found");
    return;
  }
  server.streamFile(f, "image/jpeg");
  f.close();
}


// (Optional) keep old endpoints if you still want them
void handleListIdle() {
  File root = SD_MMC.open("/idle");
  if (!root) {
    server.send(500, "text/plain", "SD error");
    return;
  }

  String out = "[";
  bool first = true;

  File f = root.openNextFile();
  while (f) {
    if (!first) out += ",";
    out += "\"" + String(f.name()) + "\"";
    first = false;
    f = root.openNextFile();
  }
  out += "]";
  server.send(200, "application/json", out);
}

void setupHTTP() {
  server.on("/latest.jpg", handleLatestImage);
  server.on("/last_timestamp", handleLastTimestamp);
  server.on("/list_idle", handleListIdle);

  server.onNotFound([]() {
    if (server.uri().startsWith("/image/")) {
      handleImage();
    } else {
      server.send(404, "text/plain", "Not found");
    }
  });

  server.begin();
}


// ---------------- Time sync ----------------
void syncTimeFromPhone() {
  HTTPClient http;
  http.begin("http://192.168.43.1:8090/time_now");
  int code = http.GET();
  if (code == 200) {
    String payload = http.getString();
    int i = payload.indexOf("epoch");
    if (i != -1) {
      long epoch = payload.substring(payload.indexOf(":", i) + 1).toInt();
      struct timeval tv;
      tv.tv_sec = epoch + (5 * 3600 + 30 * 60); // IST
      tv.tv_usec = 0;
      settimeofday(&tv, NULL);
    }
  }
  http.end();
}

// ---------------- Setup ----------------
void setup() {
  Serial.begin(115200);

  setupCamera();

  SD_MMC.begin();
  if (!SD_MMC.exists("/idle")) SD_MMC.mkdir("/idle");

  WiFi.config(local_IP, gateway, subnet);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) delay(300);

  setupHTTP();

  lastReset = millis();
  syncTimeFromPhone();
}

// ---------------- Loop ----------------
void loop() {
  if (millis() - lastCapture > interval) {
    lastCapture = millis();
    takePhoto();
  }

  if (millis() - lastReset > RESET_INTERVAL) {
    esp_restart();
  }

  server.handleClient();
}
