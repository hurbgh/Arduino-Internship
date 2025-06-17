//initialize screen
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH,SCREEN_HEIGHT,&Wire,OLED_RESET);

//defining ports to be used by the multiplexer
const int MUX_INH = 2;//port where inhibitor wire connects
const int UMUX_A = 4;
const int UMUX_B = 16;
const int UMUX_C = 17;
const int UART_EN = 15;

//defining ports to be used by the UART2
#define TX 18//the port where TX wire connects
#define RX 19//the port where RX wire connects
HardwareSerial multiplex(2);//using UART2
bool waitFor77=false;//this is a boolean variable for waiting for the next code to confirm it should start recording
bool safety=false;//this prevents the loop from happening again if the same starting number is encountered when getting data
int dataArray[32];//array to store all integers
int position=1;//integer variable to keep track of position in array
bool checkSum=false;//check sum checking boolean variable

int portTrack=0;//this is to know which port to test and changes to test each other port
bool done = false;//this is to keep track of progress collecting data from one port and switching to another port when ready

//integer variables to store all the data
int P19=0;
int P20=0;
int P21=0;
int P14=0;
int P15=0;
int P16=0;
int P17=0;
int P18=0;

//the following are for helping detect if a port does not have a sensor connected to it
unsigned long timeSinceStart;//starts recording time
unsigned long timeLimit;//this variable will store the end time since start of port switch for waiting, once crossed and not starting integers found it will rnotify the port has no sensor
bool newPort=true;//boolean variable to know when to record end time for timeLimit

//variables used to get CO2 data
byte getData[9]={0xFF,0x01,0x86,0x00,0x00,0x00,0x00,0x00,0x79};
byte turnOnCalib[9]={0xFF, 0x01, 0x79, 0xA0, 0x00, 0x00, 0x00, 0x00, 0xE6};
byte turnOffCalib[9]={0xFF, 0x01, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86};
bool decide=false;
bool askOnce=false;
String Choice="";

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  multiplex.begin(9600,SERIAL_8N1,RX,TX);//connection to the multiplexer
  
  dataArray[0]=66;//intitializing array to store pms7003 data
  
  if(!display.begin(SSD1306_SWITCHCAPVCC,0x3C)){
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  pinMode(MUX_INH,OUTPUT);
  pinMode(UMUX_A,OUTPUT);
  pinMode(UMUX_B,OUTPUT);
  pinMode(UMUX_C,OUTPUT);
  pinMode(UART_EN,OUTPUT);
  digitalWrite(MUX_INH,HIGH);
 
 
  digitalWrite(UMUX_C,HIGH);
  digitalWrite(UMUX_B,LOW);
  digitalWrite(UMUX_A,HIGH);
  digitalWrite(UART_EN,HIGH);
  digitalWrite(MUX_INH,LOW);
  
}

void loop() {
  // put your main code here, to run repeatedly:

    if (portTrack==8){
      done=true;
    }
    if (done==false){
      if (portTrack==3||portTrack==4||portTrack==7||portTrack==0){
        pmsSensor();
      }
      else if (portTrack==5||portTrack==6||portTrack==1||portTrack==2){
        co2Sensor();
      }
    }else{
      if (portTrack>=0&&portTrack<=7){
        portTrack++;
        done=false;
        portSelect(portTrack);
        while (multiplex.available()) { multiplex.read(); };
        waitFor77=false;
        safety=false;
        position=1;
        newPort=true;
      }else if (portTrack==8){
        display.clearDisplay();
        display.setCursor(0,0);
        display.print("PM2.5");
        display.setCursor(60,0);
        display.print("CO2");
        display.setCursor(0, 10);

        display.print("P14: ");
        if(P14!=-1&&P14!=-2){
          display.print(P14);
        }else if (P14==-1){
          display.print("CCF");
        }else if (P14==-2){
          display.print("CF");
        }

        display.setCursor(60, 10);
        display.print("P16: ");
        if (P16!=-1&&P16!=-2){
          display.print(P16);
        }else if (P16==-1){
          display.print("CCF");
        } else if (P16==-2){
          display.print("CF");
        }        
        


        display.setCursor(0, 20);
        display.print("P15: ");
        if(P15!=-1&&P15!=-2){
          display.print(P15);
        }else if (P15==-1){
          display.print("CCF");
        }else if (P15==-2){
          display.print("CF");
        }        
   
        display.setCursor(60, 20);
       
        display.print("P17: ");
        if(P17!=-1&&P17!=-2){
          display.print(P17);
        }else if (P17==-1){
          display.print("CCF");
        }else if (P17==-2){
          display.print("CF");
        }     
        display.setCursor(0, 30);
        
        display.print("P18: ");
        if(P18!=-1&&P18!=-2){
          display.print(P18);
        }else if (P18==-1){
          display.print("CCF");
        }else if (P18==-2){
          display.print("CF");
        }     
        display.setCursor(60, 30);
        display.print("P20: ");
        if (P20!=-1&&P20!=-2){
          display.print(P20);
        }else if (P20==-1){
          display.print("CCF");
        }else if (P20==-2){
          display.print("CF");
        }       

        
        display.setCursor(0, 40);
        display.print("P19: ");
        if(P19!=-1&&P19!=-2){
          display.print(P19);
        }else if (P19==-1){
          display.print("CCF");
        }else if (P19==-2){
          display.print("CF");
        }
        display.setCursor(60, 40);
       
        
        display.print("P21: ");
        if(P21!=-1&&P21!=-2){
          display.println(P21);
        }else if (P21==-1){
          display.println("CCF");
        }else if (P21==-2){
          display.println("CF");
        }
        display.println("CF=Connection Failed");
        display.println("CCF=Check Code Failed");
        display.display();
        portTrack=0;
        done=false;
        portSelect(portTrack);
        while (multiplex.available()) { multiplex.read(); }
        waitFor77=false;
        safety=false;
        position=1;
        newPort=true;
      }
    }
 // }
}
/*portTrack 0 = P19
  portTrack 1 = P20
  portTrack 2 = P21
  portTrack 3 = P14
  portTrack 4 = P15
  portTrack 5 = P16
  portTrack 6 = P17
  portTrack 7 = P18
  */

uint8_t uart_port[8][3] = {
    // C, B, A
    {HIGH, LOW, HIGH}, //portTrack 0
    {HIGH, HIGH, LOW}, //portTrack 1
    {HIGH, HIGH, HIGH},//portTrack 2
    {LOW, LOW, LOW},//portTrack 3
    {LOW, LOW, HIGH},//portTrack 4
    {LOW, HIGH, LOW},//portTrack 5
    {LOW, HIGH, HIGH},//portTrack 6
    {HIGH, LOW, LOW},//portTrack 7
};

void portSelect(int select){
  digitalWrite(MUX_INH,HIGH);
  
  digitalWrite(UMUX_C, uart_port[select][0]);
  digitalWrite(UMUX_B, uart_port[select][1]);
  digitalWrite(UMUX_A, uart_port[select][2]);

  digitalWrite(UART_EN,HIGH);
  digitalWrite(MUX_INH,LOW);
}
/*
PMS7003 ports are P14,P15,P18,P19
CO2 ports are P16,P17,P20,P21
*/
void pmsSensor(){
  if (newPort==true){
  timeSinceStart=millis();
  timeLimit=timeSinceStart+3000;
  newPort=false;
  }
  //timeSinceStart=millis();
  if(millis()>timeLimit&&safety==false){
    safety=false;
    position=1;
    done=true;
    newPort=true;
    waitFor77=false;
    if (portTrack==0){
      P19=-2;
    }else if(portTrack==1){
      P20=-2;
    }else if(portTrack==2){
      P21=-2;
    }else if (portTrack==3){
      P14=-2;
    }else if (portTrack==4){
      P15=-2;
    }else if (portTrack==5){
      P16=-2;
    }else if (portTrack==6){
      P17=-2;
    }else if (portTrack==7){
      P18=-2;
    }
  }
  if (multiplex.available()){
    int data =multiplex.read();  
    /*if (portTrack==3||portTrack==4){
      for (int i =0;i<32;i++){
        Serial.println(dataArray[i]);
      }
      Serial.print("Data: ");
      Serial.println(data);
      Serial.print("portTrack: ");
      Serial.println(portTrack);
      }*/
    if (data==66&&safety==false){//detected first starting byte but it might be a false alarm
      waitFor77=true;
    }
    else if (waitFor77==true&&safety==false){//if i am waiting for second byte and safety off then
      if (data==77){//if data is 77 then it is starting to send data! safety is a boolean value to prevent the loop from starting again when another 77 is detected!
        safety=true;//safety is turned on to prevent this if statement to overwrite the array
        waitFor77=false;//safety is on and I no longer need to wait for value to know start recording so I can turn this off
      }else{
        waitFor77=false;//if i do not get the next value reset the boolean condition
      }
    }
    if (safety==true&&position!=32){
      dataArray[position]=data;
      position++;
    }
      /*
    portTrack 0 = P19
    portTrack 1 = P20
    portTrack 2 = P21
    portTrack 3 = P14
    portTrack 4 = P15
    portTrack 5 = P16
    portTrack 6 = P17
    portTrack 7 = P18
    */
    if (position==32){
      checkSum=false;
      int checkCode=(dataArray[30]*256)+dataArray[31];
      int sumForCheck=0;
      for(int i =0;i<30;i++){
        sumForCheck=sumForCheck+dataArray[i];
      }
      display.clearDisplay();
      if (checkCode==sumForCheck){
        checkSum=true;
      }else{
        checkSum=false;
      }

      if (portTrack==0){//portTrack 0 = P19
        if (checkSum!=false){
          P19=(dataArray[6]*256)+dataArray[7];
          }else{
          P19=-1;
        }
      }else if (portTrack==1){//portTrack 1 = P20
        if(checkSum!=false){
          P20=(dataArray[6]*256)+dataArray[7];
        }else{
          P20=-1;
        }
      }else if (portTrack==2){//portTrack 2 = P21
        if(checkSum!=false){
          P21=(dataArray[6]*256)+dataArray[7];
          }else{
          P21=-1;
          }
      }else if (portTrack==3){//portTrack 3 = P14
        if(checkSum!=false){
          P14=(dataArray[6]*256)+dataArray[7];
        }else{
          P14=-1;
        }
      }else if (portTrack==4){//portTrack 4 = P15
        if(checkSum!=false){
          P15=(dataArray[6]*256)+dataArray[7];
        }else{
          P15=-1;
        }
        
      }else if (portTrack==5){//portTrack 5 = P16
        if (checkSum!=false){
          P16=(dataArray[6]*256)+dataArray[7];
        }else{
          P16=-1;
        }
        
      }else if (portTrack==6){//portTrack 6 = P17
        if(checkSum!=false){
          P17=(dataArray[6]*256)+dataArray[7];
        }else{
          P17=-1;
        }
        
      }else if (portTrack==7){//portTrack 7 = P18
        if(checkSum!=false){
          P18=(dataArray[6]*256)+dataArray[7];
        }else{
          P18=-1;
        }
        
      }
      safety=false;
      position=1;
      done=true;
      newPort=true;
    }
}}

void co2Sensor(){
  Serial.print("portTrack=");
  Serial.println(portTrack);
  decide=false;
  askOnce=false;
  while (decide==false){
    if (askOnce==false){
      Serial.println("Do you want to turn on self-calibration function? If yes enter y, else enter any key.");
      askOnce=true;
    }
    if (Serial.available()>0){
      Choice=Serial.readStringUntil('\n');
      decide=true;
    }
  }

  if (Choice=="y"){
    multiplex.write(turnOnCalib,9);
    Serial.println("choice A");
  }else{
    multiplex.write(turnOffCalib,9);
    Serial.println("choice B");
  }

  while (multiplex.available()) {
    multiplex.read();
  }
  delay(50);

  multiplex.write(getData,9);

  unsigned long startTime=millis();
  while (multiplex.available()<9){
    if (millis()-startTime>1500){
      Serial.println("TIMEOUT");
      if (portTrack==5){
        P16=-2;
      }else if (portTrack==6){
        P17=-2;
      }else if (portTrack==1){
        P20=-2;
      }else if (portTrack==2){
        P21=-2;}    
      while(multiplex.available()){
        multiplex.read();
      }
      delay(5000);
      done=true;
      newPort=true;
      return;
    }
    delay(10);
  }

  byte dataResponse[9];
  for (int i =0;i<9;i++){
    dataResponse[i]=multiplex.read();
  }

  //i have the data now verify data integrity
  byte checkSum =0;
  for (int i =1;i<8;i++){
    checkSum=checkSum+dataResponse[i];
  }
  for (int i =0;i<9;i++){
    Serial.println(dataResponse[i]);
  }
  checkSum=0xFF-checkSum;
  checkSum++;
  if (checkSum==int(dataResponse[8])){
    if (portTrack==5){
      P16=(int(dataResponse[2])*256)+int(dataResponse[3]);
    }else if (portTrack==6){
      P17=(int(dataResponse[2])*256)+int(dataResponse[3]);
    }else if (portTrack==1){
      P20=(int(dataResponse[2])*256)+int(dataResponse[3]);
    }else if (portTrack==2){
      P21=(int(dataResponse[2])*256)+int(dataResponse[3]);
    }
  }else{
    if (portTrack==5){
      P16=-1;
    }else if (portTrack==6){
      P17=-1;
    }else if (portTrack==1){
      P20=-1;
    }else if (portTrack==2){
      P21=-1;
    }    
  }

  done=true;
  newPort=true;
}