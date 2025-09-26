#include <WiFi.h>
#include <Wire.h>
#include <FirebaseESP32.h>
#include <LoRa.h>
#include <SPI.h>
#include <TimeLib.h>

//Change WIFI and Password before compiling
#define WIFI_SSID *****************
#define WIFI_PASSWORD *********

#define FIREBASE_HOST ********************
#define FIREBASE_AUTH ********************

//De
#define ss 5
#define rst 14
#define dio0 2

FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;
String dataSize = "tiny";
ulong lastDataReadMillis = 0;

//setup read rate
const int readDataLimit = 1000;

// On - OFF
const String ledStateLocation[] = { "/devices/4xxArxOlk1MSKPQ0eMGT/power/isLedOn", "/devices/zzeDW8HAAV6jCZydlLmA/power/isLedOn" };
const String lightSensorLocation[] = { "/devices/4xxArxOlk1MSKPQ0eMGT/power/isLightSensorOn", "/devices/zzeDW8HAAV6jCZydlLmA/power/isLightSensorOn" };
const String updateTimeStampLocation[] = { "/devices/4xxArxOlk1MSKPQ0eMGT/power/UpdateTime", "/devices/zzeDW8HAAV6jCZydlLmA/power/UpdateTime" };
const String timeStampLocation = "Test/TimeStamp";
const String batteryLocations[] = { "/devices/4xxArxOlk1MSKPQ0eMGT/power/batteryLevel", "/devices/zzeDW8HAAV6jCZydlLmA/power/batteryLevel" };

bool isLedOn[] = { false, false };
bool lastLedState[] = { false, false };

bool isLightSensorOn[] = { false, false };
bool lastLightSensorState[] = { false, false };

const byte masterAddr = 0x01;
const byte loraAddr[] = {0x02, 0x03};
byte slaveAddr;

int loraSendIndex = 0;

int resetLoraCount = 0;
unsigned long previousMillisRestartLORA = 0;
const long intervalRestartLORA = 5000;

String incomingMessage = "";
String message = "";
char separator = ',';
int batteryLevels[] = {0, 0};
int lastBatteryLevels[] = {0, 0};
bool isTimeStampInit = false;
double gmt_7 = 7 * 3600;

int loraSendRate = 2000;
unsigned long loraSendCounter = 0;


  void setup() 
{
  Serial.begin(9600);
  while (!Serial);

  WifiInit();
  FirebaseInit();
  //DataInit();

  ResetLora();
}

void loop() {
  delay(50);

  LedHandler();
  OnReceive(LoRa.parsePacket());

  LoraResetHandler();

  //Only read data when time elaspe greater than readDataLimit || first boots up
  if ((millis() - lastDataReadMillis < readDataLimit && lastDataReadMillis != 0) || !Firebase.ready()) return;
  ReadData(loraSendIndex);
  lastDataReadMillis = millis();
}


//*****************************INIT FUNCTION********************************
void WifiInit() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wifi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }

  Serial.println("******************");
  Serial.print("Connected With IP: ");
  Serial.println(WiFi.localIP());
  Serial.println("******************");
}

void FirebaseInit() {
  //Setup
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;

  //Connecting
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  //Set database read timeout to 1min (max 15 mins)
  Firebase.setReadTimeout(firebaseData, 1000 * 60);
  //Write size time out tiny-1s, small-10s, medium-30s, large-60s
  Firebase.setwriteSizeLimit(firebaseData, dataSize);
}
/*
void DataInit() {
  Firebase.setBool(firebaseData, ledStateLocation[0], false);
  Firebase.setBool(firebaseData, ledStateLocation[1], false);

  Firebase.setBool(firebaseData, sensorLocation[0], false);
  Firebase.setBool(firebaseData, sensorLocation[1], false);

  Firebase.setString(firebaseData, updateTimeStampLocation[0], GetTimeStamp());
  Firebase.setString(firebaseData, updateTimeStampLocation[1], GetTimeStamp());
}*/
//**************************END INIT************************************

//*************************TIME STAMP***********************************
String GetTimeStamp() 
{
  String result = "";
  Serial.printf("Set timestamp... %s\n", Firebase.setTimestamp(firebaseData, timeStampLocation) ? "ok" : firebaseData.errorReason().c_str());

  if (firebaseData.httpCode() == FIREBASE_ERROR_HTTP_CODE_OK) 
  {
    // Timestamp saved in millisecond, get its seconds from int value
    Serial.print("TIMESTAMP (Seconds): ");
    Serial.println(firebaseData.to<int>());
  }

  Serial.printf("Get timestamp... %s\n", Firebase.getDouble(firebaseData, timeStampLocation) ? "ok" : firebaseData.errorReason().c_str());
  if (firebaseData.httpCode() == FIREBASE_ERROR_HTTP_CODE_OK) 
  {
    double timeStamp = firebaseData.to<uint64_t>();

    setTime(timeStamp / 1000 + gmt_7);
    int mil = (int)timeStamp % 1000;
    result = String(year()) + "-" + String(month()) + "-" + String(day()) + "T" + String(hour()) + ":" + String(minute()) + ":" + String(second()) + "." + String(mil) + "Z";
  }
  return result;
}
//**********************END TIME STAMP**********************************


//**************************READ WRITE FIREBASE*************************
void LedHandler() {

  if(millis() - loraSendCounter < loraSendRate) return;
  loraSendCounter = millis();
  String sendMessage =  String(isLedOn[loraSendIndex]) +  "," + String(isLightSensorOn[loraSendIndex]);
  SendLora(loraSendIndex, sendMessage);
    //Serial.println("Send LoRa: " + String (loraSendIndex));
  loraSendIndex++;
  if (loraSendIndex > 1) loraSendIndex = 0;

}

void ReadData(int id) 
{
  if (!Firebase.getBool(firebaseData, ledStateLocation[id])) 
  {
    Serial.print("Firebase Error: ");
    Serial.println(firebaseData.errorReason());

  } 
  else 
  {
    isLedOn[id] = firebaseData.boolData();
    if (lastLedState[id] != isLedOn[id]) 
    {
      lastLedState[id] = isLedOn[id];
      //Test
      Serial.println("                                                                  ************");
      Serial.print("                                                            ");
      Serial.println("Trang thái đèn" + String(id) + ": " + String(lastLedState[id]));
    }
  }

  if (!Firebase.getBool(firebaseData, lightSensorLocation[id])) 
  {
    Serial.print("Firebase Error: ");
    Serial.println(firebaseData.errorReason());
  } 
  else 
  {
    isLightSensorOn[id] = firebaseData.boolData();
    if (lastLightSensorState[id] != isLightSensorOn[id]) 
    {
      lastLightSensorState[id] = isLightSensorOn[id];
      Serial.println("                                                                  ************");
      Serial.print("                                              ");
      Serial.println("Trang thai cam bien anh sang" + String(id) + ": " + String(isLightSensorOn[id]));
    }
  }
}
  //Only write to server when using sensor
  void WriteFirebaseData(int index, bool isLedStateChange, bool isBatteryStateChanged) {
    bool isAnyDataChanged = false;
    
    if(isLedStateChange) 
    {
      Firebase.setBool(firebaseData, ledStateLocation[index], isLedOn[index]);
      isAnyDataChanged = true;
    }

    if(isBatteryStateChanged)
    {
      Firebase.setInt(firebaseData, batteryLocations[index], batteryLevels[index]);
      isAnyDataChanged = true;
    }

    if(!isAnyDataChanged) return;
    Firebase.setString(firebaseData, updateTimeStampLocation[index], GetTimeStamp());
  }
  //**************************END READ WRITE FIREBASE*************************


  //******************************LORA HANDLER********************************
  //Cú pháp gửi dữ liệu /Địa chỉ nhận/Địa chỉ gửi/Data/
  void SendLora(int id, String Outgoing) {
    //Serial.println("Send LORA");
    LoRa.beginPacket();             //--> start packet
    LoRa.write(loraAddr[id]);       //--> add destination address
    LoRa.write(masterAddr);         //--> add sender address
    LoRa.write(Outgoing.length());  //--> add payload length
    LoRa.print(Outgoing);           //--> add payload
    LoRa.endPacket();               //--> finish packet and send it
  }

  void OnReceive(int packetSize) {
    if (packetSize == 0) return;  // if there's no packet, return

    //Read packet header bytes:
    int recipient = LoRa.read();        //--> recipient address
    byte sender = LoRa.read();          //--> sender address
    byte incomingLength = LoRa.read();  //--> incoming msg length

    // Get the address of the senders or slaves.
    slaveAddr = sender;

    // Clears Incoming variable data.
    incomingMessage = "";

    // Get all incoming data / message.
    while (LoRa.available()) {
      incomingMessage += (char)LoRa.read();
    }

    // Resets the value of the Count_to_Rst_LORA variable if a message is received.
    resetLoraCount = 0;

    //Check length for error.
    if (incomingLength != incomingMessage.length()) {
      Serial.println("error: message length does not match length");
      return;
    }

    //Checks whether the incoming data or message for the master or not
    if (recipient != masterAddr) {
      Serial.println("This message is not for me.");
      return;
    }

    //If message is for this device, or broadcast, print details:
    Serial.println();
    Serial.println("*********************************");
    Serial.println("Rc from: 0x" + String(sender, HEX));
    Serial.println("Message: " + incomingMessage);

    ProcessingIncomingData();
  }

  void ProcessingIncomingData() {
    //Check if sender addr is valid
    bool isSenderAddrValid = false;
    for (int i; i < 2; i++) {
      if (slaveAddr == loraAddr[i]) {
        isSenderAddrValid = true;
        break;
      }
    }
    if (!isSenderAddrValid) return;

    //Getting loraIndex from sender address
     int loraIndex = 0;
    if(slaveAddr == loraAddr[0])
    {
      loraIndex = 0;
    }
    else if(slaveAddr == loraAddr[1])
    {
      loraIndex = 1;
    }
    else 
    {
      Serial.println("Dia chi khong hop le");
      return;
    }
   
    String isLedOnString = GetValue(incomingMessage, 0);
    String batteryLevel =  GetValue(incomingMessage, 1);
    batteryLevels[loraIndex] = batteryLevel.toInt();
    Serial.println("Battery: " + String(batteryLevels[loraIndex]));

    bool isLedData = false;
    bool isBatteryData = false;

    if (isLedOnString == "1" || isLedOnString == "0") {
      isLedOn[loraIndex] = isLedOnString.toInt();
      if (isLedOn[loraIndex] != lastLedState[loraIndex]) {
        lastLedState[loraIndex] = isLedOn[loraIndex];

        Serial.println("ESP32 Data Receive: " + String(isLedOn[loraIndex]));
        isLedData = true;
      }
    }

    int battery = batteryLevel.toInt();
    if(battery != lastBatteryLevels[loraIndex])
    {
      lastBatteryLevels[loraIndex] = battery;
      isBatteryData = true;
    }
    WriteFirebaseData(loraIndex, isLedData, isBatteryData);
  }

  //Code này dùng để tách khi gửi có nhiều dữ liệu cần tách thành chuỗi, Đối với ESP32 nhận bool isLedOn thì không cần
  //Đối với Arduino nhận 2 bool isSensorOn, isLedOn thì cần tách một string thành những substring
  //Data sẽ là string dữ liệu (parameter: String data) cách nhau bởi dấu " , "
  //Hàm sẽ tìm index đầu cuối của string dữ liệu với vị trí xếp trong chuỗi cho trước (paramater: int index)
  //Sau đó sẽ dùng hàm substring để lấy string dữ liệu cần dùng ra
  String GetValue(String data, int index) {
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++) {
      if (data.charAt(i) == separator || i == maxIndex) {
        found++;
        strIndex[0] = strIndex[1] + 1;
        strIndex[1] = (i == maxIndex) ? i + 1 : i;
      }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
  }

  //Lora hoạt động thời gian dài có lúc bị đơ
  //Workaround -> Reset lora theo định kỳ
  void LoraResetHandler() {
    unsigned long currentMillisRestartLORA = millis();
    if (currentMillisRestartLORA - previousMillisRestartLORA >= intervalRestartLORA) {
      previousMillisRestartLORA = currentMillisRestartLORA;
      LoRa.end();
      ResetLora();

    }
  }

  void ResetLora() {
    LoRa.setPins(ss, rst, dio0);

    //Serial.println();
    //Serial.println("Restart LoRa...");
    //Serial.println("Start LoRa init...");
    while (!LoRa.begin(433E6)) {
      Serial.println("Starting LoRa failed!");
      delay(100);
    }
    // Serial.println("LoRa init succeeded.");

    // Reset the value of the Count_to_Rst_LORA variable.
    resetLoraCount = 0;
  }

  //****************************END LORA HANDLER*****************************
