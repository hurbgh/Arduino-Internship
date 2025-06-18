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
const int UART_EN = 15;//port where enable wire connects

//defining ports to be used by the UART2
#define TX 18//the port where TX wire connects
#define RX 19//the port where RX wire connects
HardwareSerial multiplex(2);//using UART2

//variables used by pmsSensor() to get PMS7003 data
bool waitFor77=false;//when the program reads 66, the first starting byte, it gets ready for the next byte, if the next byte is 77 it then starts recording, else this variable becomes false
bool safety=false;//this prevents the loop from happening again if the same starting number is encountered when getting data preventing bytes similar to starting bytes from causing the program to overwrite
int dataArray[32];//array to store all integers
int position=1;//integer variable to keep track of position in array
bool checkSum=false;//check sum checking boolean variable, it is false if the calculated check sum does not equal the value it is supposed to be

int portTrack=0;//this is to know which port to test and changes to test each other port
bool done = false;//this is to keep track of progress collecting data from one port and switching to another port when ready

int displayArray[8];//this array stores the values that will show on the display screen unlike the dataArray which stores all the data it got from a PMS7003 sensor

//the following are for helping detect if a port does not have a sensor connected to it
unsigned long timeSinceStart;//starts recording time
unsigned long timeLimit;//this variable will store the end time since start of port switch for waiting, if current time is greater than max allowed and not starting integers found it will notify the port has no sensor
bool newPort=true;//boolean variable to know when to record end time for timeLimit so that I do not restart the timeLimit and end up waiting forever for a port to send data

//variables used to get CO2 data
byte getData[9]={0xFF,0x01,0x86,0x00,0x00,0x00,0x00,0x00,0x79};//this byte array is used to tell the sensor to send data
byte turnOnCalib[9]={0xFF, 0x01, 0x79, 0xA0, 0x00, 0x00, 0x00, 0x00, 0xE6};//this byte array is used to tell the sensor to turn on self calibration
byte turnOffCalib[9]={0xFF, 0x01, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86};//this byte array is used to tell the sensor to turn off self calibration
bool decide=false;//this boolean variable is used to keep track of whether the user has decided yet on turning off or on the self calibration
bool askForEach=false;//this is to make sure the user only got the question prompt once for each port
String Choice="";//the users choice is stored here
bool co2PortChoice[4];

void setup() {
  Serial.begin(115200);
  multiplex.begin(9600,SERIAL_8N1,RX,TX);//connection to the multiplexer
  
  dataArray[0]=66;//intitializing array to store pms7003 data, i already know what the first data will be, the program depends on the second starting variable to start recording
  
  if(!display.begin(SSD1306_SWITCHCAPVCC,0x3C)){//if the display fails to start it will notify the user
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

 /*
  for (int i=0;i<4;i++){//iterate through an array
    decide=false;//once the decision is made exit the while loop
    askForEach=false;//so that I do not ask more than one time for each port
    while (decide==false){
      if (askForEach==false){
        Serial.print("Do you want to turn on self-calibration function for MH-Z16 sensor connected to port ");
        if (i==0){
          Serial.print("P16? ");
        }else if (i==1){
          Serial.print("P17? ");
        }else if (i==2){
          Serial.print("P20? ");
        }else{
          Serial.print("P21? ");
        }
        Serial.println("If yes enter the key y, else enter any key.");
        askForEach=true;
      }
      if (Serial.available()>0){
        Choice=Serial.readStringUntil('\n');
        Choice.trim();
        decide=true;
      }
    }
    if (Choice.equalsIgnoreCase("y")){
      co2PortChoice[i]=true;
      Serial.println("User selected on");
    }else{
      co2PortChoice[i]=false;
      Serial.println("User Selected off");
    }
  }*/
  //add a time out for ten seconds for user decision
  //decrease delay to screen
  //read and then update instantly, indicate which port will be read next! show with an arrow

  for (int i =0;i<4;i++){
    //Select port
    if (i==0){//P16
      portSelect(5);
    }else if (i==1){//P17
      portSelect(6);
    }else if(i==2){//P20
      portSelect(1);
    }else{//P21
      portSelect(2);
    }
    //based on value execute either turn on or turn off command
    if (co2PortChoice[i]==true){
      multiplex.write(turnOnCalib,9);
    }else{
      multiplex.write(turnOffCalib,9);
    }
  }

  digitalWrite(MUX_INH,HIGH);
 
 
  digitalWrite(UMUX_C,HIGH);
  digitalWrite(UMUX_B,LOW);
  digitalWrite(UMUX_A,HIGH);
  digitalWrite(UART_EN,HIGH);//this will always be set to high
  digitalWrite(MUX_INH,LOW);
  
}

void loop() {
  if (portTrack==8){
    done=true;//this condition prevents the program from trying to read from portTrack 8, which doesn't exist and is used asa condition to print all data and reset
  }
  if (done==false){
    if (portTrack==3||portTrack==4||portTrack==7||portTrack==0){
      pmsSensor();//if currently at the ports intended for PMS sensors, use the function intended for those sensors
    }
    else if (portTrack==5||portTrack==6||portTrack==1||portTrack==2){
      co2Sensor();//if currently at the ports intended for CO2 sensors, use the function intended for those sensors
    }
  }else{
    if (portTrack>=0&&portTrack<=7){
      portTrack++;//when I haven't reached portTrack 8, iterate because I am not done getting data from each port
      done=false;//set done to false because I have to read a new port
      portSelect(portTrack);//put the portTrack value into portSelect function so the code to get the multiplexor to switch to the next correct port gets executed
      while (multiplex.available()) { multiplex.read(); };//clear buffer
      //reset variables
      waitFor77=false;
      safety=false;
      position=1;
      newPort=true;
    }else if (portTrack==8){
      display.clearDisplay();
      display.setCursor(0,0);
      display.print("PM2.5");
      display.setCursor(60,0);
      display.print("|");
      display.print("CO2");
      display.setCursor(0, 10);
      print_reading("P14: ",displayArray[3]);
      display.setCursor(60, 10);
      display.print("|");
      print_reading("P16: ",displayArray[5]);
      display.setCursor(0, 20);
      print_reading("P15: ",displayArray[4]);
      display.setCursor(60, 20);
      display.print("|");
      print_reading("P17: ",displayArray[6]);
      display.setCursor(0, 30);
      print_reading("P18: ",displayArray[7]);
      display.setCursor(60, 30);
      display.print("|");
      print_reading("P20: ",displayArray[1]);
      display.setCursor(0, 40);
      print_reading("P19: ",displayArray[0]);
      display.setCursor(60, 40);
      display.print("|");
      print_reading("P21: ",displayArray[2]);
      display.println("");
      display.println("CF=Connection Failed");
      display.println("CCF=Check Code Failed");
      display.display();
      portTrack=0;
      done=false;
      portSelect(portTrack);//resets portTrack and executes code to select the first port
      while (multiplex.available()) { multiplex.read(); }
      waitFor77=false;
      safety=false;
      position=1;
      newPort=true;
    }
  }
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
  timeLimit=timeSinceStart+1000;
  newPort=false;
  }
  //timeSinceStart=millis();
  if(millis()>timeLimit&&safety==false){
    safety=false;
    position=1;
    done=true;
    newPort=true;
    waitFor77=false;
    displayArray[portTrack]=-1;
  }
  if (multiplex.available()){
    int data =multiplex.read();  
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
      if (checkSum){
        displayArray[portTrack]=(dataArray[6]*256)+dataArray[7];
      }else{
        displayArray[portTrack]=-2;
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

  while (multiplex.available()) {//clears the buffer before asking the CO2 sensor to send data
    multiplex.read();
    delay(1);
  }
  //delay(50);

  multiplex.write(getData,9);//the CO2 sensor is finally asked for data after the self-calibration condition for the sensor has been decided by the user

  unsigned long startTime=millis();
  while (multiplex.available()<9){
    if (millis()-startTime>1000){
      Serial.println("TIMEOUT");
      displayArray[portTrack]=-1;   
      /*while(multiplex.available()){
        multiplex.read();
      }*/
      done=true;
      newPort=true;
      return;
    }
    //delay(10);
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
  if (int(dataResponse[0])==255&&int(dataResponse[1])==134&&int(dataResponse[2])*256+int(dataResponse[3])<2001){
    /*this if statement above this comment is a bug fix, for some reason when CO2 sensors are unplugged, the port still sends data that gets read and returns either a 
    massive number or a check code failed error. this makes sure that the first two important values are present in the array for reading and the data is not greater than the largest
    possible. after this check it proceeds onwards*/
    if (checkSum==int(dataResponse[8])){
    displayArray[portTrack]=(int(dataResponse[2])*256)+int(dataResponse[3]);
    }else{
      displayArray[portTrack]=-2;
    }
  }else{
    displayArray[portTrack]=-1;
  }
  done=true;
  newPort=true;
}

void print_reading(char *title, int val)
{
  display.print(title);
  if (val==-1){
    display.print("CF");
  }else if(val==-2){
    display.print("CCF");
  }else{
    display.print(val);
  }
}