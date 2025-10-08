#include <vector>
#include <Wire.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

#define WIFI_SSID "DNNT"
#define WIFI_PASSWORD "12345678"

#define API_KEY "AIzaSyC5DJvOYT-gWT2PAKuIoBS2aEuXfmMTyiA"
#define DATABASE_URL "https://smartagriculture-4edb5-default-rtdb.asia-southeast1.firebasedatabase.app/"

#define USER_EMAIL "test@gmail.com"
#define USER_PASSWORD "12345678"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

void setup()
{
  Serial.begin(115200);
  delay(1000);

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

  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;
  Firebase.reconnectNetwork(true);
  fbdo.setBSSLBufferSize(4096, 1024);
  Firebase.begin(&config, &auth);
  Firebase.setDoubleDigits(5);
  Firebase.deleteNode(fbdo, "/test/system");
  String path = "/test/system/time";
  FirebaseJson json;
  json.set("read", 1);
  json.set("send", 10);
  if(Firebase.ready() && Firebase.set(fbdo, path.c_str(), json)) {
    Serial.println("Set time successfully!");
  } else {
    Serial.println("Set time unsuccessfully!");
  }
}

void loop() 
{}
