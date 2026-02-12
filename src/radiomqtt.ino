#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>

// Wi-Fi Credentials
const char* WIFI_SSID = "Vinternet phone";
const char* WIFI_PASS = "vincecode001";

// MQTT Credentials
const char* MQTT_HOST = "10.225.184.197";
const uint16_t MQTT_PORT = 8883;
const char* MQTT_USER = "greenhouse-user";
const char* MQTT_PASS = "greenhouse-password";

// Presence payload - To check whether this device is online or offline from the portal
const char* BIRTH_PAYLOAD = "online";
const char* DEATH_PAYLOAD = "offline";

// todo Device / Topics todo
String deviceId;        // heltec-v3-xxxx
String presenceTopic;   // greenhouse/devices/<deviceId>
String pubTopic;        // greenhouse/telemetry/<deviceId>/counter
String subTopic;        // greenhouse/commands/<deviceId>/#

// ===== CA cert =====
static const char CA_CERT[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIEczCCA1ugAwIBAgIUZXXk1j02diJPv38TPxb9ec6pygUwDQYJKoZIhvcNAQEL
BQAwgcgxCzAJBgNVBAYTAkZSMRwwGgYDVQQIDBNDZW50cmUtVmFsIGRlIExvaXJl
MRMwEQYDVQQHDApPcmzDg8KpYW5zMSMwIQYDVQQKDBpVbml2ZXJzaXTDg8KpIGQn
T3Jsw4PCqWFuczEcMBoGA1UECwwTUG9seXRlY2ggT3Jsw4PCqWFuczEdMBsGA1UE
AwwUZ3JlZW5ob3VzZS1zZXJ2ZXItY2ExJDAiBgkqhkiG9w0BCQEWFW13b3JpYXZp
bmNlQGdtYWlsLmNvbTAeFw0yNjAyMDkxNTE2MDlaFw0zNjAyMDcxNTE2MDlaMIHI
MQswCQYDVQQGEwJGUjEcMBoGA1UECAwTQ2VudHJlLVZhbCBkZSBMb2lyZTETMBEG
A1UEBwwKT3Jsw4PCqWFuczEjMCEGA1UECgwaVW5pdmVyc2l0w4PCqSBkJ09ybMOD
wqlhbnMxHDAaBgNVBAsME1BvbHl0ZWNoIE9ybMODwqlhbnMxHTAbBgNVBAMMFGdy
ZWVuaG91c2Utc2VydmVyLWNhMSQwIgYJKoZIhvcNAQkBFhVtd29yaWF2aW5jZUBn
bWFpbC5jb20wggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCRos4X5AKJ
RcWq9YhvVBqtEtRCCm/ZgVFAOxP2KEUrjekJBw92XLjL9jhuh/FTcWi3Sq596aPs
0Q3uhNJMTHcrUKODajbYkKNeP94kfetSVUZSyk+mvp3+yatKzV2xR/JNMaU3LbdO
3QhbHaLVoIWB+eb6UZWUiKU2WkgzjYA2ZhBN56x6QbU7x4efFDGYmZoFAQgCJR4u
UjO7kozbkzfveGPusNJdITgeOvtRUPlWr2VKtdYwA9+rBAsyhItTeNJMjugPy6Ta
dTim7+xb4f52AaAMCvxapnGdGEo5FqPho++M789h8l/rjfFQbm/L1CA3Jo7hdGuC
5seTcEfKqx3TAgMBAAGjUzBRMB0GA1UdDgQWBBT7cB77twW6qerymcP5nJjekntk
WjAfBgNVHSMEGDAWgBT7cB77twW6qerymcP5nJjekntkWjAPBgNVHRMBAf8EBTAD
AQH/MA0GCSqGSIb3DQEBCwUAA4IBAQB9YNZnMNDJJz4iye56EtBWj+1B97D3BudD
lbT+3OH1/PG7LO9yheTJbVU2ObPzOK1hCj6AVigTZOO0IXHMokAchuaKTCrYoPaL
C4eLNZVCfmTWu6YnwnE0TojSc85RdqEyCPK8ofxkpk2zr04PhAQQup2PCspNPtUH
Sir5N3W92+DmvlOyQ4ao67EGkyvqFLIhAFxVEcQQD32bVCI/TSQM74swLLRFkQCX
RJxMSg/Rso9m3Wlyi3xVK9u/bnJFVXWZRtqx1sDdUETrdj8gutghbXq0E0Y8QRZ0
vEfJlB79hPpmskVGSx1x7cd2cxmxWW2dXmQyTdHv3LsynzGOCzQC
-----END CERTIFICATE-----
)EOF";

// ===== Clients =====
WiFiClientSecure tlsClient;
PubSubClient mqtt(tlsClient);

// ===== Publishing =====
unsigned long lastPub = 0;
int counter = 1;

// Time sync needed for TLS cert validation
bool syncTime() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  for (int i = 0; i < 40; i++) {
    time_t now = time(nullptr);
    if (now > 1700000000) return true;
    delay(250);
  }
  return false;
}

// RX callback
void onMessage(char* topic, byte* payload, unsigned int length) {
  String msg;
  msg.reserve(length);
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  Serial.print("MQTT RX [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(msg);
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi OK. IP=");
  Serial.println(WiFi.localIP());
}

void connectMQTT() {
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMessage);

  while (!mqtt.connected()) {
    Serial.print("MQTT connecting as ");
    Serial.println(deviceId);

    bool ok = mqtt.connect(
      deviceId.c_str(),
      MQTT_USER, MQTT_PASS,
      presenceTopic.c_str(), 1, true, DEATH_PAYLOAD  // LWT offline
    );

    if (ok) {
      Serial.println("MQTT connected!");

      mqtt.subscribe(subTopic.c_str(), 1);
      Serial.print("Subscribed to: ");
      Serial.println(subTopic);

      mqtt.publish(presenceTopic.c_str(), BIRTH_PAYLOAD, true); // birth retained
      Serial.print("Presence published [");
      Serial.print(presenceTopic);
      Serial.println("] online");
    } else {
      Serial.print("MQTT failed, state=");
      Serial.println(mqtt.state());
      delay(1200);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1200);

  // Unique device ID from MAC
  uint64_t mac = ESP.getEfuseMac();
  deviceId = "heltec-v3-" + String((uint32_t)(mac & 0xFFFFFFFF), HEX);

  // Build topics using deviceId
  presenceTopic = String("greenhouse/devices/") + deviceId;
  pubTopic      = String("greenhouse/telemetry/") + deviceId + "/counter";
  subTopic      = String("greenhouse/commands/") + deviceId + "/#";

  Serial.print("DeviceId: ");
  Serial.println(deviceId);

  Serial.print("PresenceTopic: ");
  Serial.println(presenceTopic);

  Serial.print("PublishTopic: ");
  Serial.println(pubTopic);

  Serial.print("SubscribeTopic: ");
  Serial.println(subTopic);

  connectWiFi();

  Serial.println("Syncing time (NTP)...");
  Serial.println(syncTime() ? "Time synced." : "Time NOT synced (TLS may fail).");

  tlsClient.setCACert(CA_CERT);

  connectMQTT();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost. Restarting...");
    delay(1000);
    ESP.restart();
  }

  if (!mqtt.connected()) {
    connectMQTT();
  }

  mqtt.loop();

  // Publish every 10 seconds
  if (millis() - lastPub >= 10000) {
    lastPub = millis();
    String payload = String(counter);

    mqtt.publish(pubTopic.c_str(), payload.c_str(), false);

    Serial.print(pubTopic);
    Serial.print(" - ");
    Serial.println(payload);

    counter++;
  }
}
