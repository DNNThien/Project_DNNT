#include <vector>
#include <painlessMesh.h>
#include <Wire.h>
#include <DHT.h>
#include <BH1750.h>

#define MY_NAME "SENSOR1"
#define MESH_PREFIX "MyMesh"
#define MESH_PASSWORD "88888888"
#define PORT 5555

#define DHTPIN 19
#define DHTTYPE DHT22
#define SOILPIN 34

painlessMesh mesh;
Scheduler userScheduler;
DHT dht22(DHTPIN, DHTTYPE);
BH1750 bh1750;
uint32_t myID;
uint32_t rootID = 0;

std::vector<String> bufferCommand;
uint8_t TimeCommandReceive = 0;

StaticJsonDocument<200> sensorValue;
uint8_t countTimesReadValue = 0;
bool flagSendSensorValue = false;

void printlnSerial(String str);
void receivedCallback(uint32_t from, String &msg);
bool containsInBufferCommand(const String &str);

Task requestConnectToRoot(10 * TASK_SECOND, TASK_FOREVER, [](){
  if(rootID == 0 || !mesh.isConnected(rootID)) {
    String msg;
    StaticJsonDocument<200> packet;
    packet["name"] = MY_NAME;
    packet["type"] = 1;
    packet["cmd"] = "reqconn";
    serializeJson(packet, msg);
    mesh.sendBroadcast(msg);
    printlnSerial("Require Connection with Root: " + msg);
  }
});

int tmpVal = 0, humVal = 0, somVal = 0, luxVal = 0;
Task taskSendSensorValue(1 * TASK_SECOND, TASK_FOREVER, [](){
  tmpVal += dht22.readTemperature();
  humVal += dht22.readHumidity();
  luxVal += bh1750.readLightLevel();
  somVal += map(analogRead(SOILPIN), 4095, 0, 0, 100);
  countTimesReadValue++;
  // Serial.println("Count Times Read Value: " + String(countTimesReadValue));

  // send sensor value
  if(flagSendSensorValue) {
    // average value
    sensorValue["tmp"] = tmpVal / countTimesReadValue;
    sensorValue["hum"] = humVal / countTimesReadValue;
    sensorValue["lux"] = luxVal / countTimesReadValue;
    sensorValue["som"] = somVal / countTimesReadValue;
    
    // create packet
    String msg;
    serializeJson(sensorValue, msg);
    if(rootID != 0 && mesh.isConnected(rootID)) {
      mesh.sendSingle(rootID, msg);
      printlnSerial("Sent Successfully: " + msg);
    } else printlnSerial("Sent Unsuccessfully: " + msg);

    // reset
    tmpVal = 0;
    humVal = 0;
    somVal = 0;
    luxVal = 0; 
    countTimesReadValue = 0;
    flagSendSensorValue = false;
  }
});

Task taskCommandProcessing(TASK_IMMEDIATE, TASK_FOREVER, [](){
  if(bufferCommand.size()) {
    StaticJsonDocument<200> packet;
    for(uint8_t i = 0; i < bufferCommand.size(); i++) {
      deserializeJson(packet, bufferCommand[i]);
      if(packet["cmd"] == "connected" || packet["cmd"] == "settime") {
        if(rootID != packet["id"].as<uint32_t>()) {
          rootID = packet["id"].as<uint32_t>();
        }
        if(packet.containsKey("read")) {
          uint8_t timeRead = packet["read"].as<int>();
          taskSendSensorValue.setInterval(timeRead * TASK_SECOND);
        }
      }
      if(packet["cmd"] == "getval") {
        flagSendSensorValue = true;
      }
    }
    bufferCommand.clear();
  }
});

void setup() {
  //init serial
  Serial.begin(115200);
  dht22.begin();
  Wire.begin();
  bh1750.begin();
  delay(1000);

  // init mesh
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, PORT);
  mesh.setContainsRoot();

  // register callback function
  mesh.onReceive(&receivedCallback);
  mesh.onNodeTimeAdjusted([](int32_t offset){});

  // register task
  userScheduler.addTask(taskSendSensorValue);
  taskSendSensorValue.enable();
  userScheduler.addTask(taskCommandProcessing);
  taskCommandProcessing.enable();
  userScheduler.addTask(requestConnectToRoot);
  requestConnectToRoot.enable();

  //init sensor value
  sensorValue["name"] = MY_NAME;
  sensorValue["type"] = 0;
}

void loop() {
  mesh.update();
}

void receivedCallback(uint32_t from, String &msg) {
  StaticJsonDocument<200> packet;
  deserializeJson(packet, msg);
  packet["id"] = from;
  serializeJson(packet, msg);
  printlnSerial("Messenger from " + String(from) + ": " + msg);
  if(packet["type"] == 1) {
    bufferCommand.push_back(msg);
  }
}

void printlnSerial(String str) {
  Serial.println(str);
  Serial.println("------------------------------");
}
