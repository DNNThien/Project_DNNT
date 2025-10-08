#include <vector>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <FirebaseESP32.h> // Lưu ý: tên include mới
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

#define WIFI_SSID "DNNT"
#define WIFI_PASSWORD "12345678"

#define API_KEY "AIzaSyC5DJvOYT-gWT2PAKuIoBS2aEuXfmMTyiA"
#define DATABASE_URL "https://smartagriculture-4edb5-default-rtdb.asia-southeast1.firebasedatabase.app/"

#define USER_EMAIL "test@gmail.com"
#define USER_PASSWORD "12345678"

#define MY_NAME "GATEWAY"

FirebaseData fbdo;
FirebaseData fbdoStream;
FirebaseAuth auth;
FirebaseConfig config;

String bufferUART = "";
std::vector<String> dataReceive;
bool FlagUARTDataReceiveComplete = false;

void printlnSerial(String str);
void sensorValueProcessing(StaticJsonDocument<200> packet);
uint8_t count = 0;
bool firstStream = false;

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600, SERIAL_8N1, 16, 17);
  delay(1000);
  printlnSerial("Starting...............");

  // Gửi lệnh root qua UART
  String msg;
  StaticJsonDocument<200> packet;
  packet["name"] = MY_NAME;
  packet["type"] = 1;
  packet["cmd"] = "settime";
  packet["read"] = 1;
  packet["send"] = 30;
  serializeJson(packet, msg);
  Serial1.println(msg);
  Serial1.println("done");
  printlnSerial("Command Sent Root: " + msg);

  // Kết nối Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  // Cấu hình Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  fbdo.setBSSLBufferSize(4096, 1024);
  Firebase.setDoubleDigits(5);
  Firebase.deleteNode(fbdo, "/test");

  if(Firebase.ready() && Firebase.beginStream(fbdoStream, "/test/system")) {
    printlnSerial("Stream start monitoring /test/system");
  } else {
    printlnSerial("Error: " + fbdoStream.errorReason());
  }
}

void loop() {
  // Đọc UART
  while(Serial1.available()) {
    char c = Serial1.read();
    if(c == '\n') {
      if(bufferUART == "done") {
        FlagUARTDataReceiveComplete = true;
        bufferUART = "";
        break;
      }
      dataReceive.push_back(bufferUART);
      bufferUART = "";
    } else if(c != '\r') {
      bufferUART += c;
    }
  }

  if(FlagUARTDataReceiveComplete) {
    StaticJsonDocument<200> packet;
    for(uint8_t i = 0; i < dataReceive.size(); i++) {
      deserializeJson(packet, dataReceive[i]);
      if(packet["type"] == 0) {
        sensorValueProcessing(packet);
      }
    }
    count = count == 200 ? 0 : count + 1;
    FlagUARTDataReceiveComplete = false;
    dataReceive.clear();
  }

  if(Firebase.ready() && Firebase.readStream(fbdoStream)) {
    if(fbdoStream.streamTimeout()) {
      printlnSerial("Stream timeout, resuming...");
    }
    if(fbdoStream.streamAvailable()) {
      if(!firstStream) {
        firstStream = true;
      } else {
        Serial.printf("Stream path: %s\n", fbdoStream.streamPath().c_str());
        Serial.printf("Changed node: %s\n", fbdoStream.dataPath().c_str());
        Serial.printf("Data type: %s\n", fbdoStream.dataType().c_str());
        Serial.printf("Value: %s\n", fbdoStream.to<const char*>());
        Serial.println("------------------------------");
        String dataJson = fbdoStream.to<String>();
        StaticJsonDocument<200> packet;
        deserializeJson(packet, dataJson);
        packet["name"] = MY_NAME;
        packet["type"] = 1;
        if(fbdoStream.dataPath() == "/time") {
          packet["cmd"] = "settime";
        }
        serializeJson(packet, dataJson);
        Serial1.println(dataJson);
        Serial1.println("done");
        printlnSerial("Command from Server: " + dataJson);
      }
    }
  }
}

void printlnSerial(String str) {
  Serial.println("------------------------------");
  Serial.println(str);
  Serial.println("------------------------------");
}

void sensorValueProcessing(StaticJsonDocument<200> packet) {
  String path = "/test/data/" + String(packet["name"].as<String>()) + "/" + String(count);
  FirebaseJson json;
  json.set("teperature", packet["tmp"].as<int>());
  json.set("humidity", packet["hum"].as<int>());
  json.set("lumimosity", packet["lux"].as<int>());
  json.set("soil-moisture", packet["som"].as<int>());

  if(Firebase.ready() && Firebase.set(fbdo, path.c_str(), json)) {
    printlnSerial("Sent data #" + String(count) + " of " + String(packet["name"].as<String>()));
  } else {
    printlnSerial("Send data of " + String(packet["name"].as<String>()) + " failed: " + String(fbdo.errorReason()));
  }
}