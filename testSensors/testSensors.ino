#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)

#define CF -1   // Connection Failed
#define CCF -2  // Checksum Check Failed

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

enum pmSensorState{
  initial_state=0,
  read_data_looking_for_66=initial_state,
  wait_for_write_signal_77=1,
  write_data_to_array=2,
  process_data_from_array=3,
  mark_reading_as_done=4
};

enum co2SensorState{
  co2_initial_state=0,
  wait_for_buffer_length_9_bytes=co2_initial_state,
  write_buffer_to_array=1,
  process_data_from_array_co2=2,
  mark_reading_as_done_co2=3
};

struct pmsSensor{//this is all the variables that are used by the pmsSensor function
  //variables used by pmsSensor() to get PMS7003 data
  int dataArray[32];//array to store all integers
  int position=1;//integer variable to keep track of position in array
  bool checkSum=false;//check sum checking boolean variable, it is false if the calculated check sum does not equal the value it is supposed to be
  //the following are for helping detect if a port intended to be connected to a PMS7003 sensor does not have a sensor connected to it
  unsigned long timeSinceStart;//starts recording time
  unsigned long timeLimit;//this variable will store the end time since start of port switch for waiting, if current time is greater than max allowed and not starting integers found it will notify the port has no sensor
  enum pmSensorState pmStateSelect= read_data_looking_for_66;
};

struct portIterate{//these are all the variables used to get data from sensors connected to ports and keep track of which port the program is on currently and which one to go to next
  enum Port portTrack = PORT_PM_P19;//this is to know which port to test and changes to test each other port
  bool done = false;//this is to keep track of progress collecting data from one port and switching to another port when ready
  int displayArray[8];//this array stores the values that will show on the display screen unlike the dataArray which stores all the data it got from a PMS7003 sensor
  bool newPort=true;//boolean variable to know when to record end time for timeLimit so that I do not restart the timeLimit and end up waiting forever for a port to send data
};

struct co2Sensor{//this is all the vraibales used by the co2Sensor function
  //variables used to get CO2 data
  byte getData[9]={0xFF,0x01,0x86,0x00,0x00,0x00,0x00,0x00,0x79};//this byte array is used to tell the sensor to send data
  byte turnOnCalib[9]={0xFF, 0x01, 0x79, 0xA0, 0x00, 0x00, 0x00, 0x00, 0xE6};//this byte array is used to tell the sensor to turn on self calibration
  byte turnOffCalib[9]={0xFF, 0x01, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86};//this byte array is used to tell the sensor to turn off self calibration
  bool askForEach=false;//this is to make sure the user only got the question prompt once for each port
  bool decide=false;
  String Choice="";//the users choice is stored here
  bool co2PortChoice[4];
  enum co2SensorState co2StateSelect= wait_for_buffer_length_9_bytes;
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
        break; // Added break to exit while loop immediately
      }
      if (Serial.available()>0){
        context.CO2.Choice=Serial.readStringUntil('\n');
        context.CO2.Choice.trim();
        context.CO2.decide=true;
      }
    }
    if (timeExceeded==true){
      break; // Added break to exit for loop immediately
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
    delay(50); // Small delay after sending command
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
    context.PMS.position=1;
    context.Iterate.newPort=true;
    // context.PMS.timeLimit = 0; // Removed, timeout logic moved
    context.PMS.pmStateSelect=read_data_looking_for_66;
    renderDisplay();
    // No need to call portSelect again here as it's done above.
    // while (multiplex.available()) { multiplex.read(); } // Duplicative buffer clear, removed
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
  delay(10); // Give MUX time to settle
}

void pmsSensor(){
  int data = -1; // Declare data outside the switch to make it accessible in all cases
  int checkCode=0; // Declared here
  int sumForCheck=0; // Declared here
  
  if (multiplex.available()) {
    data = multiplex.read();
  }

  switch (context.PMS.pmStateSelect){
    case read_data_looking_for_66:
      if (context.Iterate.newPort==true){
        context.PMS.timeSinceStart=millis();
        context.PMS.timeLimit=context.PMS.timeSinceStart+1000; // Increased timeout to 5 seconds
        context.Iterate.newPort=false;
        context.PMS.position = 1; // Reset position
      }
      
      // If data is 0x42, transition
      if (data!=-1 && data==0x42){
        context.PMS.pmStateSelect=wait_for_write_signal_77;
      }
      // ELSE IF NO DATA AND TIMEOUT OCCURRED
      else if (data == -1 && millis() > context.PMS.timeLimit) {
        context.Iterate.displayArray[context.Iterate.portTrack]=CF;
        context.PMS.pmStateSelect = mark_reading_as_done;
      }
      // ELSE (data is -1 and no timeout, or data is wrong byte), stay in current state
      break;
    
    case wait_for_write_signal_77:
      // If data is 0x4D, transition
      if (data!=-1 && data==0x4D){
        context.PMS.pmStateSelect=write_data_to_array;
        context.PMS.dataArray[context.PMS.position]=data;
        context.PMS.position++; // Increment position
      }
      // ELSE IF NO DATA AND TIMEOUT OCCURRED
      else if (data == -1 && millis() > context.PMS.timeLimit) {
        context.Iterate.displayArray[context.Iterate.portTrack]=CF;
        context.PMS.pmStateSelect = mark_reading_as_done;
      }
      // ELSE (data is not -1 but not 0x4D) - this means a wrong byte after 0x42
      else if (data != -1) { 
        context.PMS.pmStateSelect=read_data_looking_for_66;
        context.PMS.position = 1; // Reset position
      }
      // ELSE (data is -1 and no timeout), stay in current state
      break;

    case write_data_to_array:
      // If data is valid and there's space in the array
      if (data != -1 && context.PMS.position < 32){
        context.PMS.dataArray[context.PMS.position]=data;
        context.PMS.position++;
        // Serial.print("PMS: Stored byte to position "); Serial.print(context.PMS.position - 1);
        // Serial.print(", Current Pos: "); Serial.println(context.PMS.position); // Debug print
      }
      // If array is full (32 bytes collected)
      if (context.PMS.position==32){
        context.PMS.pmStateSelect=process_data_from_array;
      }
      // NOTE: Timeout for full frame should be handled by an overall frame timeout
      // or if data stream stops, it will stall here and eventually be caught by the
      // general system timeout if that's implemented outside loop/sensor functions.
      // For now, only 66/77 initial timeouts are explicitly handled.
      break;

    case process_data_from_array:
      context.PMS.checkSum=false;
      checkCode=(context.PMS.dataArray[30]*256)+context.PMS.dataArray[31];
      sumForCheck=0;
      for(int i =0;i<30;i++){
        sumForCheck=sumForCheck+context.PMS.dataArray[i];
      }

      if (checkCode==sumForCheck){
        context.PMS.checkSum=true;
      }
      if (context.PMS.checkSum){
        context.Iterate.displayArray[context.Iterate.portTrack]=(context.PMS.dataArray[6]*256)+context.PMS.dataArray[7];
      }else{
        context.Iterate.displayArray[context.Iterate.portTrack]=CCF;
      }
      context.PMS.pmStateSelect=mark_reading_as_done;
      break;

    case mark_reading_as_done:
      context.PMS.position=1;
      context.Iterate.done=true;
      context.Iterate.newPort=true;
      context.PMS.pmStateSelect=read_data_looking_for_66;
      break;
  }
}

void co2Sensor(){

  while (multiplex.available()) {
    multiplex.read(); // Clear the buffer
  }
  multiplex.write(context.CO2.getData,9); // Request data

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