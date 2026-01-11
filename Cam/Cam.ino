#include "esp_camera.h"
#include "FS.h"
#include "SD_MMC.h"
#include <WiFi.h>
#include <WebServer.h>
#include "esp_system.h"   // for esp_restart()
#include <HTTPClient.h> 
#include <time.h>
// ---------------- Flash ----------------
#define FLASH_GPIO 4  // AI Thinker default flash LED

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
const unsigned long interval = 30000; // 30 sec
unsigned long lastReset = 0;
const unsigned long RESET_INTERVAL = 30UL * 60UL * 1000UL; // 30 minutes

// ---------------- HTTP ----------------
void handleListIdle() {
  String after = server.hasArg("after") ? server.arg("after") : "";

  File root = SD_MMC.open("/idle");
  if (!root) {
    server.send(500, "text/plain", "SD error");
    return;
  }

  String out = "[";
  bool first = true;

  File f = root.openNextFile();
  while (f) {
    String name = String(f.name());
    int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);

    if (after == "" || name > after) {
      if (!first) out += ",";
      out += "\"" + name + "\"";
      first = false;
    }

    f = root.openNextFile();
  }

  out += "]";
  server.send(200, "application/json", out);
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
  struct tm *t = localtime(&now);

  char name[32];
  sprintf(name, "/idle/%04d%02d%02d_%02d%02d%02d.jpg",
          t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
          t->tm_hour, t->tm_min, t->tm_sec);

  File f = SD_MMC.open(name, FILE_WRITE);
  if (f) {
    f.write(fb->buf, fb->len);
    f.close();
    Serial.println(name);
  }

  esp_camera_fb_return(fb);
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
      long timezoneOffset = 5*3600 + 30*60; // IST = UTC+5:30
      tv.tv_sec = epoch + timezoneOffset;
      tv.tv_usec = 0;
      settimeofday(&tv, NULL);
      Serial.println("Time synced from phone");
    }
  } else {
    Serial.println("Time sync failed");
  }
  http.end();
}

void printTime() {
  time_t now;
  time(&now);
  Serial.println(ctime(&now));
}

// ---------------- Setup ----------------
void setup() {
  Serial.begin(115200);

  pinMode(FLASH_GPIO, OUTPUT);
  digitalWrite(FLASH_GPIO, LOW);

  setupCamera();

  SD_MMC.begin();
  if (!SD_MMC.exists("/idle")) SD_MMC.mkdir("/idle");

  WiFi.config(local_IP, gateway, subnet);
  WiFi.begin(ssid, password);

  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) { 
    delay(300);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
  Serial.println(WiFi.localIP());

  setupHTTP();

  lastReset = millis();
  syncTimeFromPhone();
  printTime();
}

// ---------------- Loop ----------------
void loop() {
  if (millis() - lastCapture > interval) {
    lastCapture = millis();
    takePhoto();
  }

  if (millis() - lastReset > RESET_INTERVAL) {
    Serial.println("Auto-resetting ESP32-CAM...");
    delay(500);  
    esp_restart();
  }

  server.handleClient();
}
