#include <SPI.h>
#include <RHMesh.h>
#include <RH_RF95.h>

#define RF95_FREQ 920.0
#define RFM95_CS 10
#define RFM95_RST 9
#define RFM95_INT 2

#define NODE_ID 3         // Mesh ID for Node 1
#define NODE_STR_ID "003" // String ID for Node 1
#define GATEWAY_ID 999    // Gateway Mesh ID

RH_RF95 rf95(RFM95_CS, RFM95_INT);
RHMesh manager(rf95, NODE_ID);

struct LoraMessage {
  char sender_id[4];
  char dest_id[4];
  float weight;
  // char food_type[8];
} msg;

unsigned long lastSend = 0;
const unsigned long sendInterval = 10000; // Send every 10 seconds

void setup() {
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);
  Serial.begin(9600);
  delay(100);
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  if (!manager.init()) {
    Serial.println("Mesh init failed");
    while (1);
  }
  rf95.setFrequency(RF95_FREQ);
  rf95.setTxPower(14, false);
  rf95.setCADTimeout(500);
  Serial.print("Node initialized (ID: ");
  Serial.print(NODE_STR_ID);
  Serial.println(")");
  randomSeed(analogRead(0));
}

void loop() {
  unsigned long now = millis();
  
  // Check if it's time to send new messages
  if (now - lastSend >= sendInterval) {
    uint8_t data[sizeof(msg)];
    
    // --- Send directed message to Gateway ---
    strcpy(msg.sender_id, NODE_STR_ID);
    strcpy(msg.dest_id, "999");
    msg.weight = 690.0;             
    // strcpy(msg.food_type, "Cheese"); 
    memcpy(data, &msg, sizeof(msg));
    
    Serial.println("Sending to gateway, weight set to 690g");
    if (manager.sendtoWait(data, sizeof(msg), GATEWAY_ID)) {
      Serial.println("Gateway send acknowledged");
    } else {
      Serial.println("Gateway send failed");
    }
    
    // --- Broadcast message to Mesh ---
    strcpy(msg.dest_id, "000"); // "000" used as a broadcast indicator
    Serial.println("Broadcasting to mesh, weight set to 690g");
    if (manager.sendto(data, sizeof(msg), RH_BROADCAST_ADDRESS)) {
      Serial.println("Broadcast sent successfully");
    } else {
      Serial.println("Broadcast send failed");
    }
    
    lastSend = now;
  }
  
  // --- Continuously poll for incoming messages ---
  uint8_t buf[RH_MESH_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);
  uint8_t from;
  
  if (manager.available()) {
    // For broadcast messages we use recvfrom (no ACK)
    if (manager.recvfrom(buf, &len, &from)) {
      if (len == sizeof(LoraMessage)) {
        LoraMessage received;
        memcpy(&received, buf, sizeof(received));
        // Ignore messages sent by self
        if (strcmp(received.sender_id, NODE_STR_ID) != 0) {
          Serial.print("Received broadcast from ");
          Serial.print(received.sender_id);
          Serial.print(" to ");
          Serial.print(received.dest_id);
          Serial.print(": Weight=");
          Serial.print(received.weight);
          // Serial.print("g, Food=");
          // Serial.println(received.food_type);
        }
      }
    }
  }
  // A short delay helps avoid saturating the loop but keeps it responsive.
  delay(50);
}