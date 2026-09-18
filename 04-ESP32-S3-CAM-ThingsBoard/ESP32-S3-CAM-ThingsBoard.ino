#include <WiFi.h>
#include <PubSubClient.h>

// Wi-Fi Credentials
const char* ssid = "TambreResidence";
const char* password = "";

// ThingsBoard MQTT Server
const char* mqtt_server = "mqtt.thingsboard.cloud";
const int mqtt_port = 1883;

// Device Access Token
const char* token = "ocq48o1e3ylmqp4lrm7w";

WiFiClient espClient;
PubSubClient client(espClient);

void connectWiFi() {
  Serial.print("Connecting to WiFi");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
}

void connectMQTT() {

  while (!client.connected()) {

    Serial.print("Connecting to ThingsBoard...");

    if (client.connect("ESP32S3", token, NULL)) {
      Serial.println("Connected!");
    } else {
      Serial.print("Failed, rc=");
      Serial.println(client.state());
      delay(3000);
    }
  }
}

void setup() {

  Serial.begin(115200);

  connectWiFi();

  client.setServer(mqtt_server, mqtt_port);

  randomSeed(micros());
}

void loop() {

  if (!client.connected()) {
    connectMQTT();
  }

  client.loop();

  // Generate random temperature between 20.0°C and 35.0°C
  float temperature = random(200, 351) / 10.0;

  // Create JSON payload
  String payload = "{\"temperature\":";
  payload += String(temperature, 1);
  payload += "}";

  // Publish telemetry
  if (client.publish("v1/devices/me/telemetry", payload.c_str())) {
    Serial.print("Sent: ");
    Serial.println(payload);
  } else {
    Serial.println("Publish failed!");
  }

  delay(5000);
}
