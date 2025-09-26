#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include <BH1750.h>

//Pin LoRa RA-02
#define LORA_NSS 10
#define LORA_RST 9
#define LORA_DIO0 2

#define LED 7

const byte masterAddr = 0x01;
const byte loraAddr[] = {0x02, 0x03};
const int thisLoraAddr = 0;

unsigned long previousMillisRestartLORA = 0;
const long intervalRestartLORA = 5000;

String incomingMessage = "";
String message = "";             
char separator = ',';

bool isLedOn = false;
bool lastLedState = false;
bool isSensorOn = false;

String Slux;  
BH1750 lightMeter;

//Light sensor lux threshold
const unsigned long LED_ON_THRESHOLD = 400.0;  
const unsigned long LED_OFF_THRESHOLD = 1000.0; 

// Biến cho Moving Average Filter
const int WINDOW_SIZE = 10;
float luxReadings[WINDOW_SIZE];
int readingIndex = 0;

//Sensor Data
unsigned long sensorDelayTimer = 0;
int sensorDelayTime = 10000;
unsigned long previousMillis = 0;
const long interval = 10;
unsigned long averageLux = 0;
bool isProcessingDone = false; 

//Battery Data
double r1 = 1000; //Enter your hardware resistor value
double r2 = 1000;
double vcc = 8.21; //Enter max battery capacity
float minBattery = 6.6; //Enter min battery capacity
float batteryPercentage = 0.018; //Pre caculated voltage for 1% battery level
#define batteryPin A0
int batteryLevel = 50;
int batteryReadCount = 10;


void setup() 
{
  Serial.begin(9600);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, isLedOn ? HIGH : LOW);
  Wire.begin();    
  
  if (lightMeter.begin()) 
  {
    Serial.println("Light sensor BH1750 is ready ");
  } 
  else 
  {
    Serial.println("Light sensor error");
  }
  while (!Serial);
  ResetLora();
  sensorDelayTimer = millis();

}

void loop() 
{
  delay(50);
  
  OnReceive(LoRa.parsePacket());
  LoraResetHandler();
  if(isSensorOn)
  {
    ReadLightLevel();
  }
}

//******************************LED HANDLER**************************
void ProcessingLedData()
{
  //Serial.println("Arduino Message Receive " + String(isLedOn));
  digitalWrite(LED, isLedOn ? HIGH : LOW);
}

String GetValue(String data, int index) 
{
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;
  
  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
//*****************************END LED HANDLER**********************



//******************************LORA HANDLER*****************************
void SendLora(String Outgoing, byte Destination) {
  LoRa.beginPacket();             //--> start packet
  LoRa.write(Destination);        //--> add destination address
  LoRa.write(loraAddr[thisLoraAddr]);        //--> add sender address
  LoRa.write(Outgoing.length());  //--> add payload length
  LoRa.print(Outgoing);           //--> add payload
  LoRa.endPacket();               //--> finish packet and send it
}

void OnReceive(int packetSize) {
  if (packetSize == 0) return;          // if there's no packet, return

  //Serial.println("Receive LORA");
  //read packet header bytes:
  int recipient = LoRa.read();        //--> recipient address
  byte sender = LoRa.read();          //--> sender address
  byte incomingLength = LoRa.read();  //--> incoming msg length

  //Condition that is executed if message is not from Master.
  if (sender != masterAddr) {
    //Serial.println(sender);
    Serial.println("Not from Master, Ignore");
  	Serial.println(sender);
    // Resets the value of the Count_to_Rst_LORA variable.
    return;
  }

  // Clears Incoming variable data.
  incomingMessage = "";

  //Get all incoming data.
  while (LoRa.available()) {
    incomingMessage += (char)LoRa.read();
  }


  //Check length for error.
  if (incomingLength != incomingMessage.length()) {
    Serial.println();
    Serial.println("er"); //--> "er" = error: message length does not match length.
    //Serial.println("error: message length does not match length");
    return;
  }

  //Checks whether the incoming data or message for this device.
  if (recipient != loraAddr[thisLoraAddr]) 
  {
    Serial.println("This message is not for me.");
    return; 
  } 
  else 
  {
    // if message is for this device, or broadcast, print details:
    Serial.println();
    //Serial.println("Rc from: 0x" + String(sender, HEX));
    //Serial.println("Message: " + incomingMessage);

    // Calls the Processing_incoming_data() subroutine.
    ProcessingIncomingData(sender);
  }
}

void ProcessingIncomingData(byte sender) 
{  
  //Cú pháp tách dữ liệu lora isSensorOn,isLedOn
  isSensorOn = GetValue(incomingMessage, 1).toInt();
  

  Serial.println("******************************");
  Serial.println("Rc from : 0x" + String(sender));
  Serial.println("Message: " + incomingMessage);

  if(!isSensorOn)
  {
    isLedOn = GetValue(incomingMessage, 0).toInt();
    ProcessingLedData();
    LoraMessage();
  }
  else 
  {
    LoraMessage();
  }
}

void LoraMessage()
{
  ReadBattery();
  String ledData = isLedOn ? "1" : "0"; 
  message = String(isLedOn) + ',' + String(batteryLevel);
  Serial.println("Send Message: " + message);
  SendLora(message, masterAddr);
}


//Lora freezes after a while => Reset Lora
void LoraResetHandler()
{
  unsigned long currentMillisRestartLORA = millis();  
  if (currentMillisRestartLORA - previousMillisRestartLORA >= intervalRestartLORA) 
  {
    previousMillisRestartLORA = currentMillisRestartLORA;
    LoRa.end();
    ResetLora();
  }
}


void ResetLora() 
{
   pinMode(LORA_NSS, OUTPUT);
  LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);

 //Serial.println();
  Serial.println("Restart LoRa...");
  //Serial.println("Start LoRa init...");
  while (!LoRa.begin(433E6)) 
  {
    Serial.println("Starting LoRa failed!");
    delay(100);
  }
  //Serial.println("LoRa init succeeded.");
}

void ReadLightLevel()
{
  unsigned long currentMillis, currentLux, sum;
  if (!isProcessingDone) 
  {
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval) 
    {

      previousMillis = currentMillis;

      currentLux = lightMeter.readLightLevel();

      Serial.print(readingIndex + 1);
      Serial.print(": ");
      Serial.print(currentLux);
      Serial.println(" lux");
        
      luxReadings[readingIndex] = currentLux;
      readingIndex++;

      if (readingIndex >= WINDOW_SIZE) 
      {
        isProcessingDone = true;
        sum = 0;

        //Calculate average lux 
        for (int i = 0; i < WINDOW_SIZE; i++) 
        {
          sum += luxReadings[i];
        }
        averageLux = sum / WINDOW_SIZE;
        controlLed(averageLux);
        Serial.print("Average Lux ");
        Serial.println(averageLux);
      }
    }
  } 
  else 
  {
    if(millis() - sensorDelayTimer < sensorDelayTime) return;

    sensorDelayTimer = millis();
    isProcessingDone = false;
    readingIndex = 0;
    Serial.println("\n--- New sensor reading process ---\n");
  }
}

void controlLed(unsigned long lux) 
{
    if (lux > LED_OFF_THRESHOLD) 
    {
      digitalWrite(LED, LOW);
      isLedOn = false;
      Serial.println("Tắt đèn.");
    }
    else if (lux < LED_ON_THRESHOLD) 
    {
      digitalWrite(LED, HIGH);
      isLedOn = true;
      Serial.println("Bật đèn.");
    }
    lastLedState = isLedOn;
}

//***************************END LORA HANDLER****************************

//**************************** BATTERY HANDLER*****************************
void ReadBattery()
{
  int sumBattery = 0;
  for(int i =0; i < batteryReadCount; i++)
  {
    float readValue = analogRead(batteryPin);

    double vOut = readValue * (double)0.0048875855;
    double batteryVoltage = vOut / (double)(r2 /(r1 + r2));
    //Serial.println("Vout " + String(vOut));
  
    sumBattery += ((batteryVoltage-minBattery) / batteryPercentage);
  }

  //Serial.print("Battery Level: " + String(batteryLevel));
  batteryLevel = sumBattery / batteryReadCount;
  Serial.println("Battery: " + batteryLevel);
  
}

