#include <WiFi.h>
#include <PubSubClient.h>
#include "esp_camera.h"

// =====================================================
// CAMERA MODEL
// =====================================================

#define CAMERA_MODEL_ESP32S3_EYE

#include "camera_pins.h"

// =====================================================
// WIFI
// =====================================================

const char* ssid = "";
const char* password = "";

// =====================================================
// THINGSBOARD
// =====================================================

const char* mqtt_server = "mqtt.thingsboard.cloud";
const int mqtt_port = 1883;

// PUT YOUR NEW THINGSBOARD ACCESS TOKEN HERE
const char* token = "";

// =====================================================
// MQTT
// =====================================================

WiFiClient espClient;
PubSubClient client(espClient);

// =====================================================
// CAMERA SETTINGS
// =====================================================

// Start with a small image because Base64 makes the
// message considerably larger.

#define CAMERA_FRAME_SIZE FRAMESIZE_QQVGA
#define JPEG_QUALITY 20

// =====================================================
// BASE64 ENCODER
// =====================================================

String base64Encode(const uint8_t *data, size_t len) {

  const char table[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789+/";

  String output;

  // Reserve approximate Base64 size
  output.reserve(((len + 2) / 3) * 4);

  for (size_t i = 0; i < len; i += 3) {

    uint32_t n = ((uint32_t)data[i]) << 16;

    if (i + 1 < len) {
      n |= ((uint32_t)data[i + 1]) << 8;
    }

    if (i + 2 < len) {
      n |= data[i + 2];
    }

    output += table[(n >> 18) & 63];
    output += table[(n >> 12) & 63];

    if (i + 1 < len) {
      output += table[(n >> 6) & 63];
    } else {
      output += '=';
    }

    if (i + 2 < len) {
      output += table[n & 63];
    } else {
      output += '=';
    }
  }

  return output;
}

// =====================================================
// WIFI CONNECTION
// =====================================================

void connectWiFi() {

  Serial.println();
  Serial.print("Connecting to WiFi");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected!");

  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());
}

// =====================================================
// MQTT CONNECTION
// =====================================================

void connectMQTT() {

  while (!client.connected()) {

    Serial.print("Connecting to ThingsBoard...");

    if (client.connect(
          "ESP32S3-Camera",
          token,
          NULL
        )) {

      Serial.println("Connected!");

    } else {

      Serial.print("Failed, rc=");
      Serial.println(client.state());

      delay(3000);
    }
  }
}

// =====================================================
// CAMERA INITIALIZATION
// =====================================================

bool initCamera() {

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

  // JPEG image
  config.pixel_format = PIXFORMAT_JPEG;

  // Small image for first test
  config.frame_size = CAMERA_FRAME_SIZE;

  // JPEG compression
  config.jpeg_quality = JPEG_QUALITY;

  // Check PSRAM
  if (psramFound()) {

    Serial.println("PSRAM detected");

    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;

  } else {

    Serial.println("WARNING: PSRAM not found");

    config.fb_location = CAMERA_FB_IN_DRAM;
    config.fb_count = 1;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  }

  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {

    Serial.print("Camera initialization failed: 0x");
    Serial.println(err, HEX);

    return false;
  }

  sensor_t* sensor = esp_camera_sensor_get();

  if (sensor != NULL) {

    sensor->set_framesize(
      sensor,
      CAMERA_FRAME_SIZE
    );

    sensor->set_quality(
      sensor,
      JPEG_QUALITY
    );

    // ESP32-S3-EYE orientation
    sensor->set_vflip(sensor, 1);
  }

  Serial.println(
    "Camera initialized successfully!"
  );

  return true;
}

// =====================================================
// CAPTURE AND SEND IMAGE
// =====================================================

bool sendCameraImage() {

  Serial.println();
  Serial.println("==============================");
  Serial.println("Taking picture...");

  camera_fb_t* fb =
    esp_camera_fb_get();

  if (fb == NULL) {

    Serial.println(
      "Camera capture FAILED!"
    );

    return false;
  }

  Serial.print("JPEG size: ");
  Serial.print(fb->len);
  Serial.println(" bytes");

  // ---------------------------------------------------
  // Convert JPEG to Base64
  // ---------------------------------------------------

  Serial.println(
    "Converting image to Base64..."
  );

  String imageBase64 =
    base64Encode(
      fb->buf,
      fb->len
    );

  // Camera buffer can now be returned
  esp_camera_fb_return(fb);

  Serial.print("Base64 size: ");
  Serial.print(imageBase64.length());
  Serial.println(" bytes");

  // ---------------------------------------------------
  // Create ThingsBoard JSON
  // ---------------------------------------------------

  String payload;

  payload.reserve(
    imageBase64.length() + 30
  );

  payload = "{\"camera_image\":\"";
  payload += imageBase64;
  payload += "\"}";

  Serial.print("MQTT payload size: ");
  Serial.print(payload.length());
  Serial.println(" bytes");

  // ---------------------------------------------------
  // Send to ThingsBoard
  // ---------------------------------------------------

  Serial.println(
    "Sending image to ThingsBoard..."
  );

  bool result =
    client.publish(
      "v1/devices/me/telemetry",
      payload.c_str()
    );

  if (result) {

    Serial.println(
      "IMAGE SENT SUCCESSFULLY!"
    );

  } else {

    Serial.println(
      "IMAGE SEND FAILED!"
    );
  }

  Serial.println("==============================");

  return result;
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  delay(2000);

  Serial.println();
  Serial.println(
    "======================================"
  );

  Serial.println(
    "ESP32-S3 CAMERA + THINGSBOARD"
  );

  Serial.println(
    "======================================"
  );

  // ---------------------------------------------------
  // WiFi
  // ---------------------------------------------------

  connectWiFi();

  // ---------------------------------------------------
  // Camera
  // ---------------------------------------------------

  if (!initCamera()) {

    Serial.println(
      "Camera failed to initialize."
    );

    while (true) {
      delay(1000);
    }
  }

  // ---------------------------------------------------
  // MQTT
  // ---------------------------------------------------

  client.setServer(
    mqtt_server,
    mqtt_port
  );

  // Increase MQTT packet buffer
  client.setBufferSize(35000);

  connectMQTT();

  Serial.println();
  Serial.println(
    "SYSTEM READY!"
  );

  // ---------------------------------------------------
  // Take one test picture
  // ---------------------------------------------------

  delay(2000);

  sendCameraImage();
}

// =====================================================
// LOOP
// =====================================================

void loop() {

  if (!client.connected()) {
    connectMQTT();
  }

  client.loop();

  // Don't continuously upload images yet.
  delay(100);
}
