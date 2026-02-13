#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "radioMqttTask.h"

// ===================== Configuration =====================
static const char* WIFI_SSID = "Cudy-5B0C";
static const char* WIFI_PASS = "0477060671";

static const char* MQTT_HOST = "192.168.10.100";
static const uint16_t MQTT_PORT = 1883;
static const char* MQTT_USER = "greenhouse-user";
static const char* MQTT_PASS = "greenhouse-password";

static const char* BIRTH_PAYLOAD = "online";
static const char* DEATH_PAYLOAD = "offline";

static const char* TOPIC_DEVICES_BASE   = "greenhouse/devices/";
static const char* TOPIC_TELEMETRY_BASE = "greenhouse/telemetry/environmentdata/";
static const char* TOPIC_COMMAND_BASE   = "greenhouse/commands/remotecommand1/";

static const uint32_t WIFI_TIMEOUT_MS    = 20000;
static const uint32_t MQTT_TIMEOUT_MS    = 15000;
static const uint32_t PUBLISH_INTERVAL_MS = 10000;
static const uint32_t TASK_DELAY_MS      = 50;

// ===================== State =====================
static String deviceId;
static String presenceTopic;
static String pubTopic;
static String subTopic;

static WiFiClient netClient;
static PubSubClient mqtt(netClient);

static TaskHandle_t hMqttTask = nullptr;

// ===================== MQTT Callback =====================
static void onMessage(char* topic, byte* payload, unsigned int length) {
  String msg;
  msg.reserve(length);
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  Serial.print("MQTT RX [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(msg);

  // ------------------------------------------------------------------
  // TODO: Push received command into RTOS queue
  //
  // Steps:
  // 1. Parse topic to identify command type
  // 2. Convert payload into command structure
  // 3. Send command to logic task via queue
  //
  // Example:
  // CommandMessage cmd;
  // cmd.type = CMD_REMOTE1;
  // cmd.value = msg.toInt();
  // xQueueSend(commandQueue, &cmd, 0);
  // ------------------------------------------------------------------
}

// ===================== Connectivity =====================
static bool ensureWiFi(uint32_t timeoutMs = WIFI_TIMEOUT_MS) {
  if (WiFi.status() == WL_CONNECTED) return true;

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  Serial.print("WiFi connecting to: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    vTaskDelay(pdMS_TO_TICKS(300));
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi OK. IP=");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI=");
    Serial.println(WiFi.RSSI());
    return true;
  }

  Serial.print("WiFi FAILED. Status=");
  Serial.println((int)WiFi.status());
  return false;
}

static bool ensureMQTT(uint32_t timeoutMs = MQTT_TIMEOUT_MS) {
  if (mqtt.connected()) return true;

  if (presenceTopic.isEmpty() || pubTopic.isEmpty() || subTopic.isEmpty()) {
    Serial.println("ERROR: MQTT topics are empty. Did mqttInit() run?");
    return false;
  }

  const String clientId = deviceId + "-" + String((uint32_t)esp_random(), HEX);

  Serial.print("MQTT connecting as ");
  Serial.println(clientId);

  uint32_t start = millis();
  while (!mqtt.connected() && (millis() - start) < timeoutMs) {
    const bool ok = mqtt.connect(
      clientId.c_str(),
      MQTT_USER, MQTT_PASS,
      presenceTopic.c_str(), 1, true, DEATH_PAYLOAD
    );

    if (ok) {
      Serial.println("MQTT connected!");

      mqtt.subscribe(subTopic.c_str(), 1);
      Serial.print("Subscribed: ");
      Serial.println(subTopic);

      mqtt.publish(presenceTopic.c_str(), BIRTH_PAYLOAD, true);
      Serial.print("Presence published: ");
      Serial.println(BIRTH_PAYLOAD);

      return true;
    }

    Serial.print("MQTT failed, state=");
    Serial.println(mqtt.state());
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  return mqtt.connected();
}

// ===================== RTOS Task =====================
static void mqttTask(void* pv) {
  Serial.println("RTOS MQTT task started.");

  uint32_t lastPub = 0;
  uint32_t counter = 1;

  for (;;) {
    if (!ensureWiFi()) {
      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }

    if (!ensureMQTT()) {
      vTaskDelay(pdMS_TO_TICKS(1500));
      continue;
    }

    mqtt.loop();

    // ------------------------------------------------------------------
    // TODO: Replace counter publish with queue-based telemetry
    //
    // Steps:
    // 1. Read telemetry message from RTOS queue
    // 2. Convert struct to MQTT payload (JSON or simple values)
    // 3. Publish to pubTopic
    //
    // Example:
    // TelemetryMessage msg;
    // if (xQueueReceive(telemetryQueue, &msg, 0) == pdPASS) {
    //   mqtt.publish(pubTopic.c_str(), msg.payload, false);
    // }
    // ------------------------------------------------------------------

    // Temporary demo publisher (remove when queue is used)
    if (millis() - lastPub >= PUBLISH_INTERVAL_MS) {
      lastPub = millis();

      const String payload = String(counter++);
      const bool ok = mqtt.publish(pubTopic.c_str(), payload.c_str(), false);

      Serial.print("MQTT TX [");
      Serial.print(pubTopic);
      Serial.print("] ");
      Serial.print(payload);
      Serial.println(ok ? " ✓" : " ✗");
    }

    vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS));
  }
}

// ===================== Public API =====================
void mqttInit() {
  const uint64_t mac = ESP.getEfuseMac();
  deviceId = "heltec-v3-" + String((uint32_t)(mac & 0xFFFFFFFF), HEX);

  presenceTopic = String(TOPIC_DEVICES_BASE) + deviceId;
  pubTopic      = String(TOPIC_TELEMETRY_BASE) + deviceId + "/counter";
  subTopic      = String(TOPIC_COMMAND_BASE) + deviceId + "/#";

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMessage);

  Serial.println("===== MQTT CONFIG =====");
  Serial.print("DeviceId: ");       Serial.println(deviceId);
  Serial.print("PresenceTopic: ");  Serial.println(presenceTopic);
  Serial.print("PublishTopic: ");   Serial.println(pubTopic);
  Serial.print("SubscribeTopic: "); Serial.println(subTopic);
  Serial.println("=======================");
}

void mqttStartTask() {
  if (hMqttTask != nullptr) {
    Serial.println("MQTT task already running.");
    return;
  }
  xTaskCreate(mqttTask, "mqttTask", 6144, nullptr, 2, &hMqttTask);
}
