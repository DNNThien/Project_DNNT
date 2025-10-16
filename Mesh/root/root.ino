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

#define KEYPAD_ADDR 0x20
#define LCD_ADDR 0x27

painlessMesh mesh;
Scheduler userScheduler;
LiquidCrystal_I2C lcd(LCD_ADDR, 20, 4);

char keys[16] = {
  '1', '2', '3', 'A',
  '4', '5', '6', 'B',
  '7', '8', '9', 'C',
  '*', '0', '#', 'D'
};
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

int timeLcdOn;

/*{
  "time": {
    "read": 1,
    "send": 30,
    "screen": 1
  },
  "threshold": {
    "tmp": {
      "up": 35,
      "down": 28
    },
    "hum": {
      "up": 60,
      "down": 40
    },
    "som": {
      "up": 60,
      "down": 40
    },
    "lux": {
      "up": 30,
      "down": 10
    }
  }
}*/

void printlnSerial(String str);
void receivedCallback(uint32_t from, String &msg);
void newConnectionCallback(uint32_t nodeId);
void writeFile(String path, String content);
void readSystemInformation();
void initPayload();
char readKey();

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
          String doc;
          serializeJsonPretty(nodes, doc);
          writeFile("/system/nodes.txt", doc);
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
      if(tmpAveVal > parameters["threshold"]["tmp"]["up"].as<int>() || tmpAveVal < parameters["threshold"]["tmp"]["down"].as<int>()) {
        flagTemperature++;
      } else {
        flagTemperature = 0;
      }
      if(humAveVal > parameters["threshold"]["hum"]["up"].as<int>() || humAveVal < parameters["threshold"]["hum"]["down"].as<int>()) {
        flagHumidity++;
      } else {
        flagHumidity = 0;
      }
      if(luxAveVal > parameters["threshold"]["som"]["up"].as<int>() || luxAveVal < parameters["threshold"]["som"]["down"].as<int>()) {
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

enum SETTING_OPTION {
  SET_OFF = 0,
  SET_ON = 1,
  SET_TIME_READ = 2,
  SET_TIME_SEND = 3,
  SET_TIME_SCREEN = 4,
  SET_THRESHOLD_TMP = 5,
  SET_THRESHOLD_HUM = 6,
  SET_THRESHOLD_SOM = 7,
  SET_THRESHOLD_LUX = 8,
};
uint8_t curSetting = SET_OFF;
uint8_t preSetting = 9;
bool updateScreen = true;
String tempVal = "";
uint8_t typeThresh = 0; // 0: up, 1: down
Task setting(TASK_MILLISECOND, TASK_FOREVER, [](){
  static bool touchActive = false;
  static unsigned long touchStart = 0;
  static const unsigned long holdTime = 5000; // 5s
  static const unsigned long timeOut = 20000; // 2p
  if(curSetting == SET_OFF) {
    if(digitalRead(wakupPin) == 1) {
      if(!touchActive) {
        touchActive = true;
        touchStart = millis();
      } else {
        if(millis() - touchStart >= holdTime) {
          touchStart = millis();
          curSetting = SET_ON;
        }
      }
    } else {
      touchActive = false;
    }
  } else {
    if(millis() - touchStart <= timeOut) {
      char key = readKey();
      if(key) {
        touchStart = millis();
        switch(curSetting) {
        case SET_ON:
          if(key >= '1' && key <= '7') {
            curSetting = (key - '0') + 1;
          } else if(key == 'C') {
            curSetting = SET_OFF;
            preSetting = SET_OFF;
            readKey();
          }
          break;
        case SET_TIME_READ:
          if(key >= '0' && key <= '9' && tempVal.length() < 2) {
            tempVal += String(key);
            printlnSerial("Temp Val: " + tempVal);
          } else if(key == 'A') {
            if(tempVal.toInt() <= parameters["time"]["send"].as<int>()) {
              String doc;
              StaticJsonDocument<200> packet;
              packet["name"] = MY_NAME;
              packet["type"] = 1;
              packet["cmd"] = "settime";
              packet["read"] = tempVal.toInt();
              serializeJson(packet, doc);
              mesh.sendBroadcast(doc);
              parameters["time"]["read"] = tempVal.toInt();
              serializeJsonPretty(parameters, doc);
              writeFile("/system/parameters.txt", doc);
              printlnSerial("Sensor read time changed successfully.");
            }
            tempVal = "";
            curSetting = SET_ON;
          } else if(key == 'B') {
            tempVal = "";
          } else if(key == 'C') {
            tempVal = "";
            curSetting = SET_ON;
          }
          break;
        case SET_TIME_SEND:
          if(key >= '0' && key <= '9' && tempVal.length() < 2) {
            tempVal += String(key);
            printlnSerial("Temp Val: " + tempVal);
          } else if(key == 'A') {
            if(tempVal.toInt() >= parameters["time"]["read"].as<int>()) {
              String doc;
              parameters["time"]["send"] = tempVal.toInt();
              serializeJsonPretty(parameters, doc);
              writeFile("/system/parameters.txt", doc);
              printlnSerial("Sensor send time changed successfully.");
            }
            tempVal = "";
            curSetting = SET_ON;
          } else if(key == 'B') {
            tempVal = "";
          } else if(key == 'C') {
            tempVal = "";
            curSetting = SET_ON;
          }
          break;
        case SET_TIME_SCREEN:
          if(key >= '0' && key <= '9' && tempVal.length() < 2) {
            tempVal += String(key);
            printlnSerial("Temp Val: " + tempVal);
          } else if(key == 'A') {
            if(tempVal) {
              String doc;
              parameters["time"]["screen"] = tempVal.toInt();              
              serializeJsonPretty(parameters, doc);
              writeFile("/system/parameters.txt", doc);
              timeLcdOn = parameters["time"]["screen"].as<int>() * 60000;
              printlnSerial("Screen brightness time has been changed.");
            }
            tempVal = "";
            curSetting = SET_ON;
          } else if(key == 'B') {
            tempVal = "";
          } else if(key == 'C') {
            tempVal = "";
            curSetting = SET_ON;
          }
          break;
        case SET_THRESHOLD_TMP:
          if(key >= '0' && key <= '9' && tempVal.length() < 2) {
            tempVal += String(key);
            printlnSerial("Temp Val: " + tempVal);
          } else if(key == 'A') {
            if(tempVal) {
              if(typeThresh == 0 && tempVal.toInt() > parameters["threshold"]["tmp"]["down"]) {
                String doc;
                parameters["threshold"]["tmp"]["up"] = tempVal.toInt();              
                serializeJsonPretty(parameters, doc);
                writeFile("/system/parameters.txt", doc);
                printlnSerial("Upper temperature threshold changed successfully.");
                typeThresh = 1;
                tempVal = "";
              } else if(typeThresh == 1 && tempVal.toInt() < parameters["threshold"]["tmp"]["up"]) {
                String doc;
                parameters["threshold"]["tmp"]["down"] = tempVal.toInt();              
                serializeJsonPretty(parameters, doc);
                writeFile("/system/parameters.txt", doc);
                printlnSerial("Lower temperature threshold changed successfully.");
                typeThresh = 2;
              } else printlnSerial("Unsuccessfully!");
            } else printlnSerial("Unsuccessfully!");
            if(typeThresh == 2) {
              typeThresh = 0;
              tempVal = "";
              curSetting = SET_ON;
            }
          } else if(key == 'B') {
            tempVal = "";
          } else if(key == 'C') {
            tempVal = "";
            curSetting = SET_ON;
          }
          break;
        case SET_THRESHOLD_HUM:
          if(key >= '0' && key <= '9' && tempVal.length() < 2) {
            tempVal += String(key);
            printlnSerial("Temp Val: " + tempVal);
          } else if(key == 'A') {
            if(tempVal) {
              if(typeThresh == 0 && tempVal.toInt() > parameters["threshold"]["hum"]["down"]) {
                String doc;
                parameters["threshold"]["hum"]["up"] = tempVal.toInt();              
                serializeJsonPretty(parameters, doc);
                writeFile("/system/parameters.txt", doc);
                printlnSerial("Upper humidity threshold changed successfully.");
                typeThresh = 1;
                tempVal = "";
              } else if(typeThresh == 1 && tempVal.toInt() < parameters["threshold"]["hum"]["up"]) {
                String doc;
                parameters["threshold"]["hum"]["down"] = tempVal.toInt();              
                serializeJsonPretty(parameters, doc);
                writeFile("/system/parameters.txt", doc);
                printlnSerial("Lower humidity threshold changed successfully.");
                typeThresh = 2;
              } else printlnSerial("Unsuccessfully!");
            } else printlnSerial("Unsuccessfully!");
            if(typeThresh == 2) {
              typeThresh = 0;
              tempVal = "";
              curSetting = SET_ON;
            }
          } else if(key == 'B') {
            tempVal = "";
          } else if(key == 'C') {
            tempVal = "";
            curSetting = SET_ON;
          }
          break;
        case SET_THRESHOLD_SOM:
          if(key >= '0' && key <= '9' && tempVal.length() < 2) {
            tempVal += String(key);
            printlnSerial("Temp Val: " + tempVal);
          } else if(key == 'A') {
            if(tempVal) {
              if(typeThresh == 0 && tempVal.toInt() > parameters["threshold"]["som"]["down"]) {
                String doc;
                parameters["threshold"]["som"]["up"] = tempVal.toInt();              
                serializeJsonPretty(parameters, doc);
                writeFile("/system/parameters.txt", doc);
                printlnSerial("Upper soil-moisture threshold changed successfully.");
                typeThresh = 1;
                tempVal = "";
              } else if(typeThresh == 1 && tempVal.toInt() < parameters["threshold"]["som"]["up"]) {
                String doc;
                parameters["threshold"]["som"]["down"] = tempVal.toInt();              
                serializeJsonPretty(parameters, doc);
                writeFile("/system/parameters.txt", doc);
                printlnSerial("Lower soil-moisture threshold changed successfully.");
                typeThresh = 2;
              } else printlnSerial("Unsuccessfully!");
            } else printlnSerial("Unsuccessfully!");
            if(typeThresh == 2) {
              typeThresh = 0;
              tempVal = "";
              curSetting = SET_ON;
            }
          } else if(key == 'B') {
            tempVal = "";
          } else if(key == 'C') {
            tempVal = "";
            curSetting = SET_ON;
          }
          break;
        case SET_THRESHOLD_LUX:
          if(key >= '0' && key <= '9' && tempVal.length() < 2) {
            tempVal += String(key);
            printlnSerial("Temp Val: " + tempVal);
          } else if(key == 'A') {
            if(tempVal) {
              if(typeThresh == 0 && tempVal.toInt() > parameters["threshold"]["lux"]["down"]) {
                String doc;
                parameters["threshold"]["lux"]["up"] = tempVal.toInt();              
                serializeJsonPretty(parameters, doc);
                writeFile("/system/parameters.txt", doc);
                printlnSerial("Upper luminosity threshold changed successfully.");
                typeThresh = 1;
                tempVal = "";
              } else if(typeThresh == 1 && tempVal.toInt() < parameters["threshold"]["lux"]["up"]) {
                String doc;
                parameters["threshold"]["lux"]["down"] = tempVal.toInt();              
                serializeJsonPretty(parameters, doc);
                writeFile("/system/parameters.txt", doc);
                printlnSerial("Lower luminosity threshold changed successfully.");
                typeThresh = 2;
              } else printlnSerial("Unsuccessfully!");
            } else printlnSerial("Unsuccessfully!");
            if(typeThresh == 2) {
              typeThresh = 0;
              tempVal = "";
              curSetting = SET_ON;
            }
          } else if(key == 'B') {
            tempVal = "";
          } else if(key == 'C') {
            tempVal = "";
            curSetting = SET_ON;
          }
          break;
        }
        updateScreen = true;
      }
    } else {
      curSetting = SET_OFF;
    }
  }
});

Task controlLCD(TASK_MILLISECOND, TASK_FOREVER, [](){
  static int checkValueChange = 0;
  static bool stateBacklightLcd = true;
  if(curSetting == SET_OFF) {
    if(timeLcdOn > 0) {
      timeLcdOn--;
      if(checkValueChange != tmpLcd + humLcd + luxLcd + somLcd) {
        checkValueChange = tmpLcd + humLcd + luxLcd + somLcd;
        updateScreen = true;
      }
      if(updateScreen) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Temp      Humi");
        if(flagTemperature >= 5) {
          lcd.setCursor(1, 1);
          lcd.print("!" + String(tmpLcd));
        } else {
          lcd.setCursor(1, 1);
          lcd.print(String(tmpLcd));
        }
        if(flagHumidity >= 5) {
          lcd.setCursor(10, 1);
          lcd.print("!" + String(humLcd));
        } else {
          lcd.setCursor(11, 1);
          lcd.print(String(humLcd));
        }
        
        lcd.setCursor(5, 2);
        lcd.print("Soil      Light");
        if(flagSoilMoisture >= 5) {
          lcd.setCursor(4, 3);
          lcd.print("!" + String(somLcd));
        } else {
          lcd.setCursor(6, 3);
          lcd.print(String(somLcd));
        }
        if(flagLuminosity >= 5) {
          lcd.setCursor(15, 3);
          lcd.print("!" + String(luxLcd));
        } else {
          lcd.setCursor(16, 3);
          lcd.print(String(luxLcd));
        }
        updateScreen = false;
      }
    } else if(timeLcdOn == 0) {
      lcd.clear();
      lcd.noBacklight();
      timeLcdOn = -1;
      stateBacklightLcd = false;
    }
    if(digitalRead(wakupPin) && stateBacklightLcd == false) {
      lcd.backlight();
      timeLcdOn = parameters["time"]["screen"].as<int>() * 60000;
      updateScreen = true;
      stateBacklightLcd = true;
    }
  } else {
    if(curSetting != preSetting || updateScreen == true) {
      preSetting = curSetting;
      switch(curSetting) {
      case SET_ON:
        lcd.clear();
        lcd.setCursor(1, 0);
        lcd.print("SETTING   4.ThTmp");
        lcd.setCursor(1, 1);
        lcd.print("1.TmRead  5.ThHum");
        lcd.setCursor(1, 2);
        lcd.print("2.TmSend  6.ThSom");
        lcd.setCursor(1, 3);
        lcd.print("3.TmScrn  7.ThLux");
        break;
      case SET_TIME_READ:
        lcd.clear();
        lcd.setCursor(2,0);
        lcd.print("SENSOR READ TIME");
        lcd.setCursor(1, 1);
        lcd.print("Present Value: " + (parameters["time"]["read"].as<String>().length() == 1 ? "0" + parameters["time"]["read"].as<String>() : parameters["time"]["read"].as<String>()) + "s");
        lcd.setCursor(9, 2);
        if(tempVal == "") {
          lcd.print("__");
        } else lcd.print(tempVal);
        break;
      case SET_TIME_SEND:
        lcd.clear();
        lcd.setCursor(2,0);
        lcd.print("SENSOR SEND TIME");
        lcd.setCursor(1, 1);
        lcd.print("Present Value: " + (parameters["time"]["send"].as<String>().length() == 1 ? "0" + parameters["time"]["send"].as<String>() : parameters["time"]["send"].as<String>()) + "s");
        lcd.setCursor(9, 2);
        if(tempVal == "") {
          lcd.print("__");
        } else lcd.print(tempVal);
        break;
      case SET_TIME_SCREEN:
        lcd.clear();
        lcd.setCursor(3,0);
        lcd.print("SCREEN ON TIME");
        lcd.setCursor(1, 1);
        lcd.print("Present Value: " + (parameters["time"]["screen"].as<String>().length() == 1 ? "0" + parameters["time"]["screen"].as<String>() : parameters["time"]["screen"].as<String>()) + "m");
        lcd.setCursor(9, 2);
        if(tempVal == "") {
          lcd.print("__");
        } else lcd.print(tempVal);
        break;
      case SET_THRESHOLD_TMP:
        lcd.clear();
        lcd.setCursor(1,0);
        lcd.print("SET TEMP THRESHOLD");
        lcd.setCursor(6, 1); //threashold current
        lcd.print(parameters["threshold"]["tmp"]["down"].as<String>() + " - " + parameters["threshold"]["tmp"]["up"].as<String>());
        if(typeThresh == 0) {
          if(tempVal == "") {
            lcd.setCursor(0, 2);
            lcd.print("Upper Threshold: __");
            lcd.setCursor(0, 3);
            lcd.print("Lower Threshold: __");
          } else {
            lcd.setCursor(0, 2);
            lcd.print("Upper Threshold: " + tempVal);
            lcd.setCursor(0, 3);
            lcd.print("Lower Threshold: __");
          }
        } else if(typeThresh == 1) {
          if(tempVal == "") {
            lcd.setCursor(0, 2);
            lcd.print("Upper Threshold: " + parameters["threshold"]["tmp"]["up"].as<String>());
            lcd.setCursor(0, 3);
            lcd.print("Lower Threshold: __");
          } else {
            lcd.setCursor(0, 2);
            lcd.print("Upper Threshold: " + parameters["threshold"]["tmp"]["up"].as<String>());
            lcd.setCursor(0, 3);
            lcd.print("Lower Threshold: " + tempVal);
          }
        }
        break;
      case SET_THRESHOLD_HUM:
        lcd.clear();
        lcd.setCursor(1,0);
        lcd.print("SET HUMI THRESHOLD");
        lcd.setCursor(6, 1); //threashold current
        lcd.print(parameters["threshold"]["hum"]["down"].as<String>() + " - " + parameters["threshold"]["hum"]["up"].as<String>());
        if(typeThresh == 0) {
          if(tempVal == "") {
            lcd.setCursor(0, 2);
            lcd.print("Upper Threshold: __");
            lcd.setCursor(0, 3);
            lcd.print("Lower Threshold: __");
          } else {
            lcd.setCursor(0, 2);
            lcd.print("Upper Threshold: " + tempVal);
            lcd.setCursor(0, 3);
            lcd.print("Lower Threshold: __");
          }
        } else if(typeThresh == 1) {
          if(tempVal == "") {
            lcd.setCursor(0, 2);
            lcd.print("Upper Threshold: " + parameters["threshold"]["hum"]["up"].as<String>());
            lcd.setCursor(0, 3);
            lcd.print("Lower Threshold: __");
          } else {
            lcd.setCursor(0, 2);
            lcd.print("Upper Threshold: " + parameters["threshold"]["hum"]["up"].as<String>());
            lcd.setCursor(0, 3);
            lcd.print("Lower Threshold: " + tempVal);
          }
        }
        break;
      case SET_THRESHOLD_SOM:
        lcd.clear();
        lcd.setCursor(1,0);
        lcd.print("SET SOIL THRESHOLD");
        lcd.setCursor(6, 1); //threashold current
        lcd.print(parameters["threshold"]["som"]["down"].as<String>() + " - " + parameters["threshold"]["som"]["up"].as<String>());
        if(typeThresh == 0) {
          if(tempVal == "") {
            lcd.setCursor(0, 2);
            lcd.print("Upper Threshold: __");
            lcd.setCursor(0, 3);
            lcd.print("Lower Threshold: __");
          } else {
            lcd.setCursor(0, 2);
            lcd.print("Upper Threshold: " + tempVal);
            lcd.setCursor(0, 3);
            lcd.print("Lower Threshold: __");
          }
        } else if(typeThresh == 1) {
          if(tempVal == "") {
            lcd.setCursor(0, 2);
            lcd.print("Upper Threshold: " + parameters["threshold"]["som"]["up"].as<String>());
            lcd.setCursor(0, 3);
            lcd.print("Lower Threshold: __");
          } else {
            lcd.setCursor(0, 2);
            lcd.print("Upper Threshold: " + parameters["threshold"]["som"]["up"].as<String>());
            lcd.setCursor(0, 3);
            lcd.print("Lower Threshold: " + tempVal);
          }
        }
        break;
      case SET_THRESHOLD_LUX:
        lcd.clear();
        lcd.setCursor(1,0);
        lcd.print("SET LIGHT THRESHOLD");
        lcd.setCursor(6, 1); //threashold current
        lcd.print(parameters["threshold"]["lux"]["down"].as<String>() + " - " + parameters["threshold"]["lux"]["up"].as<String>());
        if(typeThresh == 0) {
          if(tempVal == "") {
            lcd.setCursor(0, 2);
            lcd.print("Upper Threshold: __");
            lcd.setCursor(0, 3);
            lcd.print("Lower Threshold: __");
          } else {
            lcd.setCursor(0, 2);
            lcd.print("Upper Threshold: " + tempVal);
            lcd.setCursor(0, 3);
            lcd.print("Lower Threshold: __");
          }
        } else if(typeThresh == 1) {
          if(tempVal == "") {
            lcd.setCursor(0, 2);
            lcd.print("Upper Threshold: " + parameters["threshold"]["lux"]["up"].as<String>());
            lcd.setCursor(0, 3);
            lcd.print("Lower Threshold: __");
          } else {
            lcd.setCursor(0, 2);
            lcd.print("Upper Threshold: " + parameters["threshold"]["lux"]["up"].as<String>());
            lcd.setCursor(0, 3);
            lcd.print("Lower Threshold: " + tempVal);
          }
        }
        break;
      }
      updateScreen = false;
    }
  }
});

void setup() {
  pinMode(buzzerPin, OUTPUT);
  pinMode(wakupPin, INPUT);
  Wire.begin();
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
  timeLcdOn = parameters["time"]["screen"].as<int>() * 60000;
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
  userScheduler.addTask(setting);
  setting.enable();

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

void writeFile(String path, String content) {
  File file = SD.open(path, FILE_WRITE);
  if(file) {
    file.println(content);
    file.close();
  }
}

void readSystemInformation() {
  String doc;
  File file = SD.open("/system/nodes.txt");
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

char readKey() {
  static unsigned long lastCheck = 0;
  static bool waitingRelease = false;
  static uint8_t lastKeyIndex = 255;
  const unsigned long debounceTime = 50; // chống dội phím

  if (millis() - lastCheck < debounceTime) return '\0';
  lastCheck = millis();

  uint8_t lowByte = 0, highByte = 0;
  uint16_t data = 0xFFFF;

  Wire.requestFrom(KEYPAD_ADDR, 2);
  if (Wire.available() == 2) {
    lowByte = Wire.read();
    highByte = Wire.read();
    data = (highByte << 8) | lowByte;
  }

  if (waitingRelease) {
    if (!(data & (1 << lastKeyIndex))) {
      waitingRelease = false;
      lastKeyIndex = 255;
    }
    return '\0';
  }

  if (data != 0xFFFF) {
    for (uint8_t i = 0; i < 16; i++) {
      if (data & (1 << i)) {
        waitingRelease = true;
        lastKeyIndex = i;
        return keys[i];
      }
    }
  }

  return '\0';
}

