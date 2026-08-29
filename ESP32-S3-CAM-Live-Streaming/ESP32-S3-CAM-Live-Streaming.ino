#include "esp_camera.h"
#include <WiFi.h>

// ============================================================
// CAMERA MODEL
// ============================================================

#define CAMERA_MODEL_ESP32S3_EYE

#include "camera_pins.h"

// ============================================================
// WIFI
// ============================================================

const char* ssid = " ";
const char* password = " ";

// ============================================================
// CAMERA SERVER
// ============================================================

void startCameraServer();

// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);

  delay(2000);

  Serial.println();
  Serial.println("================================");
  Serial.println("ESP32-S3-EYE LIVE CAMERA");
  Serial.println("================================");

  // ==========================================================
  // CAMERA CONFIGURATION
  // ==========================================================

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

  // JPEG is best for WiFi streaming
  config.pixel_format = PIXFORMAT_JPEG;

  // ==========================================================
  // STREAM SETTINGS
  // ==========================================================

  // QVGA = 320 x 240
  // Good balance between quality and latency
  config.frame_size = FRAMESIZE_QVGA;

  // Higher number = smaller JPEG = faster streaming
  config.jpeg_quality = 15;

  // ==========================================================
  // PSRAM
  // ==========================================================

  if (psramFound())
  {
    Serial.println("PSRAM FOUND");

    config.fb_location = CAMERA_FB_IN_PSRAM;

    // Two buffers allow continuous streaming
    config.fb_count = 2;

    // Always use the newest frame
    config.grab_mode = CAMERA_GRAB_LATEST;
  }
  else
  {
    Serial.println("WARNING: PSRAM NOT FOUND");

    config.fb_location = CAMERA_FB_IN_DRAM;
    config.fb_count = 1;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  }

  // ==========================================================
  // INITIALIZE CAMERA
  // ==========================================================

  Serial.println("Initializing camera...");

  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK)
  {
    Serial.printf(
      "Camera initialization failed: 0x%x\n",
      err
    );

    while (true)
    {
      delay(1000);
    }
  }

  Serial.println("Camera initialized!");

  // ==========================================================
  // CAMERA SENSOR SETTINGS
  // ==========================================================

  sensor_t *sensor = esp_camera_sensor_get();

  if (sensor)
  {
    Serial.printf(
      "Camera Sensor PID: 0x%02X\n",
      sensor->id.PID
    );

    // OV3660 image adjustment
    if (sensor->id.PID == OV3660_PID)
    {
      sensor->set_vflip(sensor, 1);
      sensor->set_brightness(sensor, 1);
      sensor->set_saturation(sensor, -2);
    }

    // Make sure stream starts at QVGA
    sensor->set_framesize(
      sensor,
      FRAMESIZE_QVGA
    );

    sensor->set_quality(
      sensor,
      15
    );
  }

  // ==========================================================
  // WIFI
  // ==========================================================

  Serial.println();
  Serial.println("Connecting to WiFi...");

  WiFi.mode(WIFI_STA);

  // Disable WiFi power saving
  // This reduces streaming latency
  WiFi.setSleep(false);

  WiFi.begin(
    ssid,
    password
  );

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected!");

  // ==========================================================
  // IP ADDRESS
  // ==========================================================

  Serial.println();
  Serial.println("================================");
  Serial.println("CAMERA READY");
  Serial.println("================================");

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // ==========================================================
  // START SERVER
  // ==========================================================

  Serial.println();
  Serial.println("Starting camera server...");

  startCameraServer();

  Serial.println();
  Serial.println("================================");
  Serial.println("LIVE STREAM READY!");
  Serial.println("================================");

  Serial.print("Open: http://");
  Serial.println(WiFi.localIP());

  Serial.println();

  Serial.println("NGROK COMMAND:");
  Serial.print("ngrok http ");
  Serial.println(WiFi.localIP());

  Serial.println();
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
  delay(10000);

  Serial.printf(
    "Running | Heap: %u | PSRAM: %u\n",
    ESP.getFreeHeap(),
    ESP.getFreePsram()
  );
}
