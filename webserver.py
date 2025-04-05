from flask import Flask, render_template, request, redirect, url_for
from flask_socketio import SocketIO, emit
import paho.mqtt.client as mqtt
import json
import base64

app = Flask(__name__)
socketio = SocketIO(app)

broker = "localhost"
topicPlant = "/Plant"
topicServer = "/Server"
items = []

ENCRYPT_KEY = 5

def encrypt(text, key):
    data = text.encode('utf-8')
    encrypted_data = bytearray()

    for byte in data:
        encrypted_data.append((byte + key) % 256)
    
    encrypted_base64 = base64.b64encode(bytes(encrypted_data)).decode('utf-8')
    return encrypted_base64

def on_connect(client, userdata, flags, rc):
    print("Connected to MQTT broker with code:", rc)
    client.subscribe(topicPlant)

def on_message(client, userdata, msg):
    data = json.loads(msg.payload.decode("utf-8"))
    plant_id = data['plantId']
    new_weight = data['weight']
    new_type = data['type']
    new_address = data.get('address')
    new_reserved = data.get('reserved')  # Could be 0/1 from MQTT

    # Debug: Log the incoming reserved value
    print(f"Received reserved value: {new_reserved}")

    # Convert reserved to boolean (0 = False, 1 = True)
    if new_reserved is not None:
        new_reserved = bool(int(new_reserved))  # Ensure 0/1 is converted correctly
    else:
        new_reserved = False  # Default to False if not provided

    for item in items:
        if item['id'] == plant_id:
            plant_type = new_type
            plant_weight = new_weight
            plant_address = new_address if new_address is not None else item['address']
            plant_reserved = new_reserved if new_reserved is not None else item['reserved']

            item.update({
                'value': f"Plant {plant_id} - {plant_type}\nWeight: {round(plant_weight, 2)} g\nAddress of Owner: {plant_address}\nIs the item reserved?: {'Reserved' if plant_reserved else 'Available'}",
                'type': plant_type,
                'weight': plant_weight,
                'address': plant_address,
                'reserved': plant_reserved
            })
            print(f"Updated via MQTT: Plant {plant_id} to {plant_weight} g, {plant_type}, {plant_address}, Reserved: {plant_reserved}")
            print(f"Updated value string: {item['value']}")
            socketio.emit('new_item', item)
            return

    plant_info = {
        'id': plant_id,
        'value': f"Plant {plant_id} - {new_type}\nWeight: {round(new_weight, 2)} g\nAddress of Owner: {new_address or 'Unknown'}\nIs the item reserved?: {'Reserved' if new_reserved else 'Available'}",
        'type': new_type,
        'weight': new_weight,
        'address': new_address or 'Unknown',
        'reserved': new_reserved
    }
    items.append(plant_info)
    print(f"Added via MQTT: {plant_id}, {new_weight}, {new_type}, {new_address or 'Unknown'}, Reserved: {plant_info['reserved']}")
    print(f"Added value string: {plant_info['value']}")
    socketio.emit('new_item', plant_info)

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect(broker, 1883)
client.loop_start()

@app.route('/')
def index():
    return render_template('index.html', items=items)

@app.route('/details', methods=['POST'])
def receive_plant_data():
    if request.is_json:
        data = request.get_json()
    else:
        data = {
            'plantId': request.form['plantId'],
            'weight': request.form.get('weight'),
            'type': request.form.get('type'),
            'address': request.form.get('address'),
            'reserved': request.form.get('reserved')
        }
    plant_id = int(data['plantId'])
    new_weight = (data.get('weight'))
    new_type = data.get('type')
    new_address = data.get('address')
    new_reserved = data.get('reserved')

    # Debug: Log the incoming reserved value from form
    print(f"Received reserved value from form: {new_reserved}")

    # Convert reserved from form ("true"/"false") to boolean
    if new_reserved.lower() == 'true':
        reserved = True
    else:
        reserved = False  # Default to False if not provided

    print(f'plant id debug: {plant_id} / {type(plant_id)}')
    for item in items:
        debugitem = item['id']
        print(f'item id debug loop: {debugitem} / {type(debugitem)}')
        if item['id'] == plant_id:
            plant_type = new_type if new_type else item['type']
            plant_weight = new_weight if new_weight else item['weight']
            address = new_address if new_address is not None else item['address']
            plant_reserved = reserved if new_reserved is not None else item['reserved']

            item.update({
                'value': f"Plant {plant_id} - {plant_type}\nWeight: {round(plant_weight, 2)} g\nAddress of Owner: {address}\nIs the item reserved?: {'Reserved' if plant_reserved else 'Available'}",
                'type': plant_type,
                'weight': plant_weight,
                'address': address,
                'reserved': plant_reserved
            })
            print(f"Updated via Flask: Plant {plant_id} to {plant_weight} g, {plant_type}, {address}, Reserved: {plant_reserved}")
            print(f"Updated value string: {item['value']}")
            socketio.emit('new_item', item)

            updated_data = {
                'plantId': plant_id,
                'type': plant_type,
                'weight': plant_weight,
                'address': address,
                'reserved': int(plant_reserved)  # Convert to 1/0 for MQTT
            }
            
            ciphertext = encrypt(json.dumps(updated_data), ENCRYPT_KEY)

            client.publish(topicServer, ciphertext)
            print(f"Published update to {topicServer}: {updated_data}")
            return redirect(url_for('details', item_id=plant_id))

    if not new_weight or not new_type:
        return "Weight and type are required for new plants", 400
    plant_info = {
        'id': plant_id,
        'value': f"Plant {plant_id} - {new_type}\nWeight: {round(float(new_weight), 2)} g\nAddress of Owner: {new_address or 'Unknown'}\nIs the item reserved?: {'Reserved' if reserved else 'Available'}",
        'type': new_type,
        'weight': float(new_weight),
        'address': new_address or 'Unknown',
        'reserved': reserved
    }
    items.append(plant_info)
    print(f"Added via Flask: {plant_id}, {new_weight}, {new_type}, {new_address or 'Unknown'}, Reserved: {plant_info['reserved']}")
    print(f"Added value string: {plant_info['value']}")
    socketio.emit('new_item', plant_info)

    new_data = {
        'plantId': plant_id,
        'type': new_type,
        'weight': float(new_weight),
        'address': new_address or 'Unknown',
        'reserved': int(plant_info['reserved'])  # Convert to 1/0 for MQTT
    }
    client.publish(topicServer, json.dumps(new_data))
    print(f"Published new plant to {topicServer}: {new_data}")
    return redirect(url_for('details', item_id=plant_id))

@app.route('/details/<item_id>', methods=['GET'])
def details(item_id):
    for item in items:
        if item['id'] == int(item_id):
            print(f"Rendering details for item {item_id}: {item['value']}")
            return render_template('details.html', item_id=item_id, value=item['value'])
    return "Item not found", 404

if __name__ == "__main__":
    socketio.run(app, host='0.0.0.0', port=5000, debug=True)