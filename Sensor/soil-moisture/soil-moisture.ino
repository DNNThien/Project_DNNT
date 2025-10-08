#define SOIL_PIN 34  // chọn chân ADC (ví dụ GPIO34)

void setup() {
  Serial.begin(115200);
}

void loop() {
  int value = analogRead(SOIL_PIN);  // đọc giá trị ADC (0 - 4095)
  
  // Chuyển sang % (0% = khô, 100% = ẩm tối đa)
  int moisturePercent = map(value, 4095, 0, 0, 100);
  
  Serial.print("Gia tri ADC: ");
  Serial.print(value);
  Serial.print(" -> Do am dat: ");
  Serial.print(moisturePercent);
  Serial.println(" %");

  delay(1000);
}
