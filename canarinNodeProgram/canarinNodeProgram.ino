#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME280 bme; // I2C

unsigned long delayTime;

class COSensor{
  private:
    HardwareSerial& serialPort;
    int rxPin;
    int txPin;
  public:
    COSensor(HardwareSerial& port, int rx, int tx) 
      : serialPort(port), rxPin(rx), txPin(tx) {}
    
    void begin(){
      serialPort.begin(9600, SERIAL_8N1, rxPin, txPin);
    }

    int getCOData(){
      byte response[9];
      unsigned long timeout = millis() + 3000;
      while (serialPort.available() < 9 && millis() < timeout);
      for (int i = 0; i < 9; i++) {
        response[i] = serialPort.read();
      }
      int checksum = 0;
      for (int i = 1; i < 8; i++) {
        checksum += response[i];
      }
      checksum = 0xFF - checksum + 1;
      if (checksum == response[8]) {
      int co_ppm = (response[2] << 8) + response[3];
      return co_ppm;
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

    int getPMData() {
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

          
          if (checkCode == sumForCheck) {
            return (dataArray[6] * 256) + dataArray[7];
          } else {
            return 0;
          }    

          safety = false;
          position = 1;
        }else{
          return 0;
        }
      }
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
    MHZ16(HardwareSerial& port, int rx, int tx)
      : serialPort(port), rxPin(rx), txPin(tx) {}

    void begin() {
      serialPort.begin(9600, SERIAL_8N1, rxPin, txPin);
    }

    int getCO2Data() {
  while (serialPort.available()) serialPort.read(); // flush buffer

  unsigned long timeout = millis() + 3000;
  while (serialPort.available() < 9 && millis() < timeout);

  if (serialPort.available() < 9) {
    Serial.println("Timeout waiting for CO2 data");
    return 0;
  }

  for (int i = 0; i < 9; i++) {
    dataResponse[i] = serialPort.read();
  }

  if (dataResponse[0] != 0xFF || dataResponse[1] != 0x86) {
    Serial.println("Invalid CO2 frame header");
    return 0;
  }

  checkSum = 0;
  for (int i = 1; i < 8; i++) checkSum += dataResponse[i];
  checkSum = 0xFF - checkSum + 1;
  checkSum &= 0xFF;

  if (checkSum == dataResponse[8]) {
    int co2Data = (dataResponse[2] << 8) + dataResponse[3];
    return co2Data;
  } else {
    Serial.println("CO2 checksum failed");
    return 0;
  }
}

};


//PMS7003
#define RX 18
#define TX 5
HardwareSerial pmsConnect(1); // using UART1
PMS7003Sensor pmSensor(pmsConnect, RX, TX);

//MH-Z16
#define CO2_RX 16
#define CO2_TX 17
HardwareSerial sharedSerial(2); // UART2
MHZ16 co2Sensor(sharedSerial, CO2_RX, CO2_TX);

//ZE-07
#define CO_RX 14
#define CO_TX 27
COSensor coConnect(sharedSerial,CO_RX,CO_TX);//MH-Z16 and ZE-07 share the same hardware serial port!

void setup() {
  Serial.begin(115200);
  pmSensor.begin();

  unsigned status;
    
  // default settings
  status = bme.begin(0x76);
  delayTime = 1000;

  sharedSerial.begin(9600, SERIAL_8N1, CO2_RX, CO2_TX);
  //turning off self calibration in MH-Z16
  byte turnOffSelfCalibration[9]={0xFF, 0x01, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86};
  sharedSerial.write(turnOffSelfCalibration,9);


  //Making PMS7003 switch to passive mode
  byte switchToPassive[7] = {0x42, 0x4D, 0xE1, 0x00, 0x00, 0x01, 0x70};
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
  }
  
  sharedSerial.begin(9600, SERIAL_8N1, CO_RX, CO_TX);
  byte setCOSensorToPassive[9]={0xFF,0x01,0x78,0x41,0x00,0x00,0x00,0x00,0x46};
  sharedSerial.write(setCOSensorToPassive,9);

}

int pmData=0;
int co2Data=0;
int coData=0;
float temp=0;
float humidity=0;
float altitude=0;
float airPressure=0;

void loop() {
  //For PMS7003
  byte wakeUp[7]={0x42,0x4D,0xE4,0x00,0x01,0x01,0x74};//wakeup
  byte requestDataPM[7]={0x42,0x4D,0xE2,0x00,0x00,0x01,0x71};//ask to send data
  byte sleep[7]={0x42,0x4D,0xE4,0x00,0x00,0x01,0x73};//sleep

  //Serial.println("Now reading PMS7003");
  while (sharedSerial.available()) {
  sharedSerial.read();//clear buffer
  }

  pmsConnect.write(wakeUp, 7);
  delay(30000); // allow sensor to wake up

  while (pmsConnect.available()) {
  pmsConnect.read();//clear buffer first before sending request
  }

  pmsConnect.write(requestDataPM, 7);//request sent
  delay(100);

  unsigned long timeLimit = millis() + 3000;
  while (millis() < timeLimit){
    pmData=pmSensor.getPMData();//read data
  }

  pmsConnect.write(sleep,7);//go to sleep to save battery
  //Serial.println("Now reading MH-Z16");
  sharedSerial.begin(9600, SERIAL_8N1, CO2_RX, CO2_TX);

  while (sharedSerial.available()) {
  sharedSerial.read();//clear buffer
  }
  
  byte requestCO2Data[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
  sharedSerial.write(requestCO2Data, 9);//request data
  co2Data=co2Sensor.getCO2Data();//read data
  //Serial.println("Now reading BME280");
  //BME280
  airValues();
  delay(delayTime);

  while (sharedSerial.available()) sharedSerial.read(); // clear buffer
  //Serial.println("Now reading ZE-07");
  //For ZE-07
  sharedSerial.begin(9600, SERIAL_8N1, CO_RX, CO_TX);
  byte requestCOData[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
  sharedSerial.write(requestCOData,9);
  coData=coConnect.getCOData();
  
  String sensorData = String(pmData) + "," +
                    String(co2Data) + "," +
                    String(coData) + "," +
                    String(temp) + "," +
                    String(humidity) + "," +
                    String(airPressure) + "," +
                    String(altitude);

  Serial.println(sensorData);

}

void airValues() {
  
  temp = bme.readTemperature();
  airPressure = bme.readPressure() / 100.0F;
  altitude = bme.readAltitude(SEALEVELPRESSURE_HPA);
  humidity = bme.readHumidity();


  /*
    Serial.print("Temperature = ");
    Serial.print(bme.readTemperature());
    Serial.println(" °C");

    Serial.print("Pressure = ");

    Serial.print(bme.readPressure() / 100.0F);
    Serial.println(" hPa");

    Serial.print("Approx. Altitude = ");
    Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
    Serial.println(" m");

    Serial.print("Humidity = ");
    Serial.print(bme.readHumidity());
    Serial.println(" %");

    Serial.println();
    return bme.readTemperature(),bme.readPressure() / 100.0F,bme.readAltitude(SEALEVELPRESSURE_HPA),bme.readHumidity();*/
}

