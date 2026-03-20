#include <Arduino.h>
#include <DHT.h>

#define POT_PIN 34
#define DHT_PIN 4
#define DHT_TYPE DHT11
#define RELAY_PIN 26
#define RELAY_ON  HIGH   // contact closed
#define RELAY_OFF LOW  // contact open

// Constants
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

// Function Declarations
float readLoad();
float readTemperature();
void controlRelay(bool deactivate);
void evaluateState(float load, float temp);
void handleSerialCommand();
const char* stateToString(SystemState state);
void printStatus(float load, float temp);

// Objects
DHT dht(DHT_PIN, DHT_TYPE);

// enum
enum SystemState {
  NORMAL,
  WARNING,
  TRIPPED,
  LOCKOUT
};

SystemState currentState = NORMAL;

void setup() {
  Serial.begin(115200);
  delay(1000);

  dht.begin();
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);
  Serial.println("=================================");
  Serial.println(" Electrical Protection Monitor  ");
  Serial.println("=================================");
  delay(2000);
}

void loop() {
  handleSerialCommand();

  unsigned long now = millis();
  if (now - lastRead >= READ_INTERVAL) {
    lastRead = now;

    float load = readLoad();
    float temp = readTemperature();
    evaluateState(load, temp);
    bool fault = (currentState == TRIPPED || currentState == LOCKOUT);
    controlRelay(fault);

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
    Serial.println("⚠ DHT11 read failed!");
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

void handleSerialCommand() {
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == 'r' || cmd == 'R') {
      if (currentState == LOCKOUT) {
        Serial.println(">>> MANUAL RESET received");
        currentState = NORMAL;
      } else {
        Serial.println(">>> Reset ignored — system not in LOCKOUT");
      }
    }
  }
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


void printStatus(float load, float temp) {
  Serial.printf(
    "{ \"load_pct\": %.1f, \"temp_c\": %.1f, \"state\": \"%s\", \"relay\": \"%s\" }\n",
    load,
    temp,
    stateToString(currentState),
    (currentState == TRIPPED || currentState == LOCKOUT) ? "TRIPPED(OFF)" : "NORMAL(ON)"
  );
}
