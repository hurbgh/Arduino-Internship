import serial
import csv
from datetime import datetime

logging = open('logging.csv',mode='a', newline='')
writer = csv.writer(logging,delimiter=",", escapechar=' ', quoting=csv.QUOTE_NONE)

ser=serial.Serial('COM3',115200)
ser.flushInput()

ser.write(bytes('x','utf-8'))
currentlyWriting=False
recievedAlready=False

try:
    while True:
        ser_bytes=ser.readline()
        print(ser_bytes)

        decoded_bytes = ser_bytes.decode("utf-8").strip()
        print(decoded_bytes)

        c=datetime.now()
        current_time=c.strftime('%H:%M:%S')
        print(current_time)
        if (decoded_bytes=="getReady"):
            print("Going to start writing logs to logging csv file.")
            
            recievedAlready=True
        if (decoded_bytes=="startWriting"):
            currentlyWriting=True
        if (decoded_bytes=="getReady"and recievedAlready==True):
            currentlyWriting=False
            print("ESP32 Reboot detected. Continuing to to collect logs after startup finished.")
        if (currentlyWriting==True):
            if (decoded_bytes!="startWriting"):
                writer.writerow([current_time,decoded_bytes])
except:
    ser.close()
    logging.close()
    print("logging finished")    