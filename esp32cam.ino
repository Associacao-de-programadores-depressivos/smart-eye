#include <WiFi.h>
#include "esp_camera.h"

#define INTERVAL_MS 10000

// CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// Wifi secrets
const char *ssid = "ssid";
const char *password = "pass";

// Server specs
const String host = "127.0.0.1";
const String path = "/esp32cam/upload";
const int port = 8081;

WiFiClient client;
unsigned long previous_millis = 0;

void setup()
{
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.print("ESP32-CAM IP Address: ");
  Serial.println(WiFi.localIP());

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound())
  {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 10; //0-63 lower number means higher quality
    config.fb_count = 2;
  }
  else
  {
    config.frame_size = FRAMESIZE_CIF;
    config.jpeg_quality = 12; //0-63 lower number means higher quality
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK)
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }
}

void loop()
{
  unsigned long current_millis = millis();

  if (current_millis - previous_millis >= INTERVAL_MS)
  {
    upload_image();
    previous_millis = current_millis;
  }
}

void upload_image()
{
  camera_fb_t *fb = esp_camera_fb_get();

  if (!fb)
  {
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
    return;
  }

  Serial.println("Connecting to host: " + host);

  if (client.connect(host.c_str(), port))
  {
    Serial.println("Connected");

    String header = "--espimage\r\nContent-Disposition: form-data; name=\"image\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--espimage--\r\n";

    uint16_t total_len = fb->len + (header.length() + tail.length());

    client.println("POST " + path + " HTTP/1.1");
    client.println("Host: " + host);
    client.println("Content-Length: " + String(total_len));
    client.println("Content-Type: multipart/form-data; boundary=espimage");
    client.println();
    client.print(header);

    uint8_t *fb_buf = fb->buf;
    size_t fb_len = fb->len;

    for (size_t n = 0; n < fb_len; n = n + 1024)
    {
      if (n + 1024 < fb_len)
      {
        client.write(fb_buf, 1024);
        fb_buf += 1024;
      }
      else if (fb_len % 1024 > 0)
      {
        size_t rest = fb_len % 1024;
        client.write(fb_buf, rest);
      }
    }

    client.print(tail);
    esp_camera_fb_return(fb);
  }
  else
  {
    Serial.println("Failed to connect to host " + host);
  }
}