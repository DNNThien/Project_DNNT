#include <vector>
#include <SD.h>
#include <SPI.h>
#include <painlessMesh.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>

#define MY_NAME "ROOT"
#define MESH_PREFIX "MyMesh"
#define MESH_PASSWORD "88888888"
#define PORT 5555

painlessMesh mesh;
Scheduler userScheduler;
File file;
LiquidCrystal_I2C lcd(0x27, 20, 4);

uint8_t buzzerPin = 4;
uint8_t wakupPin = 34;

uint8_t numNodes;
StaticJsonDocument<200> nodes;
StaticJsonDocument<200> parameters;
uint8_t timeGetSensorValue = 0;
std::vector<String> bufferCommand;
uint8_t timeWaitCommand = 0;
std::vector<String> bufferSensorValue;
uint8_t timeWaitSensorValue = 0;
StaticJsonDocument<200> payload;

/*Parameters: {
  "time": {
    "read": 2,
    "send": 10
  },
  "threshold": {
    "tmp": 35,
    "hum": 50,
    "som": 40,
    "lux": {
      "up": 30,
      "down": 10
    }
  }
}*/

void printlnSerial(String str);
void receivedCallback(uint32_t from, String &msg);
void newConnectionCallback(uint32_t nodeId);
void readSystemInformation();
void initPayload();

Task commandProcessing(TASK_SECOND, TASK_FOREVER, [](){
  if(bufferCommand.size()) {
    timeWaitCommand++;
    if(timeWaitCommand == 5) {
      StaticJsonDocument<200> packet;
      for(uint8_t i = 0; i < bufferCommand.size(); i++) {
        deserializeJson(packet, bufferCommand[i]);
        String name = packet["name"].as<String>();
        uint32_t id = packet["id"].as<uint32_t>();
        if( !nodes.containsKey(name) || nodes[name].as<uint32_t>() != id) {
          nodes[name] = id;
          printlnSerial("Notification: New connection, Name: " + name + " - ID: " + String(id));
          // write file
        }
        if(packet["cmd"] == "reqconn") {
          String doc;
          packet["name"] = MY_NAME;
          packet["cmd"] = "connected";
          packet["read"] = parameters["time"]["read"].as<int>();
          packet["send"] = parameters["time"]["send"].as<int>();
          serializeJson(packet, doc);
          mesh.sendSingle(id, doc);
          printlnSerial("Command: " + doc);
        }
      }
      timeWaitCommand = 0;
      bufferCommand.clear();
    }
  }
});

Task getSensorValue( TASK_SECOND, TASK_FOREVER, [](){
  timeGetSensorValue++;
  if(timeGetSensorValue == parameters["time"]["send"].as<int>()) {
    String msg;
    StaticJsonDocument<200> packet;
    packet["name"] = MY_NAME;
    packet["type"] = 1;
    packet["cmd"] = "getval";
    serializeJson(packet, msg);
    mesh.sendBroadcast(msg);
    timeGetSensorValue = 0;
    printlnSerial("Command: " + msg);
  }
});

int tmpAveVal = 0, humAveVal = 0, luxAveVal = 0, somAveVal = 0;
int tmpLcd = 0, humLcd = 0, luxLcd = 0, somLcd = 0;
uint8_t flagTemperature = 0, flagHumidity = 0, flagSoilMoisture = 0, flagLuminosity = 0;
Task sensorValueProcessing( TASK_SECOND, TASK_FOREVER, [](){
  if(bufferSensorValue.size()) {
    timeWaitSensorValue++;
    if(timeWaitSensorValue == 5 || bufferSensorValue.size() == numNodes) {
      StaticJsonDocument<200> packet;
      for(uint8_t i = 0; i < bufferSensorValue.size(); i++) {
        Serial1.println(bufferSensorValue[i]);
        Serial.println("Sent Data: " + bufferSensorValue[i]);
        deserializeJson(packet, bufferSensorValue[i]);
        tmpAveVal += packet["tmp"].as<int>();
        humAveVal += packet["hum"].as<int>();
        luxAveVal += packet["lux"].as<int>();
        somAveVal += packet["som"].as<int>();
        payload.remove(packet["name"].as<String>());
      }
      tmpAveVal /= bufferSensorValue.size();
      humAveVal /= bufferSensorValue.size();
      luxAveVal /= bufferSensorValue.size();
      somAveVal /= bufferSensorValue.size();
      payload["ROOT"]["tmp"] = tmpAveVal;
      payload["ROOT"]["hum"] = humAveVal;
      payload["ROOT"]["lux"] = luxAveVal;
      payload["ROOT"]["som"] = somAveVal;
      String msg;
      for(JsonPair i : payload.as<JsonObject>()) {
        serializeJson(i.value(), msg);
        Serial1.println(msg);
        Serial.println("Sent data: " + msg);
      }
      Serial1.println("done");
      if(tmpAveVal > parameters["threshold"]["tmp"].as<int>()) {
        flagTemperature++;
      } else {
        flagTemperature = 0;
      }
      if(humAveVal > parameters["threshold"]["hum"].as<int>()) {
        flagHumidity++;
      } else {
        flagHumidity = 0;
      }
      if(luxAveVal < parameters["threshold"]["som"].as<int>()) {
        flagSoilMoisture++;
      } else {
        flagSoilMoisture = 0;
      }
      if(somAveVal > parameters["threshold"]["lux"]["down"].as<int>() && somAveVal < parameters["threshold"]["lux"]["up"].as<int>()) {
        flagLuminosity++;
      } else {
        flagLuminosity = 0;
      }
      printlnSerial("Notification: Flag control buzzer: \nTemperature: " + String(flagTemperature) + "\nHumidity: " + String(flagHumidity) + "\nSoil-moisture: " + String(flagSoilMoisture) + "\nLuminosity: " + String(flagLuminosity));
      initPayload();
      tmpLcd = tmpAveVal;
      humLcd = humAveVal;
      luxLcd = luxAveVal;
      somLcd = somAveVal;

      tmpAveVal = 0;
      humAveVal = 0;
      luxAveVal = 0;
      somAveVal = 0;
      timeWaitSensorValue = 0;
      bufferSensorValue.clear();
    }
  }
});

bool flagControlBuzzer = false;
uint8_t timeControlBuzzer = 0;
Task controlBuzzer(TASK_SECOND, TASK_FOREVER, [](){
  if(flagTemperature < 5 && flagHumidity < 5 && flagLuminosity < 5 && flagSoilMoisture < 5) {
    flagControlBuzzer = false;
    timeControlBuzzer = 0;
  } else flagControlBuzzer = true;
  if(flagControlBuzzer) {
    timeControlBuzzer++;
    if(timeControlBuzzer <= 6) {
      digitalWrite(buzzerPin, !digitalRead(buzzerPin));
    } else if(timeControlBuzzer == 60) {
      timeControlBuzzer = 0;
    }
  }
});

int timeLcdOn = 600;
int checkValueChange = 0;
uint8_t stateBacklightLcd = 1;

Task controlLCD(100 * TASK_MILLISECOND, TASK_FOREVER, [](){
  if(timeLcdOn > 0) {
    timeLcdOn--;
    if(checkValueChange != tmpLcd + humLcd + luxLcd + somLcd) {
      checkValueChange = tmpLcd + humLcd + luxLcd + somLcd;
      stateBacklightLcd = 1;
    }
    if(stateBacklightLcd == 1) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Temp");
      if(flagTemperature >= 5) {
        lcd.setCursor(2, 1);
        lcd.print("!" + String(tmpLcd));
      } else {
        lcd.setCursor(1, 1);
        lcd.print(String(tmpLcd));
      }
      
      lcd.setCursor(10, 0);
      lcd.print("Humi");
      if(flagHumidity >= 5) {
        lcd.setCursor(10, 1);
        lcd.print("!" + String(humLcd));
      } else {
        lcd.setCursor(11, 1);
        lcd.print(String(humLcd));
      }
      
      lcd.setCursor(5, 2);
      lcd.print("Soil");
      if(flagSoilMoisture >= 5) {
        lcd.setCursor(4, 3);
        lcd.print("!" + String(somLcd));
      } else {
        lcd.setCursor(6, 3);
        lcd.print(String(somLcd));
      }
      
      lcd.setCursor(15, 2);
      lcd.print("Light");
      if(flagLuminosity >= 5) {
        lcd.setCursor(15, 3);
        lcd.print("!" + String(luxLcd));
      } else {
        lcd.setCursor(16, 3);
        lcd.print(String(luxLcd));
      }
      stateBacklightLcd = 0;
    }
  }
  if(timeLcdOn == 0) {
    timeLcdOn--;
    lcd.clear();
    lcd.noBacklight();
    stateBacklightLcd = 0;
  }
  if(digitalRead(wakupPin)) {
    timeLcdOn = 600;
    if(stateBacklightLcd == 0) {
      lcd.backlight();
      stateBacklightLcd = 1;
    }
  }
});

// Task setting(TASK_SECOND, TASK_FOREVER, [](){
//   if(digitalRead(wakupPin)) {
//     int start = millis();
//     uint8_t flag = 0; // 0: wakup, 1: setting
//     while(digitalRead(wakupPin))) {
//       if(millis() - timeWait == 5) {
//         flag = 1;
//         break;
//       }
//     }
//     if(flag)
//   }
// });

void setup() {
  pinMode(buzzerPin, OUTPUT);
  pinMode(wakupPin, INPUT);
  Serial.begin(115200);
  Serial1.begin(9600, SERIAL_8N1, 16, 17);
  lcd.init();
  lcd.backlight();
  delay(1000);
  if(SD.begin(5)) {
    printlnSerial("SD card initialization successful.....");
  } else {
    printlnSerial("SD card initialization unsuccessful!!!");
  }
  readSystemInformation();
  numNodes = nodes.size();
  initPayload();

  mesh.setRoot();
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, PORT);
  mesh.setHostname(MY_NAME);
  mesh.setContainsRoot();
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onNodeTimeAdjusted([](int32_t offset){});

  userScheduler.addTask(getSensorValue);
  getSensorValue.enable();
  userScheduler.addTask(commandProcessing);
  commandProcessing.enable();
  userScheduler.addTask(sensorValueProcessing);
  sensorValueProcessing.enable();
  userScheduler.addTask(controlBuzzer);
  controlBuzzer.enable();
  userScheduler.addTask(controlLCD);
  controlLCD.enable();

  lcd.setCursor(7, 0);
  lcd.print("HELLO");
  lcd.setCursor(0, 1);
  lcd.print("Welcome to ");
  lcd.setCursor(2, 2);
  lcd.print("MY SMART GARDEN");
  lcd.setCursor(0, 3);
  lcd.print("Wait a minute.....");
  delay(2000);
}

void loop() {
  mesh.update();
}

void printlnSerial(String str) {
  Serial.println("Serial: " + str);
  Serial.println("-------------------------");
}

void receivedCallback(uint32_t from, String &msg) {
  StaticJsonDocument<200> packet;
  deserializeJson(packet, msg);
  if(packet["type"] == 1) {
    packet["id"] = from;
    serializeJson(packet, msg);
    bufferCommand.push_back(msg);
  } else {
    bufferSensorValue.push_back(msg);
  }
  printlnSerial("Messenger from " + String(from) + ": " + msg);
}

void newConnectionCallback(uint32_t nodeId) {
  printlnSerial("New connection: " + String(nodeId));
}

void readSystemInformation() {
  String doc;
  file = SD.open("/system/nodes.txt");
  if(file) {
    while(file.available()) {
      doc += (char) file.read();
    }
    printlnSerial("Nodes: " + doc);
    deserializeJson(nodes, doc);  
    file.close();
  }

  doc = "";
  file = SD.open("/system/parameters.txt");
  if(file) {
    while(file.available()) {
      doc += (char) file.read();
    }
    printlnSerial("Parameters: " + doc);
    deserializeJson(parameters, doc);
    file.close();
  }
}

void initPayload() {
  JsonObject node;
  if(payload.size())
    payload.clear();
  for(uint8_t i = 1; i <= numNodes; i++) {
    node = payload.createNestedObject("SENSOR" + String(i));
    node["name"] = "SENSOR" + String(i);
    node["type"] = 0;
    node["tmp"] = 0;
    node["hum"] = 0;
    node["lux"] = 0;
    node["som"] = 0;
  }
  node = payload.createNestedObject("ROOT");
  node["name"] = "ROOT";
  node["type"] = 0;
  node["tmp"] = 0;
  node["hum"] = 0;
  node["lux"] = 0;
  node["som"] = 0;
}
