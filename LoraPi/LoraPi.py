from dataclasses import dataclass, asdict
import paho.mqtt.client as mqtt
import serial
import json
import sqlite3
from time import sleep
import base64

CRYPT_KEY = 5

@dataclass
class Plant:
    plantId: int
    weight: float
    type: str = "Unknown"
    address: str = "Unknown"
    reserved: bool = False

def decrypt(text, key):
    data = base64.b64decode(text)

    decrypted_data = bytearray()
    
    for byte in data:
        decrypted_data.append((byte - key) % 256)
    
    # Convert to Base64 encoding (as string)
    decrypted_text = decrypted_data.decode('utf-8')
    return decrypted_text

# SQLite3 db
connection_obj = sqlite3.connect('plant.db')
cursor_obj = connection_obj.cursor()

def dbUpdatePlant(plant, dbconnection, dbcursor):
    if not isinstance(plant,Plant):
        raise TypeError("Only a plant dataclass allowed")
    
    statement = """
    INSERT INTO PLANT (PlantID,Weight,Type,Address,Reserved)
    VALUES (?,?,?,?,?)
    ON CONFLICT(PlantID) DO UPDATE SET
    Weight = excluded.Weight
    """

    dbcursor.execute(statement, (plant.plantId, plant.weight, plant.type, plant.address, plant.reserved))
    dbconnection.commit()

def dbUpdateMqtt(plantId, address, type, reserved, dbconnection, dbcursor):
    dbcursor.execute("UPDATE PLANT SET Address = ?, Type = ?, Reserved = ? WHERE PlantID = ?",(address, type, reserved, plantId))
    dbconnection.commit()


def dbReadPlant(plantId, dbconnection, dbcursor):
    dbcursor.execute("SELECT PlantID, Weight, Type, Address, Reserved FROM PLANT WHERE PlantID = ?", (plantId,))
    row = dbcursor.fetchone()

    if row:
        return Plant(plantId=row[0], weight=row[1], type=row[2], address=row[3], reserved=row[4])
    else:
        print("No such plant")
        return None

# MQTT Client Setup
# broker = "172.20.10.2"  # Update with your Pi’s IP
broker = "localhost"
topicPlant = "/Plant"   # Houseplant topic
topicServer = "/Server" # Server topic

def on_connect(client, userdata, flags, reason_code, properties=None):
    print("Connected with code:", reason_code)
    client.subscribe(topicServer)

def on_message(client, userdata, msg):
    print("MSG COME")
    connection_thread = sqlite3.connect('plant.db')
    cursor_thread = connection_thread.cursor()

    msg_text = decrypt(msg.payload.decode("utf-8"), CRYPT_KEY)

    data = json.loads(msg_text)
    # Todo: update the data parameter
    dbUpdateMqtt(data['plantId'], data['address'], data['type'], data['reserved'], connection_thread, cursor_thread)
    plant = dbReadPlant(data['plantId'], connection_thread, cursor_thread)
    client.publish(topicPlant, json.dumps(asdict(plant)))
    print(f"Sent: {plant.plantId}, {plant.weight}, {plant.type}, {plant.address}")
    
    connection_thread.close()
    
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.on_connect = on_connect
client.on_message = on_message
client.connect(broker, 1883)
client.loop_start()

# Arduino setup
serialPort = '/dev/ttyUSB0'
while True:
    try:
        print(f"Attempt Connection with Arduino {serialPort}")
        ser = serial.Serial(serialPort, 9600, timeout=1)
        print(f"Connected to {serialPort}")
        break
    except:
        print("Serial Port Error, retry in 3 second")
        sleep(3)

try:
    while True:
        serInput = ser.readline().decode('utf-8').strip()
        if serInput:
            print(f'Received Serial: {serInput}')
            try:
                data = json.loads(serInput)
            except ValueError as e:
                print("Not a valid JSON")
                continue

            try:
                plant = Plant(data['sender_id'], data['weight'])
                dbUpdatePlant(plant, connection_obj, cursor_obj)
                plant = dbReadPlant(plant.plantId, connection_obj, cursor_obj)
                client.publish(topicPlant, json.dumps(asdict(plant)))
                print(f"Sent: {plant}")
                sleep(2)
            except TypeError as e:
                print(f"TypeError: {e}")
            except Exception as e:
                print("Serial Plant update and publish failed")
                print(e)

except KeyboardInterrupt:
    client.disconnect()
    connection_obj.close()
    print("Disconnected")