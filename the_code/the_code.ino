#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>

#include "secrets.h"

const char* MQTT_CLIENT_ID = "ESP32_Client_01";
const char* MQTT_TOPIC     = "home/lights/bedroom";
const char* OTA_TOPIC      = "home/lights/bedroom/ota";
const int   MQTT_PORT      = 1883;

const char* OTA_URL =
  "https://github.com/Sibu464/esp32-relay/releases/latest/download/firmware.bin";

const char* FW_VERSION = "1.0.8";

const int OUTPUT_PIN = 26;
const int buzzer = 27;
const unsigned long PULSE_DURATION_MS = 1000;

// ============ GLOBALS ============
WiFiClient   espClient;
PubSubClient client(espClient);

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

bool reconnect_wifi() {
  if (WiFi.status() == WL_CONNECTED) return true;

  Serial.println("WiFi lost, reconnecting...");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Wait up to 10 seconds for WiFi to come back
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWiFi reconnected. IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  }

  Serial.println("\nWiFi reconnect failed, will retry");
  return false;
}

// ============ OTA ============
void runHttpOta() {
  Serial.printf("HTTP OTA: checking %s (current v%s)\n", OTA_URL, FW_VERSION);

  WiFiClientSecure otaClient;
  otaClient.setInsecure();

  client.disconnect();

  httpUpdate.setLedPin(buzzer, HIGH);
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

  if (String(topic) == OTA_TOPIC) {
    if (message.equalsIgnoreCase("UPDATE")) runHttpOta();
    return;
  }

  if (message == "ON" || message == "trigger") {
    triggerPulse("MQTT");
  }
}

void reconnect_mqtt() {
  if (client.connected()) return;

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
    Serial.printf("failed, rc=%d\n", client.state());
  }
}

// ============ MAIN ============
void setup() {
  Serial.begin(115200);
  Serial.printf("\nesp32-relay v%s\n", FW_VERSION);

  pinMode(OUTPUT_PIN, OUTPUT);
  pinMode(buzzer, OUTPUT);
  digitalWrite(OUTPUT_PIN, LOW);

  setup_wifi();
  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(callback);
  client.setKeepAlive(30);
}

void loop() {
  // Step 1 — make sure WiFi is up first
  if (!reconnect_wifi()) {
    delay(5000);   // WiFi not back yet, wait and try again next loop
    return;
  }

  // Step 2 — WiFi is up, now handle MQTT
  reconnect_mqtt();
  client.loop();

  // Step 3 — handle pulse timer
  handlePulse();
}