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

RH_RF95 rf95(RFM95_CS, RFM95_INT);
RHMesh manager(rf95, NODE_ID);
HX711 scale;

struct LoraMessage {
  char sender_id[4];
  char dest_id[4];
  float weight;
  // char food_type[8];
} msg;

unsigned long lastWeightCheck = 0;
const unsigned long weightCheckInterval = 5000;
unsigned long lastSend = 0;
const unsigned long sendInterval = 10000;
const float threshold = 100.0;

void setup() {
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);
  Serial.begin(9600);
  delay(100);
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  scale.begin(DT, CLK);
  Serial.println("Taring...");
  scale.tare();
  delay(500);
  Serial.println("Tare done.");
  scale.set_scale(-1312.64);
  Serial.println("Scale ready.");

  if (!manager.init()) {
    Serial.println("Mesh failed");
    while (1);
  }
  rf95.setFrequency(RF95_FREQ);
  rf95.setTxPower(20, false);
  rf95.setCADTimeout(500);
  Serial.print("Node ID: ");
  Serial.println(NODE_STR_ID);
  randomSeed(analogRead(0));
}

void loop() {
  unsigned long now = millis();

  if (now - lastWeightCheck >= weightCheckInterval) {
    if (scale.is_ready()) {
      float weight = scale.get_units(5);
      Serial.print("W: ");
      Serial.print(weight);
      Serial.println("g");

      if (weight > threshold && now - lastSend >= sendInterval) {
        Serial.println("Above threshold");
        strcpy(msg.sender_id, NODE_STR_ID);
        strcpy(msg.dest_id, "999");
        msg.weight = weight;
        // strcpy(msg.food_type, "Tomato");

        uint8_t data[sizeof(msg)];
        memcpy(data, &msg, sizeof(msg));

        Serial.println("To gateway...");
        if (manager.sendtoWait(data, sizeof(msg), GATEWAY_ID)) {
          Serial.println("Gateway OK");
        } else {
          Serial.println("Gateway fail");
        }

        strcpy(msg.dest_id, "000");
        Serial.println("To mesh...");
        if (manager.sendto(data, sizeof(msg), RH_BROADCAST_ADDRESS)) {
          Serial.println("Mesh OK");
        } else {
          Serial.println("Mesh fail");
        }

        lastSend = now;
      } else if (weight <= threshold) {
        Serial.println("Below threshold");
      }
    } else {
      Serial.println("Scale not ready");
    }
    lastWeightCheck = now;
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
          Serial.print("g ");
          // Serial.println(received.food_type);
        }
      }
    }
  }

  delay(50);
}