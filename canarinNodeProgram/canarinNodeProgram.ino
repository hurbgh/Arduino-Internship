#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "BluetoothSerial.h"

String device_name = "Canarin Node";
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled!
#endif
#if !defined(CONFIG_BT_SPP_ENABLED)
#error Bluetooth SPP is not enabled.
#endif

BluetoothSerial SerialBT;

#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME280 bme;  // I2C
unsigned long delayTime;

class COSensor {
private:
  HardwareSerial& serialPort;
  int rxPin;
  int txPin;
public:
  COSensor(HardwareSerial& port, int rx, int tx) : serialPort(port), rxPin(rx), txPin(tx) {}
  void begin() { serialPort.begin(9600, SERIAL_8N1, rxPin, txPin); }
  int getCOData() {
    byte response[9];
    unsigned long timeout = millis() + 3000;
    while (serialPort.available() < 9 && millis() < timeout) { /* wait */ }
    if (serialPort.available() < 9) return 0;
    for (int i = 0; i < 9; i++) response[i] = serialPort.read();
    int checksum = 0;
    for (int i = 1; i < 8; i++) checksum += response[i];
    checksum = 0xFF - checksum + 1;
    if (checksum == response[8]) {
      return (response[2] << 8) + response[3];
    } else {
      return 0;
    }
  }
};

class PMS7003Sensor {
private:
  int rxPin;
  int txPin;
  HardwareSerial& serialPort;
public:
  PMS7003Sensor(HardwareSerial& port, int rx, int tx) : serialPort(port), rxPin(rx), txPin(tx) {}
  void begin() {
    serialPort.begin(9600, SERIAL_8N1, rxPin, txPin);
    serialPort.setTimeout(200);
  }
  String readFrame() {
  unsigned long t0 = millis();
  while (millis() - t0 < 500) {
    if (serialPort.available() >= 2) {
      int b1 = serialPort.read();
      if (b1 == 0x42) {
        int b2 = serialPort.read();
        if (b2 == 0x4D) {
          byte rest[30];
          size_t got = serialPort.readBytes(rest, 30);
          if (got == 30) {
            byte frame[32];
            frame[0] = 0x42; frame[1] = 0x4D;
            for (int i = 0; i < 30; i++) frame[2 + i] = rest[i];

            int sum = 0;
            for (int i = 0; i < 30; i++) sum += frame[i];
            int checkCode = (frame[30] << 8) + frame[31];

            if (sum == checkCode) {
              // Use atmospheric environment values
              int pm1  = (frame[10] << 8) + frame[11];
              int pm25 = (frame[12] << 8) + frame[13];
              int pm10 = (frame[14] << 8) + frame[15];
              return String(pm1) + "," + String(pm25) + "," + String(pm10);
            } else {
              Serial.println("PMS7003 checksum failed");
              return "0,0,0";
            }
          }
        }
      }
    }
  }
  return "0,0,0";
}

};

class MHZ16 {
private:
  HardwareSerial& serialPort;
  int rxPin;
  int txPin;
  byte dataResponse[9];
  int checkSum = 0;
public:
  MHZ16(HardwareSerial& port, int rx, int tx) : serialPort(port), rxPin(rx), txPin(tx) {}
  void begin() { serialPort.begin(9600, SERIAL_8N1, rxPin, txPin); }
  int getCO2Data() {
    // Do not flush here; wait for bytes to arrive
    unsigned long timeout = millis() + 3000;
    while (serialPort.available() < 9 && millis() < timeout) { /* wait */ }
    if (serialPort.available() < 9) {
      Serial.println("Timeout waiting for CO2 data");
      return 0;
    }
    for (int i = 0; i < 9; i++) dataResponse[i] = serialPort.read();
    if (dataResponse[0] != 0xFF || dataResponse[1] != 0x86) {
      Serial.println("Invalid CO2 frame header");
      return 0;
    }
    checkSum = 0;
    for (int i = 1; i < 8; i++) checkSum += dataResponse[i];
    checkSum = 0xFF - checkSum + 1;
    checkSum &= 0xFF;
    if (checkSum == dataResponse[8]) {
      return (dataResponse[2] << 8) + dataResponse[3];
    } else {
      Serial.println("CO2 checksum failed");
      return 0;
    }
  }
};

// PMS7003 UART1
#define RX 18
#define TX 5
HardwareSerial pmsConnect(1);
PMS7003Sensor pmSensor(pmsConnect, RX, TX);

// MH-Z16 + ZE-07 share UART2
#define CO2_RX 16
#define CO2_TX 17
#define CO_RX 14
#define CO_TX 27
HardwareSerial sharedSerial(2);
MHZ16 co2Sensor(sharedSerial, CO2_RX, CO2_TX);
COSensor coConnect(sharedSerial, CO_RX, CO_TX);

String pmData;
int co2Data = 0;
int coData = 0;
float temp = 0;
float humidity = 0;
float altitude = 0;
float airPressure = 0;

void setup() {
  Serial.begin(115200);
  pmSensor.begin();

  // BME280
  unsigned status = bme.begin(0x76);
  delayTime = 1000;

  // MH-Z16 init + disable auto-cal
  sharedSerial.begin(9600, SERIAL_8N1, CO2_RX, CO2_TX);
  byte turnOffSelfCalibration[9] = {0xFF,0x01,0x79,0x00,0x00,0x00,0x00,0x00,0x86};
  sharedSerial.write(turnOffSelfCalibration, 9);

  Serial.println("getReady");

  // PMS7003 switch to passive and read reply properly
  byte switchToPassive[7] = {0x42,0x4D,0xE1,0x00,0x00,0x01,0x70};
  pmsConnect.write(switchToPassive, 7);

  // Read 8-byte response with timeout
  pmsConnect.setTimeout(200);
  byte response[8] = {0};
  size_t got = pmsConnect.readBytes(response, 8);

  byte expectedResponse[8] = {0x42,0x4D,0x00,0x04,0xE1,0x00,0x01,0x74};
  bool match = (got == 8);
  for (int i = 0; i < 8 && match; i++) {
    if (response[i] != expectedResponse[i]) match = false;
  }
  if (match) {
    Serial.println("Successfully changed to passive mode!");
  } else {
    Serial.println("Failed to change to passive mode.");
  }

  // Put ZE-07 to passive on its RX/TX pair
  sharedSerial.begin(9600, SERIAL_8N1, CO_RX, CO_TX);
  byte setCOSensorToPassive[9] = {0xFF,0x01,0x78,0x41,0x00,0x00,0x00,0x00,0x46};
  sharedSerial.write(setCOSensorToPassive, 9);

  SerialBT.begin(device_name);
  Serial.printf("The device with name \"%s\" is started.\nNow you can pair it with Bluetooth!\n", device_name.c_str());
  Serial.println("startWriting");
}

void airValues() {
  temp = bme.readTemperature();
  airPressure = bme.readPressure() / 100.0F;
  altitude = bme.readAltitude(SEALEVELPRESSURE_HPA);
  humidity = bme.readHumidity();
}

void loop() {
  // PMS7003 passive request and read
  byte requestDataPM[7] = {0x42,0x4D,0xE2,0x00,0x00,0x01,0x71};
  pmsConnect.write(requestDataPM, 7);
  delay(80); // allow response to start
  // Wait briefly for full frame to be present
  unsigned long pmWaitEnd = millis() + 200;
  while (pmsConnect.available() < 32 && millis() < pmWaitEnd) { /* wait */ }
  pmData = pmSensor.readFrame();
  if (pmData == "0,0,0") {
    // quick retry once
    delay(50);
    pmData = pmSensor.readFrame();
  }

  // MH-Z16 CO2
  sharedSerial.begin(9600, SERIAL_8N1, CO2_RX, CO2_TX);
  byte requestCO2Data[9] = {0xFF,0x01,0x86,0x00,0x00,0x00,0x00,0x00,0x79};
  sharedSerial.write(requestCO2Data, 9);
  delay(80); // give sensor time to respond
  co2Data = co2Sensor.getCO2Data();

  // BME280
  airValues();
  delay(delayTime);

  // ZE-07 CO
  sharedSerial.begin(9600, SERIAL_8N1, CO_RX, CO_TX);
  byte requestCOData[9] = {0xFF,0x01,0x86,0x00,0x00,0x00,0x00,0x00,0x79};
  sharedSerial.write(requestCOData, 9);
  delay(50);
  coData = coConnect.getCOData();

  // Output
  String sensorData = pmData + "," + String(co2Data) + "," + String(coData) + "," +
                      String(temp) + "," + String(humidity) + "," +
                      String(airPressure) + "," + String(altitude);

  SerialBT.println(sensorData);
  Serial.println(sensorData);

  // One-second cadence
  delay(1000);
}
