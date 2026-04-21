#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>

#include "secrets.h"

const char* MQTT_CLIENT_ID = "ESP32_Client_01";
const char* MQTT_TOPIC     = "home/lights/bedroom";
const char* OTA_TOPIC      = "home/lights/bedroom/ota";
const int   MQTT_PORT      = 1883;   // 8883 once you enable TLS


const char* OTA_URL =
  "https://github.com/Sibu464/esp32-relay/releases/latest/download/firmware.bin";

// Bump this before each release tag so logs are honest
const char* FW_VERSION = "1.0.3";

const int OUTPUT_PIN = 27;
const unsigned long PULSE_DURATION_MS = 1000;

// ============ GLOBALS ============
WiFiClient    espClient;
PubSubClient  client(espClient);

bool pulseActive = false;
unsigned long pulseStartTime = 0;

// ============ PULSE ============
void triggerPulse(const char* source) {
  digitalWrite(OUTPUT_PIN, HIGH);
  pulseActive = true;
  pulseStartTime = millis();
  Serial.printf("Output pin HIGH (via %s)\n", source);
}

void handlePulse() {
  if (pulseActive && (millis() - pulseStartTime >= PULSE_DURATION_MS)) {
    digitalWrite(OUTPUT_PIN, LOW);
    pulseActive = false;
    Serial.println("Output pin LOW");
  }
}

// ============ WIFI ============
void setup_wifi() {
  delay(10);
  Serial.printf("\nConnecting to %s", WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.printf("\nWiFi connected. IP: %s\n", WiFi.localIP().toString().c_str());
}

// ============ OTA (HTTP pull from GitHub Releases) ============
void runHttpOta() {
  Serial.printf("HTTP OTA: checking %s (current v%s)\n", OTA_URL, FW_VERSION);

  WiFiClientSecure otaClient;
  otaClient.setInsecure();

  client.disconnect();

  httpUpdate.setLedPin(OUTPUT_PIN, HIGH);
  httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);  

  t_httpUpdate_return ret = httpUpdate.update(otaClient, OTA_URL, FW_VERSION);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("OTA failed: (%d) %s\n",
        httpUpdate.getLastError(),
        httpUpdate.getLastErrorString().c_str());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("OTA: already up to date");
      break;
    case HTTP_UPDATE_OK:
      Serial.println("OTA: success, rebooting");
      break;
  }
}
// ============ MQTT ============
void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];
  Serial.printf("MQTT [%s]: %s\n", topic, message.c_str());

  message.trim();

  // OTA control topic
  if (String(topic) == OTA_TOPIC) {
    if (message.equalsIgnoreCase("UPDATE")) runHttpOta();
    return;
  }

  // Pulse control topic
  if (message == "ON" || message == "1" || message == "trigger") {
    triggerPulse("MQTT");
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");

    bool connected;
    if (strlen(MQTT_USER) > 0) {
      connected = client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS);
    } else {
      connected = client.connect(MQTT_CLIENT_ID);
    }

    if (connected) {
      Serial.println("connected");
      client.subscribe(MQTT_TOPIC);
      client.subscribe(OTA_TOPIC);
      Serial.printf("Subscribed to %s and %s\n", MQTT_TOPIC, OTA_TOPIC);
    } else {
      Serial.printf("failed, rc=%d  retrying in 5s\n", client.state());
      delay(5000);
    }
  }
}

// ============ MAIN ============
void setup() {
  Serial.begin(115200);
  Serial.printf("\nesp32-relay v%s\n", FW_VERSION);

  pinMode(OUTPUT_PIN, OUTPUT);
  digitalWrite(OUTPUT_PIN, LOW);

  setup_wifi();
  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(callback);
  client.setKeepAlive(30);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    delay(1000);
    return;
  }

  if (!client.connected()) reconnect();
  client.loop();

  handlePulse();
}