#include <SPI.h>
#include <RHMesh.h>
#include <RH_RF95.h>
#include <HX711.h>

#define RF95_FREQ 915.0
#define RFM95_CS 10
#define RFM95_RST 9
#define RFM95_INT 2
#define NODE_ID 1
#define NODE_STR_ID "001"
#define GATEWAY_ID 999
#define DT A0
#define CLK A1
#define DRAIN_PUMP_PIN 5
#define REFILL_PUMP_PIN 4

#define CRYPT_KEY 5

RH_RF95 rf95(RFM95_CS, RFM95_INT);
RHMesh manager(rf95, NODE_ID);
HX711 scale;

void encrypt(uint8_t* data, size_t len, int shift) {
  for (size_t i = 0; i < len; i++) {
    data[i] = (data[i] + shift) % 256;
  }
}

struct LoraMessage {
  char sender_id[4];
  char dest_id[4];
  float weight;
} msg;

unsigned long lastCycle = 0;
const unsigned long cycleInterval = 60000;
const unsigned long pumpDuration = 10000;

void setup() {

  pinMode(DRAIN_PUMP_PIN, OUTPUT);
  pinMode(REFILL_PUMP_PIN, OUTPUT);
  digitalWrite(DRAIN_PUMP_PIN, LOW);
  digitalWrite(REFILL_PUMP_PIN, LOW);


  // pinMode(RFM95_RST, OUTPUT);
  // digitalWrite(RFM95_RST, HIGH);
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);
  Serial.begin(9600);
  delay(100);
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  Serial.println("Initializing mesh...");
  if (!manager.init()) {
    Serial.println("Mesh init failed - check wiring or pin conflicts");
    while (1);
  }

  

  Serial.println("Resetting LoRa...");
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  Serial.println("Initializing scale...");
  scale.begin(DT, CLK);
  Serial.println("Taring...");
  scale.tare();
  delay(500);
  Serial.println("Tare done.");
  scale.set_scale(-1312.64);
  Serial.println("Scale ready.");

  
  Serial.println("Mesh initialized");
  rf95.setFrequency(RF95_FREQ);
  rf95.setTxPower(20, false);
  rf95.setCADTimeout(500);
  Serial.print("Node ID: ");
  Serial.println(NODE_STR_ID);
  randomSeed(analogRead(0));
}

void loop() {
  unsigned long now = millis();
  if (now - lastCycle >= cycleInterval) {
    Serial.println("Starting cycle: Drainage pump ON");
    digitalWrite(DRAIN_PUMP_PIN, HIGH);
    delay(pumpDuration);
    digitalWrite(DRAIN_PUMP_PIN, LOW);
    Serial.println("Drainage pump OFF");

    if (scale.is_ready()) {
      float weight = scale.get_units(5);
      Serial.print("W: ");
      Serial.print(weight);
      Serial.println("g");

      strcpy(msg.sender_id, NODE_STR_ID);
      strcpy(msg.dest_id, "999");
      msg.weight = weight;
      uint8_t data[sizeof(msg)];
      memcpy(data, &msg, sizeof(msg));

      encrypt(data, sizeof(data), CRYPT_KEY);

      Serial.println("To gateway...");
      unsigned long sendTimeStamp = millis();
      if (!manager.sendtoWait(data, sizeof(msg), GATEWAY_ID)) {
        unsigned long ackTimeStamp = millis();
        unsigned long latency = (ackTimeStamp - sendTimeStamp)/2;
        Serial.print("Latency: ");
        Serial.println(latency);
        Serial.println("Gateway OK");
      } else {
        Serial.println("Gateway fail");
      }

    } else {
      Serial.println("Scale not ready - skipping publish");
    }

    Serial.println("Refill pump ON");
    digitalWrite(REFILL_PUMP_PIN, HIGH);
    delay(pumpDuration);
    digitalWrite(REFILL_PUMP_PIN, LOW);
    Serial.println("Refill pump OFF");

    lastCycle = now;
    Serial.println("Cycle complete");
  }

  uint8_t buf[32];
  uint8_t len = sizeof(buf);
  uint8_t from;
  if (manager.available()) {
    if (manager.recvfrom(buf, &len, &from)) {
      if (len == sizeof(LoraMessage)) {
        LoraMessage received;
        memcpy(&received, buf, sizeof(received));
        if (strcmp(received.sender_id, NODE_STR_ID) != 0) {
          Serial.print("From ");
          Serial.print(received.sender_id);
          Serial.print(": ");
          Serial.print(received.weight);
          Serial.println("g");
        }
      }
    }
  }

  delay(50);
}