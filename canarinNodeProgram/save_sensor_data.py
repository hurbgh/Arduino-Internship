import serial
import csv
import time
from datetime import datetime
import os

# File path in same directory as script
filename = os.path.join(os.path.dirname(__file__), "saveddata.csv")

# Open CSV file for appending
logfile = open(filename, mode='a', newline='')
writer = csv.writer(logfile)

# Connect to ESP32 USB serial (adjust COM port if needed)
try:
    ser = serial.Serial('COM3', 115200, timeout=1)
except Exception as e:
    print("Could not open COM port:", e)
    exit(1)

time.sleep(2)  # wait for ESP32 reset
ser.flushInput()

currentlyWriting = False
gotReady = False

def log_event(event_text):
    """Helper to log special events like restart/unplug."""
    current_time = datetime.now().strftime('%H:%M:%S')
    writer.writerow([current_time, f"*** {event_text} ***"])
    logfile.flush()
    print(event_text)

try:
    while True:
        ser_bytes = ser.readline()

        if not ser_bytes:
            continue

        decoded_bytes = ser_bytes.decode("utf-8", errors="ignore").strip()
        print(decoded_bytes)

        current_time = datetime.now().strftime('%H:%M:%S')

        # Control messages from ESP32
        if decoded_bytes == "getReady":
            gotReady = True
            log_event("ESP32 startup detected")
        elif decoded_bytes == "startWriting" and gotReady:
            currentlyWriting = True
            log_event("Logging started")

        # Sensor data
        elif currentlyWriting:
            writer.writerow([current_time, decoded_bytes])
            logfile.flush()

except KeyboardInterrupt:
    log_event("Logging finished by user")
    ser.close()
    logfile.close()
except Exception as e:
    # If serial port errors (like unplug), log it
    log_event(f"ESP32 disconnected or error: {e}")
    ser.close()
    logfile.close()
