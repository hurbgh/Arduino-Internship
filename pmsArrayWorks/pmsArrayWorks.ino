#define TX 19//this number should be the port where RX wire connect
#define RX 18////this number should be the port where TX wire connect
HardwareSerial pmsConnect(1);//using UART1
bool waitFor77=false;//this is a boolean variable for waiting for the next code to confirm it should start recording
bool safety=false;//this boolean variable becomes true to prevent the if statement from restarting writing to array because one of the values is 66
int dataArray[32];//array to store all integers
int position=1;//this is is to iterate over the array and store information in the approproate position

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pmsConnect.begin(9600,SERIAL_8N1,RX,TX);
  dataArray[0]=66;
}

void loop() {
  // put your main code here, to run repeatedly:
  if (pmsConnect.available()){
    int data =pmsConnect.read();
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
    if (position==32){
      int checkCode=(dataArray[30]*256)+dataArray[31];
      int sumForCheck=0;
      for(int i =0;i<30;i++){
        sumForCheck=sumForCheck+dataArray[i];
      }
      Serial.println("----------------------------------------------------------");
      if (checkCode==sumForCheck){
        Serial.println("The data has good integrity. Check code success.");
      } else{
        Serial.println("The data has bad integrity. Check code failed.");
      }


      Serial.print("PM2.5 concentration unit μ g/m3 standard particle: ");
      Serial.println((dataArray[6]*256)+dataArray[7]);

      
      Serial.println("----------------------------------------------------------");

      safety=false;
      position=1;
    }
  }
}
