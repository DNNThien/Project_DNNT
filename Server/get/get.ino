#include <Arduino.h>
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

bool firstStream = false;

void setup()
{

  Serial.begin(115200);

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
  fbdo.setBSSLBufferSize(8192, 4096);
  Firebase.begin(&config, &auth);
  Firebase.setDoubleDigits(5);

  if (Firebase.beginStream(fbdo, "/test/data/SENSOR1")) { // không dùng Firebase.RTDB
    Serial.println("Stream started");
  } else {
     Serial.println(fbdo.errorReason());
  }
}

// Firebase.ready()

void loop() {
  if (Firebase.readStream(fbdo)) { // không dùng Firebase.RTDB
    if (fbdo.streamTimeout()) {
        Serial.println("Stream timeout, resuming...");
    }
    if (fbdo.streamAvailable()) {
      if(!firstStream) {
        firstStream = true;
      } else {
        Serial.printf("Stream path: %s\n", fbdo.streamPath().c_str());
        Serial.printf("Changed node: %s\n", fbdo.dataPath().c_str());
        Serial.printf("Data type: %s\n", fbdo.dataType().c_str());
        Serial.printf("Value: %s\n", fbdo.to<const char*>());
        Serial.println("------------------------------");
      }
    }
  }
}
