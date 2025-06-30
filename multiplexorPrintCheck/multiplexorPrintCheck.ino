#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define CF -1
#define CCF -2
#define STR(x) #x
#define STRINGIFY(x) STR(x)
#define TX 18//the port where TX wire connects
#define RX 19//the port where RX wire connects

Adafruit_SSD1306 display(SCREEN_WIDTH,SCREEN_HEIGHT,&Wire,OLED_RESET);
//defining ports to be used by the UART2
HardwareSerial multiplex(2);//using UART2
//defining ports to be used by the multiplexer
const int MUX_INH = 2;//port where inhibitor wire connects
const int UMUX_A = 4;
const int UMUX_B = 16;
const int UMUX_C = 17;
const int UART_EN = 15;//port where enable wire connects

enum Port {
  PORT_MIN=0,
  PORT_PM_P19 = PORT_MIN,
  PORT_CO2_P20 = 1,
  PORT_CO2_P21 = 2,
  PORT_PM_P14 = 3,
  PORT_PM_P15 = 4,
  PORT_CO2_P16 = 5,
  PORT_CO2_P17 = 6,
  PORT_PM_P18 = 7,
  PORT_MAX = 8
};

struct pmsSensor{
  //variables used by pmsSensor() to get PMS7003 data
  bool waitFor77=false;//when the program reads 66, the first starting byte, it gets ready for the next byte, if the next byte is 77 it then starts recording, else this variable becomes false
  bool safety=false;//this prevents the loop from happening again if the same starting number is encountered when getting data preventing bytes similar to starting bytes from causing the program to overwrite
  int dataArray[32];//array to store all integers
  int position=1;//integer variable to keep track of position in array
  bool checkSum=false;//check sum checking boolean variable, it is false if the calculated check sum does not equal the value it is supposed to be
  //the following are for helping detect if a port intended to be connected to a PMS7003 sensor does not have a sensor connected to it
  unsigned long timeSinceStart;//starts recording time
  unsigned long timeLimit;//this variable will store the end time since start of port switch for waiting, if current time is greater than max allowed and not starting integers found it will notify the port has no sensor
};

struct portIterate{
  enum Port portTrack = PORT_PM_P19;//this is to know which port to test and changes to test each other port
  bool done = false;//this is to keep track of progress collecting data from one port and switching to another port when ready
  int displayArray[8];//this array stores the values that will show on the display screen unlike the dataArray which stores all the data it got from a PMS7003 sensor
  bool newPort=true;//boolean variable to know when to record end time for timeLimit so that I do not restart the timeLimit and end up waiting forever for a port to send data
};

struct co2Sensor{
  //variables used to get CO2 data
  byte getData[9]={0xFF,0x01,0x86,0x00,0x00,0x00,0x00,0x00,0x79};//this byte array is used to tell the sensor to send data
  byte turnOnCalib[9]={0xFF, 0x01, 0x79, 0xA0, 0x00, 0x00, 0x00, 0x00, 0xE6};//this byte array is used to tell the sensor to turn on self calibration
  byte turnOffCalib[9]={0xFF, 0x01, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86};//this byte array is used to tell the sensor to turn off self calibration
  bool askForEach=false;//this is to make sure the user only got the question prompt once for each port
  bool decide=false;
  String Choice="";//the users choice is stored here
  bool co2PortChoice[4];
};

struct programContext{
  struct pmsSensor PMS;
  struct co2Sensor CO2;
  struct portIterate Iterate;
};
static struct programContext context;
//function declarations
void portSelect(int select);
void pmsSensor();
void co2Sensor();
void print_reading(char *title, int val);
void renderDisplay();

void setup() {
  Serial.begin(115200);
  multiplex.begin(9600,SERIAL_8N1,RX,TX);//connection to the multiplexer
  
  context.PMS.dataArray[0]=66;//intitializing array to store pms7003 data, i already know what the first data will be, the program depends on the second starting variable to start recording
  
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

  unsigned long timeAtStart;
  bool timeExceeded=false;
  Serial.println("If you want to maintain current settings and not change anyhting then just wait ten seconds.");
  for (int i=0;i<4;i++){//iterate through an array
    context.CO2.decide=false;//once the decision is made exit the while loop
    context.CO2.askForEach=false;//so that I do not ask more than one time for each port
    timeAtStart=millis();
    while (context.CO2.decide==false){
      if (context.CO2.askForEach==false){
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
        context.CO2.askForEach=true;
      }
      /*if the user gives no input for ten seconds the program will move on.
      this is intended to help customize which sensor for which port whille have
      self-calibration function on or off. if the user wants to maintain defaults just wait ten seconds*/
      if (millis()-timeAtStart>10000){
        timeExceeded=true;
        return;
      }
      if (Serial.available()>0){
        context.CO2.Choice=Serial.readStringUntil('\n');
        context.CO2.Choice.trim();
        context.CO2.decide=true;
      }
    }
    if (timeExceeded==true){
      return;
    }
    if (context.CO2.Choice.equalsIgnoreCase("y")){
      context.CO2.co2PortChoice[i]=true;
      Serial.println("User selected on");
    }else{
      context.CO2.co2PortChoice[i]=false;
      Serial.println("User Selected off");
    }
  }
  for (int i =0;i<4;i++){
    //Select port
    if (i==0){//P16
      portSelect(PORT_CO2_P16);
    }else if (i==1){//P17
      portSelect(PORT_CO2_P17);
    }else if(i==2){//P20
      portSelect(PORT_CO2_P20);
    }else{//P21
      portSelect(PORT_CO2_P21);
    }
    //based on value execute either turn on or turn off command
    if (context.CO2.co2PortChoice[i]==true){
      multiplex.write(context.CO2.turnOnCalib,9);
    }else{
      multiplex.write(context.CO2.turnOffCalib,9);
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
  if (context.Iterate.done==false){
    if (context.Iterate.portTrack==PORT_PM_P19||context.Iterate.portTrack==PORT_PM_P14||context.Iterate.portTrack==PORT_PM_P15||context.Iterate.portTrack==PORT_PM_P18){
      pmsSensor();//if currently at the ports intended for PMS sensors, use the function intended for those sensors
    }
    else if (context.Iterate.portTrack==PORT_CO2_P20||context.Iterate.portTrack==PORT_CO2_P21||context.Iterate.portTrack==PORT_CO2_P16||context.Iterate.portTrack==PORT_CO2_P17){
      co2Sensor();//if currently at the ports intended for CO2 sensors, use the function intended for those sensors
    }
  }else{
    context.Iterate.portTrack = static_cast<Port>(static_cast<int>(context.Iterate.portTrack) + 1);//when I haven't reached portTrack 8, iterate because I am not done getting data from each port
    if (context.Iterate.portTrack==PORT_MAX){
      context.Iterate.portTrack=PORT_MIN;
    }
    context.Iterate.done=false;//set done to false because I have to read a new port
    portSelect(context.Iterate.portTrack);//put the portTrack value into portSelect function so the code to get the multiplexor to switch to the next correct port gets executed
    while (multiplex.available()) { multiplex.read(); };//clear buffer
    //reset variables
    context.PMS.waitFor77=false;
    context.PMS.safety=false;
    context.PMS.position=1;
    context.Iterate.newPort=true;
    renderDisplay();
    portSelect(context.Iterate.portTrack);//resets portTrack and executes code to select the first port
    while (multiplex.available()) { multiplex.read(); }
  }
}

uint8_t uart_port[PORT_MAX][3] = {
    // C, B, A
    {HIGH, LOW, HIGH}, //PORT_PM_P19
    {HIGH, HIGH, LOW}, //PORT_CO2_P20
    {HIGH, HIGH, HIGH},//PORT_CO2_P21
    {LOW, LOW, LOW},//PORT_PM_P14
    {LOW, LOW, HIGH},//PORT_PM_P15
    {LOW, HIGH, LOW},//PORT_CO2_P16
    {LOW, HIGH, HIGH},//PORT_CO2_P17
    {HIGH, LOW, LOW},//PORT_PM_P18
};

void portSelect(int select){
  digitalWrite(MUX_INH,HIGH);
  
  digitalWrite(UMUX_C, uart_port[select][0]);
  digitalWrite(UMUX_B, uart_port[select][1]);
  digitalWrite(UMUX_A, uart_port[select][2]);

  digitalWrite(UART_EN,HIGH);
  digitalWrite(MUX_INH,LOW);
}

void pmsSensor(){
  if (context.Iterate.newPort==true){
  context.PMS.timeSinceStart=millis();
  context.PMS.timeLimit=context.PMS.timeSinceStart+1000;
  context.Iterate.newPort=false;
  }
  //timeSinceStart=millis();
  if(millis()>context.PMS.timeLimit&&context.PMS.safety==false){
    context.PMS.safety=false;
    context.PMS.position=1;
    context.Iterate.done=true;
    context.Iterate.newPort=true;
    context.PMS.waitFor77=false;
    context.Iterate.displayArray[context.Iterate.portTrack]=CF;
  }
  if (multiplex.available()){
    int data =multiplex.read();  
    if (data==66&&context.PMS.safety==false){//detected first starting byte but it might be a false alarm
      context.PMS.waitFor77=true;
    }
    else if (context.PMS.waitFor77==true&&context.PMS.safety==false){//if i am waiting for second byte and safety off then
      if (data==77){//if data is 77 then it is starting to send data! safety is a boolean value to prevent the loop from starting again when another 77 is detected!
        context.PMS.safety=true;//safety is turned on to prevent this if statement to overwrite the array
        context.PMS.waitFor77=false;//safety is on and I no longer need to wait for value to know start recording so I can turn this off
      }else{
        context.PMS.waitFor77=false;//if i do not get the next value reset the boolean condition
      }
    }
    if (context.PMS.safety==true&&context.PMS.position!=32){
      context.PMS.dataArray[context.PMS.position]=data;
      context.PMS.position++;
    }
    if (context.PMS.position==32){
      context.PMS.checkSum=false;
      int checkCode=(context.PMS.dataArray[30]*256)+context.PMS.dataArray[31];
      int sumForCheck=0;
      for(int i =0;i<30;i++){
        sumForCheck=sumForCheck+context.PMS.dataArray[i];
      }
      if (checkCode==sumForCheck){
        context.PMS.checkSum=true;
      }else{
        context.PMS.checkSum=false;
      }
      if (context.PMS.checkSum){
        context.Iterate.displayArray[context.Iterate.portTrack]=(context.PMS.dataArray[6]*256)+context.PMS.dataArray[7];
      }else{
        context.Iterate.displayArray[context.Iterate.portTrack]=CCF;
      }
      context.PMS.safety=false;
      context.PMS.position=1;
      context.Iterate.done=true;
      context.Iterate.newPort=true;
    }
}}

void co2Sensor(){
  while (multiplex.available()) {//clears the buffer before asking the CO2 sensor to send data
    multiplex.read();
  }
  multiplex.write(context.CO2.getData,9);//the CO2 sensor is finally asked for data after the self-calibration condition for the sensor has been decided by the user

  unsigned long startTime=millis();
  while (multiplex.available()<9){
    if (millis()-startTime>1000){
      context.Iterate.displayArray[context.Iterate.portTrack]=CF;   
      context.Iterate.done=true;
      context.Iterate.newPort=true;
      return;
    }

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
  checkSum=0xFF-checkSum;
  checkSum++;
  if (int(dataResponse[0])==255&&int(dataResponse[1])==134&&int(dataResponse[2])*256+int(dataResponse[3])<2001){
    /*this if statement above this comment is a bug fix, for some reason when CO2 sensors are unplugged, the port still sends data that gets read and returns either a 
    massive number or a check code failed error. this makes sure that the first two important values are present in the array for reading and the data is not greater than the largest
    possible. after this check it proceeds onwards*/
    if (checkSum==int(dataResponse[8])){
    context.Iterate.displayArray[context.Iterate.portTrack]=(int(dataResponse[2])*256)+int(dataResponse[3]);
    }else{
      context.Iterate.displayArray[context.Iterate.portTrack]=CCF;
    }
  }else{
    context.Iterate.displayArray[context.Iterate.portTrack]=CF;
  }
  context.Iterate.done=true;
  context.Iterate.newPort=true;
}

void print_reading(char *title, int val)
{
  display.print(title);
  if (val==CF){
    display.print("CF");
  }else if(val==CCF){
    display.print("CCF");
  }else{
    display.print(val);
  }
}

void renderDisplay(){
  display.clearDisplay();
  display.setCursor(0,0);
  display.print("PM2.5");
  display.setCursor(55,0);
  display.print("|");
  display.print("CO2");

  display.setCursor(0, 10);
  print_reading("P14:",context.Iterate.displayArray[PORT_PM_P14]);
  if (context.Iterate.portTrack==PORT_PM_P14){
    display.setCursor(49, 10);
    display.print("<");
  }
  display.setCursor(55, 10);
  display.print("|");
  print_reading("P16:",context.Iterate.displayArray[PORT_CO2_P16]);
  if (context.Iterate.portTrack==PORT_CO2_P16){
    display.setCursor(120, 10);
    display.print("<");
  }
  display.setCursor(0, 20);
  print_reading("P15:",context.Iterate.displayArray[PORT_PM_P15]);
  if (context.Iterate.portTrack==PORT_PM_P15){
    display.setCursor(49, 20);
    display.print("<");
  }
  display.setCursor(55, 20);
  display.print("|");
  print_reading("P17:",context.Iterate.displayArray[PORT_CO2_P17]);
  if (context.Iterate.portTrack==PORT_CO2_P17){
    display.setCursor(120, 20);
    display.print("<");
  }
  display.setCursor(0, 30);
  print_reading("P18:",context.Iterate.displayArray[PORT_PM_P18]);
  if (context.Iterate.portTrack==PORT_PM_P18){
    display.setCursor(49, 30);
    display.print("<");
  }
  display.setCursor(55, 30);
  display.print("|");
  print_reading("P20:",context.Iterate.displayArray[PORT_CO2_P20]);
  if (context.Iterate.portTrack==PORT_CO2_P20){
    display.setCursor(120, 30);
    display.print("<");
  }
  display.setCursor(0, 40);
  print_reading("P19:",context.Iterate.displayArray[PORT_PM_P19]);
  if (context.Iterate.portTrack==PORT_PM_P19){
    display.setCursor(49, 40);
    display.print("<");
  }
  display.setCursor(55, 40);
  display.print("|");
  print_reading("P21:",context.Iterate.displayArray[PORT_CO2_P21]);
  if (context.Iterate.portTrack==PORT_CO2_P21){
    display.setCursor(120, 40);
    display.print("<");
  }
  display.setCursor(0, 48);
  display.println("CF=Connection Failed");
  display.print("CCF=Check Code Failed");
  display.display();
}