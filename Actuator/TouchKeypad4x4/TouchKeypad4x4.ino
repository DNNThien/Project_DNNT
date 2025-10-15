#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define PCF8575_ADDR 0x20  // Địa chỉ của PCF8575 (A0, A1, A2 = GND)
#define LCD_ADDR 0x27

LiquidCrystal_I2C lcd(LCD_ADDR, 20, 4);

char keys[16] = {
  '1', '2', '3', 'A',
  '4', '5', '6', 'B',
  '7', '8', '9', 'C',
  '*', '0', '#', 'D'
};
uint8_t lowByte, highByte;
uint16_t data;

// ===============================
// Hàm hiển thị lên LCD
// ===============================
void displayLCD(int clear, int col, int row, String str) {
  if (clear) lcd.clear();
  lcd.setCursor(col, row);
  lcd.print(str);
}

// ===============================
// Hàm đọc phím nhấn từ PCF8575
// ===============================
char readKey() {
  data = 0xFFFF;

  Wire.requestFrom(PCF8575_ADDR, 2);
  if (Wire.available() == 2) {
    lowByte = Wire.read();
    highByte = Wire.read();
    data = (highByte << 8) | lowByte;
  }

  if (data != 0xFFFF) {
    for (uint8_t i = 0; i < 16; i++) {
      if (data & (1 << i)) {
        char key = keys[i];
        do {
          Wire.requestFrom(PCF8575_ADDR, 2);
          if (Wire.available() == 2) {
            lowByte = Wire.read();
            highByte = Wire.read();
            data = (highByte << 8) | lowByte;
          }
          delay(50);
        } while (data & (1 << i));

        return key;
      }
    }
  }

  return '\0';
}

// ===============================
// Setup
// ===============================
void setup() {
  Serial.begin(9600);
  Wire.begin();
  lcd.init();
  lcd.backlight();

  lcd.clear();
  displayLCD(1, 5, 1, "TOUCH KEYPAD");
  delay(1000);
  displayLCD(1, 6, 1, "YOUR KEY");
  Serial.println("System Ready!");
}

// ===============================
// Vòng lặp chính
// ===============================
void loop() {
  char key = readKey();
  if (key != '\0') {
    Serial.print("Key: ");
    Serial.println(key);
    displayLCD(0, 9, 2, String(key));
  }
  delay(50);
}
