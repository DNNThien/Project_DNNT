#include "DHT.h"

#define DHTPIN 19      // chân data của DHT22 nối vào pin số 2 Arduino
#define DHTTYPE DHT22 // định nghĩa loại cảm biến (DHT11 hoặc DHT22)

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  dht.begin(); // khởi động cảm biến
}

void loop() {
  delay(2000); // DHT22 cần khoảng 2 giây để lấy dữ liệu mới

  float h = dht.readHumidity();    // đọc độ ẩm
  float t = dht.readTemperature(); // đọc nhiệt độ C

  // kiểm tra nếu dữ liệu không hợp lệ (NaN)
  if (isnan(h) || isnan(t)) {
    Serial.println("Loi khi doc cam bien DHT22!");
    return;
  }

  Serial.print("Nhiet do: ");
  Serial.print(t);
  Serial.print(" *C, Do am: ");
  Serial.print(h);
  Serial.println(" %");
}
