#include <SPI.h>
#include <RHMesh.h>
#include <RH_RF95.h>

#define RF95_FREQ 915.0
#define RFM95_CS 10
#define RFM95_RST 9
#define RFM95_INT 2

#define CRYPT_KEY 5

RH_RF95 rf95(RFM95_CS, RFM95_INT);
RHMesh manager(rf95, 999);

struct LoraMessage {
  char sender_id[4];
  char dest_id[4];
  float weight;
  // char food_type[8];
};

void decrypt(uint8_t* data, size_t len, int shift) {
  for (size_t i = 0; i < len; i++) {
    data[i] = (data[i] - shift) % 256;
  }
}

bool isValidMessage(LoraMessage msg) {
  // Check dest_id is "999\0"
  if (strncmp(msg.dest_id, "999", 4) != 0) {
    return false;
  }

  // Validate sender_id: Check up to first null or all 4 bytes if no null
  int sender_len = 0;
  for (int i = 0; i < 4; i++) {
    if (msg.sender_id[i] == '\0') {
      sender_len = i; // Length up to null
      break;
    }
    if (msg.sender_id[i] < 32 || msg.sender_id[i] > 126) {
      return false; // Non-printable character
    }
    sender_len = i + 1; // No null yet, keep counting
  }
  if (sender_len == 0) {
    return false; // Empty sender_id not allowed
  }

  return true;
}

void setup() {
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);
  Serial.begin(9600);
  while (!Serial);
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  if (!manager.init()) {
    Serial.println("Mesh failed");
    while (1);
  }
  rf95.setFrequency(RF95_FREQ);
  rf95.setTxPower(20, false);
  rf95.setCADTimeout(500);
  Serial.println("Gateway ready (ID: 999)");
}

void loop() {
  uint8_t buf[32];
  uint8_t len = sizeof(buf);
  uint8_t from;
  if (manager.available()) {
    if (manager.recvfromAck(buf, &len, &from)) {
      if (len == sizeof(LoraMessage)) {
        
        decrypt(buf,sizeof(LoraMessage), CRYPT_KEY);

        LoraMessage msg;
        memcpy(&msg, buf, sizeof(msg));
  
        if(!isValidMessage(msg)){
          Serial.println("Invalid message received");
          return;
        }

        char weight_str[10];
        dtostrf(msg.weight, 6, 2, weight_str);
        char json[100];
        snprintf(json, sizeof(json), 
                 "{\"sender_id\":\"%s\",\"dest_id\":\"%s\",\"weight\":%s}",
                 msg.sender_id, msg.dest_id, weight_str);
        Serial.println(json);
        Serial.flush();
      } else {
        Serial.println("Invalid message size");
      }
    }
  }
  delay(200);
}