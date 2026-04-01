#include <Arduino.h>
#include <credentials.h>
#include <DHT.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

#define POT_PIN 34
#define DHT_PIN 4
#define DHT_TYPE DHT11
#define RELAY_PIN 26
#define RELAY_ON  HIGH   // contact closed
#define RELAY_OFF LOW  // contact open

// Constants
const char* WIFI_SSID = wifi_ssid;
const char* WIFI_PASSWORD = wifi_password;
const char* MQTT_BROKER = "cc4de1ff17b64436a3537d3ab69baad9.s1.eu.hivemq.cloud";
const int MQTT_PORT = 8883;
const char* MQTT_USER = mqtt_user;
const char* MQTT_PASS = mqtt_password;

const char* TOPIC_TELEMETRY = "iot/supanat-p/esp32-electrical-protect/telemetry";
const char* TOPIC_ALERT     = "iot/supanat-p/esp32-electrical-protect/alert";
const char* TOPIC_COMMAND   = "iot/supanat-p/esp32-electrical-protect/command";

const int ADC_MAX = 4095;
const int ADC_MIN = 0;
const float LOAD_MAX = 100.0;
const float LOAD_MIN = 0;
const float LOAD_WARNING = 85.0;
const float LOAD_TRIP    = 95.0;
const float TEMP_TRIP    = 45.0;
const long READ_INTERVAL = 2000; // Read every 2 seconds

// Timing
unsigned long lastRead = 0;

// enum
enum SystemState {
  NORMAL,
  WARNING,
  TRIPPED,
  LOCKOUT
};

SystemState currentState = NORMAL;
SystemState previousState = NORMAL;

// Function Declarations
float readLoad();
float readTemperature();
void controlRelay(bool deactivate);
void evaluateState(float load, float temp);
void publishTelemetry(float load, float temp);
void publishAlert();
void mqttCallback(const char* topic, byte* payload, unsigned int length);
void handleSerialCommand();
void connectWifi();
void reconnectMQTT();
void printStatus(float load, float temp);
const char* stateToString(SystemState state);

// Objects
DHT dht(DHT_PIN, DHT_TYPE);
WiFiClientSecure wifiClient;
PubSubClient mqtt(wifiClient);

void setup() {
  Serial.begin(115200);
  delay(1000);

  connectWifi();
  wifiClient.setInsecure();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  dht.begin();
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);
  Serial.println("=================================");
  Serial.println(" Electrical Protection Monitor  ");
  Serial.println("=================================");
  delay(2000);
}

void loop() {
  if (!mqtt.connected()) {
    reconnectMQTT();
  }
  mqtt.loop();
  handleSerialCommand();
  unsigned long now = millis();
  if (now - lastRead >= READ_INTERVAL) {
    lastRead = now;

    float load = readLoad();
    float temp = readTemperature();
    previousState = currentState;
    evaluateState(load, temp);
    bool fault = (currentState == TRIPPED || currentState == LOCKOUT);
    controlRelay(fault);

    publishTelemetry(load, temp);
    if (currentState != previousState) {
      publishAlert();
    }

    printStatus(load, temp);
  }
}

float readLoad() {
  long sum = 0;
  for  (int i = 0; i < 10; i++ ) {
    sum += analogRead(POT_PIN);
    delay(5);
  }
  int avgADC = sum / 10;
  float load = map(avgADC, ADC_MIN, ADC_MAX, 0, 1000) / 10.0;
  return constrain(load, LOAD_MIN, LOAD_MAX);
}

float readTemperature() {
  float temp = dht.readTemperature();
  if (isnan(temp)) {
    Serial.println("DHT11 read failed!");
    return NAN;
  }
  return temp;
}

void controlRelay (bool fault) {
digitalWrite(RELAY_PIN, fault ? RELAY_OFF : RELAY_ON);
}

void evaluateState(float load, float temp) {
  if (isnan(temp)) return; // skip this cycle
  switch (currentState) {
    case NORMAL:
      if (load > LOAD_TRIP || temp > TEMP_TRIP) {
        currentState = TRIPPED;
      }
      else if (load > LOAD_WARNING) {
        currentState = WARNING;
      }
      break;
    case WARNING:
      if (load > LOAD_TRIP || temp > TEMP_TRIP) {
        currentState = TRIPPED;
      }
      else if (load <= 80.0) {
        currentState = NORMAL;
      }
      break;
    case TRIPPED:
      currentState = LOCKOUT;
      break;
    case LOCKOUT:
      if (load < 30.0 && temp < 35.0) {
        currentState = NORMAL;
      }
      break;
  }
}

void publishTelemetry (float load, float temp) {
  if(!mqtt.connected()) return;

  char payload[192];
  snprintf(payload, sizeof(payload), "{ \"ts\": %lu, \"load_pct\": %.1f, \"temp_c\": %.1f, \"state\": \"%s\", \"relay\": \"%s\" }",
    millis() / 1000,
    load,
    isnan(temp) ? 0.0 : temp,
    stateToString(currentState),
    (currentState == TRIPPED || currentState == LOCKOUT) ? 0 : 1);

    mqtt.publish(TOPIC_TELEMETRY,payload);
    Serial.printf("Telemetry: %s\n", payload);
}

void publishAlert() {
  if (!mqtt.connected()) return;

  char payload[192];
  snprintf(payload, sizeof(payload),
    "{ \"ts\": %lu, \"event\": \"STATE CHANGE\", \"from\": \"%s\", \"to\": \"%s\" }",
    millis() / 1000,
    stateToString(previousState),
    stateToString(currentState)
  );

  mqtt.publish(TOPIC_ALERT, payload);
  Serial.printf("Alert: %s\n", payload);
}

void mqttCallback(const char* topic, byte* payload, unsigned int length) {
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';

  Serial.printf("Command received [%s]: %s\n", topic, message);
  if(strcmp(topic, TOPIC_COMMAND) == 0) {
    if (strcmp(message, "RESET") == 0) {
      if (currentState == LOCKOUT) {
        Serial.println(">>> MANUAL RESET received");
        previousState = currentState;
        currentState = NORMAL;
        publishAlert();
      } else {
        Serial.println(">>> Reset ignored — system not in LOCKOUT");
      }
    }
  }
}

void handleSerialCommand() {
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == 'r' || cmd == 'R') {
      if (currentState == LOCKOUT) {
        Serial.println(">>> MANUAL RESET received");
        currentState = NORMAL;
        publishAlert();
      } else {
        Serial.println(">>> Reset ignored — system not in LOCKOUT");
      }
    }
  }
}

void connectWifi() {
  Serial.println("Connecting to Wifi ...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Wifi connected.");
}

void reconnectMQTT() {
    Serial.println("Attempting to MQTT...");
    if (mqtt.connect("esp32-electrical-protect",MQTT_USER, MQTT_PASS)) {
      Serial.println("MQTT connected");
      mqtt.subscribe(TOPIC_COMMAND);
      Serial.printf("Subscribed to: %s\n", TOPIC_COMMAND);
    }
    else {Serial.printf("MQTT connection failed, rc= %d", mqtt.state());
    delay(2000);
    }
}

void printStatus(float load, float temp) {
  Serial.printf(
    "{ \"ts\": %lu,\"load_pct\": %.1f, \"temp_c\": %.1f, \"state\": \"%s\", \"relay\": \"%s\" }\n",
    millis() / 1000,
    load,
    isnan(temp) ? 0.0 : temp,
    stateToString(currentState),
    (currentState == TRIPPED || currentState == LOCKOUT) ? 0 : 1
  );
}

const char* stateToString(SystemState state) {
  switch (state) {
    case NORMAL:  return "NORMAL";
    case WARNING: return "WARNING";
    case TRIPPED: return "TRIPPED";
    case LOCKOUT: return "LOCKOUT";
    default:      return "UNKNOWN";
  }
}