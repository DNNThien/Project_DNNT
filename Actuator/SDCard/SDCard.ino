#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>

File myFile;
StaticJsonDocument<150> sys;
String doc;

bool writeFile(const char *mode, String content, String fileName, String folderName = "") {
  if(folderName.length() != 0) {
    SD.mkdir(folderName);
  }
  String path = folderName + fileName;
  myFile = SD.open(path, mode);
  if(myFile) {
    myFile.println(content);
    myFile.close();
    return true;
  }
  return false;
}

bool readFile(String path) {
  Serial.println("Content of file " + path);
  String doc;
  myFile = SD.open(path);
  if(myFile) {
    while(myFile.available()) {
      doc += (char) myFile.read();
    }
    Serial.println(doc);
    Serial.println("--------------------------");
    myFile.close();
    return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Initializing SD card...");
  if (!SD.begin(5)) {
    Serial.println("initialization failed!");
    while (1);
  }
  Serial.println("initialization done.");
  
  sys["SENSOR1"] = -1;
  sys["SENSOR2"] = -1;
  sys["SENSOR3"] = -1;

  serializeJsonPretty(sys, doc);
  writeFile(FILE_WRITE, doc, "/nodes.txt", "/system");

  sys.clear();
  JsonObject time = sys.createNestedObject("time");
  time["read"] = 1; //giay
  time["send"] = 30; //giay
  time["screen"] = 120; //giay
  JsonObject threshold = sys.createNestedObject("threshold");
  JsonObject tmpThresh = threshold.createNestedObject("tmp");
  tmpThresh["up"] = 35;
  tmpThresh["down"] = 28;
  JsonObject humThresh = threshold.createNestedObject("hum");
  humThresh["up"] = 60;
  humThresh["down"] = 40;
  JsonObject somThresh = threshold.createNestedObject("som");
  somThresh["up"] = 60;
  somThresh["down"] = 40;
  JsonObject luxThresh = threshold.createNestedObject("lux");
  luxThresh["up"] = 30;
  luxThresh["down"] = 10;
  serializeJsonPretty(sys, doc);
  writeFile(FILE_WRITE, doc, "/parameters.txt", "/system");

  readFile("/system/nodes.txt");
  readFile("/system/parameters.txt");
}

void loop()
{}