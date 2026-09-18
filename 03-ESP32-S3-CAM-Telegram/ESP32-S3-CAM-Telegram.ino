
#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>

// ============================================================
// ESP32-S3-EYE
// ============================================================

#define CAMERA_MODEL_ESP32S3_EYE
#include "camera_pins.h"

// ============================================================
// WIFI
// ============================================================

const char* WIFI_SSID = "";
const char* WIFI_PASSWORD = "";

// ============================================================
// TELEGRAM
// ============================================================

// IMPORTANT:
// Put your NEW token here after revoking the exposed token.
const char* BOT_TOKEN = "";

// Your Chat ID
const char* CHAT_ID = "";

// ============================================================
// SEND PHOTO TO TELEGRAM
// ============================================================

bool sendPhotoToTelegram(camera_fb_t* fb)
{
  if (fb == NULL)
  {
    Serial.println("ERROR: Camera frame is NULL");
    return false;
  }

  WiFiClientSecure client;

  // For testing, don't require certificate verification.
  // Once everything works, we can make this more secure.
  client.setInsecure();

  Serial.println("Connecting to Telegram...");

  if (!client.connect("api.telegram.org", 443))
  {
    Serial.println("ERROR: Could not connect to Telegram");
    return false;
  }

  String boundary = "----ESP32CameraBoundary";

  String head =
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" +
    String(CHAT_ID) +
    "\r\n"
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"photo\"; filename=\"esp32.jpg\"\r\n"
    "Content-Type: image/jpeg\r\n\r\n";

  String tail =
    "\r\n--" + boundary + "--\r\n";

  size_t contentLength =
    head.length() +
    fb->len +
    tail.length();

  String request =
    "POST /bot" + String(BOT_TOKEN) + "/sendPhoto HTTP/1.1\r\n"
    "Host: api.telegram.org\r\n"
    "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n"
    "Content-Length: " + String(contentLength) + "\r\n"
    "Connection: close\r\n"
    "\r\n";

  client.print(request);
  client.print(head);

  // Send JPEG data
  size_t written = client.write(
    fb->buf,
    fb->len
  );

  client.print(tail);

  Serial.printf(
    "JPEG size: %u bytes\n",
    fb->len
  );

  Serial.printf(
    "Bytes sent: %u\n",
    written
  );

  // Read Telegram response
  unsigned long timeout = millis();

  while (client.connected() &&
         millis() - timeout < 10000)
  {
    while (client.available())
    {
      String line = client.readStringUntil('\n');

      Serial.println(line);

      timeout = millis();
    }
  }

  client.stop();

  return true;
}

// ============================================================
// CAMERA INITIALIZATION
// ============================================================

bool initCamera()
{
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

  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;

  // JPEG camera output
  config.pixel_format = PIXFORMAT_JPEG;

  // Start with QVGA for stability
  config.frame_size = FRAMESIZE_QVGA;

  config.jpeg_quality = 12;

  if (psramFound())
  {
    Serial.println("PSRAM FOUND");

    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
  }
  else
  {
    Serial.println("WARNING: PSRAM NOT FOUND");

    config.fb_location = CAMERA_FB_IN_DRAM;
    config.fb_count = 1;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  }

  Serial.println("Initializing camera...");

  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK)
  {
    Serial.printf(
      "Camera initialization FAILED: 0x%x\n",
      err
    );

    return false;
  }

  Serial.println("Camera initialized!");

  sensor_t* sensor = esp_camera_sensor_get();

  if (sensor == NULL)
  {
    Serial.println("ERROR: Sensor is NULL");
    return false;
  }

  sensor->set_framesize(
    sensor,
    FRAMESIZE_QVGA
  );

  // ESP32-S3-EYE orientation
  sensor->set_vflip(
    sensor,
    1
  );

  Serial.println("Camera ready!");

  return true;
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);

  delay(2000);

  Serial.println();
  Serial.println("================================");
  Serial.println("ESP32-S3 TELEGRAM CAMERA");
  Serial.println("================================");

  // ----------------------------------------------------------
  // WiFi
  // ----------------------------------------------------------

  Serial.println("Connecting to WiFi...");

  WiFi.mode(WIFI_STA);

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);

    Serial.print(".");

    attempts++;

    if (attempts > 60)
    {
      Serial.println();
      Serial.println("WiFi connection failed!");

      while (true)
      {
        delay(1000);
      }
    }
  }

  Serial.println();
  Serial.println("WiFi connected!");

  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  // ----------------------------------------------------------
  // Camera
  // ----------------------------------------------------------

  if (!initCamera())
  {
    Serial.println("Camera failed.");

    while (true)
    {
      delay(1000);
    }
  }

  // ----------------------------------------------------------
  // Capture
  // ----------------------------------------------------------

  Serial.println();
  Serial.println("Capturing photo...");

  camera_fb_t* fb = esp_camera_fb_get();

  if (fb == NULL)
  {
    Serial.println("ERROR: Capture failed!");
    return;
  }

  Serial.printf(
    "Photo captured: %u bytes\n",
    fb->len
  );

  // ----------------------------------------------------------
  // Send to Telegram
  // ----------------------------------------------------------

  Serial.println("Sending photo to Telegram...");

  bool success = sendPhotoToTelegram(fb);

  // Return frame buffer
  esp_camera_fb_return(fb);

  if (success)
  {
    Serial.println();
    Serial.println("================================");
    Serial.println("PHOTO SENT TO TELEGRAM!");
    Serial.println("================================");
  }
  else
  {
    Serial.println();
    Serial.println("PHOTO SEND FAILED");
  }
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
  // Take another picture every 30 seconds.
  delay(30000);

  Serial.println();
  Serial.println("Capturing next photo...");

  camera_fb_t* fb = esp_camera_fb_get();

  if (fb == NULL)
  {
    Serial.println("Capture failed!");
    return;
  }

  Serial.printf(
    "Captured %u bytes\n",
    fb->len
  );

  sendPhotoToTelegram(fb);

  esp_camera_fb_return(fb);
}
