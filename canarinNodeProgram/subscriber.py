import paho.mqtt.client as mqtt
from datetime import datetime
import os

# 1. HiveMQ Cloud Connection Details
BROKER = "dbb1e064fb494148b791a3bbed394a13.s1.eu.hivemq.cloud" # Get this from HiveMQ Console
PORT = 8883                                     # Secure Port
TOPIC = "interlab/node/bluetooth/38182BB39C56/data"
USERNAME = "Irfan"                      # The credential you created
PASSWORD = "XT30nR2d1qE9Hm"

LOGFILE = "outdoor_thirty_meters_1.log"

def on_connect(client, userdata, flags, reason_code, properties=None):
    if reason_code == 0:
        # Removed the emoji here to prevent the 'charmap' error
        print(f"CONNECTED: Successfully linked to HiveMQ Cloud.")
        print(f"LOGGING TO: {LOGFILE}")
        client.subscribe(TOPIC)
    else:
        print(f"CONNECTION ERROR: Failed with code {reason_code}")

def on_message(client, userdata, msg):
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    payload = msg.payload.decode("utf-8", errors="ignore")
    
    line = f"[{timestamp}] {msg.topic}: {payload}"
    print(line) # Your terminal should handle text just fine

    with open(LOGFILE, "a") as f:
        f.write(line + "\n")

# 2. Setup the Client with SSL/TLS and Auth
client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
client.on_connect = on_connect
client.on_message = on_message

# REQUIRED for HiveMQ Cloud:
client.tls_set() 
client.username_pw_set(USERNAME, PASSWORD)

print(f"Attempting to connect to {BROKER}...")
client.connect(BROKER, PORT, 60)

# 3. Start the loop
try:
    client.loop_forever()
except KeyboardInterrupt:
    print("\nStopping subscriber...")