class PMS7003Sensor {
  private:
    int rxPin;
    int txPin;
    HardwareSerial& serialPort;
    bool waitFor77 = false;
    bool safety = false;
    int dataArray[32];
    int position = 1;

  public:
    PMS7003Sensor(HardwareSerial& port, int rx, int tx) 
      : serialPort(port), rxPin(rx), txPin(tx) {}

    void begin() {
      serialPort.begin(9600, SERIAL_8N1, rxPin, txPin);
      dataArray[0] = 66;
    }

    void readAndPrintPM25() {
      if (serialPort.available()) {
        int data = serialPort.read();

        if (data == 66 && !safety) {
          waitFor77 = true;
        } else if (waitFor77 && !safety) {
          if (data == 77) {
            safety = true;
            waitFor77 = false;
          } else {
            waitFor77 = false;
          }
        }

        if (safety && position != 32) {
          dataArray[position] = data;
          position++;
        }

        if (position == 32) {
          int checkCode = (dataArray[30] * 256) + dataArray[31];
          int sumForCheck = 0;
          for (int i = 0; i < 30; i++) {
            sumForCheck += dataArray[i];
          }

          Serial.println("----------------------------------------------------------");
          if (checkCode == sumForCheck) {
            Serial.println("The data has good integrity. Check code success.");
          } else {
            Serial.println("The data has bad integrity. Check code failed.");
          }

          for (int i =0;i<32;i++){
            Serial.println(dataArray[i]);
          }

          Serial.print("PM2.5 concentration unit μ g/m3 standard particle: ");
          Serial.println((dataArray[6] * 256) + dataArray[7]);
          Serial.println("----------------------------------------------------------");

          safety = false;
          position = 1;
        }
      }
    }
};

class MHZ16 {
  private:

  
  public:
    
};

#define RX 18
#define TX 5
HardwareSerial pmsConnect(1); // using UART1

PMS7003Sensor pmSensor(pmsConnect, RX, TX);

void setup() {
  Serial.begin(115200);
  pmSensor.begin();

  /*byte switchToPassive[7] = {0x42, 0x4D, 0xE1, 0x00, 0x00, 0x01, 0x70};
  pmsConnect.write(switchToPassive, 7);

  unsigned long startTime = millis();
  while (pmsConnect.available() < 8 && millis() - startTime < 1000) {
    // wait up to 1 second
  }

  byte response[8];
  for (int i = 0; i < 8 && pmsConnect.available(); i++) {
    response[i] = pmsConnect.read();
  }
  byte expectedResponse[8] = {0x42, 0x4D, 0x00, 0x04, 0xE1, 0x00, 0x01, 0x74};
  bool match = true;
  for (int i = 0; i < 8; i++) {
    if (response[i] != expectedResponse[i]) {
      match = false;
      break;
    }
  }

  if (match) {
    Serial.println("Successfully changed to passive mode!");
  } else {
    Serial.println("Failed to change to passive mode.");
  }*/
  
}

void loop() {
  //For PMS7003
  byte wakeUp[7]={0x42,0x4D,0xE4,0x00,0x01,0x01,0x74};//wakeup
  byte requestDataPM[7]={0x42,0x4D,0xE2,0x00,0x00,0x01,0x71};//ask to send data
  byte sleep[7]={0x42,0x4D,0xE4,0x00,0x00,0x01,0x73};//sleep

  pmsConnect.write(wakeUp, 7);
  delay(30000); // allow sensor to wake up

  pmsConnect.write(requestDataPM, 7);
  

  unsigned long timeLimit = millis() + 3000;
  while (millis() < timeLimit){
    pmSensor.readAndPrintPM25();
  }
  
  pmsConnect.write(sleep, 7);

  
}
