#include "esp_camera.h"
#include "FS.h"
#include "SD_MMC.h"
#include <WiFi.h>
#include <WebServer.h>

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
const char* ssid = "SEARCH FOR YOUR STUPID WIFI SSID";
const char* password = "PUT YOUR DAMN PASSWORD HERE";

// ---------------- Static IP ----------------
IPAddress local_IP(192, 168, 43, 84);
IPAddress gateway(192, 168, 43, 1);
IPAddress subnet(255, 255, 255, 0);

// ---------------- Globals ----------------
WebServer server(80);
unsigned long lastCapture = 0;
const unsigned long interval = 30000; // 30 sec
int photoCount = 0;

// ---------------- Counter persistence ----------------
void saveLastPhotoName(const String &name) {
  File f = SD_MMC.open("/last.txt", FILE_WRITE);
  if (f) {
    f.print(name);
    f.close();
  }
}

String readLastPhotoName() {
  if (!SD_MMC.exists("/last.txt")) return "";
  File f = SD_MMC.open("/last.txt");
  if (!f) return "";
  String name = f.readString();
  f.close();
  name.trim();
  return name;
}

void loadCounterFromFile() {
  String last = readLastPhotoName(); // idle_0012.jpg
  if (last.startsWith("idle_")) {
    int n = last.substring(5, 9).toInt();
    photoCount = n + 1;
  }
}

// ---------------- HTTP ----------------
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
    out += "\"" + String(f.name()).substring(6) + "\""; // remove /idle/
    first = false;
    f = root.openNextFile();
  }

  out += "]";
  server.send(200, "application/json", out);
}

void handleLast() {
  server.send(200, "text/plain", readLastPhotoName());
}

void handleImage() {
  String name = server.uri().substring(7);
  File f = SD_MMC.open("/idle/" + name);
  if (!f) {
    server.send(404, "text/plain", "Not found");
    return;
  }
  server.streamFile(f, "image/jpeg");
  f.close();
}

void setupHTTP() {
  server.on("/list_idle", handleListIdle);
  server.on("/last", handleLast);

  server.onNotFound([]() {
    if (server.uri().startsWith("/image/")) {
      handleImage();
    } else {
      server.send(404, "text/plain", "Not found");
    }
  });

  server.begin();
}

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

  c.frame_size = FRAMESIZE_VGA;  // 800x600
  c.jpeg_quality = 10;            // higher quality
  c.fb_count = 1;

  esp_camera_init(&c);
}

// ---------------- Capture ----------------
void takePhoto() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return;

  char name[32];
  sprintf(name, "/idle/idle_%04d.jpg", photoCount++);

  File f = SD_MMC.open(name, FILE_WRITE);
  if (f) {
    f.write(fb->buf, fb->len);
    f.close();
    saveLastPhotoName(String(name).substring(6));
    Serial.println(name);
  }

  esp_camera_fb_return(fb);
}

// ---------------- Setup ----------------
void setup() {
  Serial.begin(115200);

  setupCamera();

  SD_MMC.begin();
  if (!SD_MMC.exists("/idle")) SD_MMC.mkdir("/idle");

  loadCounterFromFile();

  WiFi.config(local_IP, gateway, subnet);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(300);

  setupHTTP();
}

// ---------------- Loop ----------------
void loop() {
  if (millis() - lastCapture > interval) {
    lastCapture = millis();
    takePhoto();
  }
  server.handleClient();
}
