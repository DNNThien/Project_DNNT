//https://chatgpt.com/share/689d8786-41ac-8001-8814-a2eb96bb1a30
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define PCF8575_ADDR 0x20  // Địa chỉ PCF8575 (A0, A1, A2 = GND)

LiquidCrystal_I2C lcd(0x27, 20, 4);

uint8_t interuptPin = 2;

char keys[16] = {
  '1', '2', '3', 'A',
  '4', '5', '6', 'B',
  '7', '8', '9', 'C',
  '*', '0', '#', 'D'
};

void displayLCD();

void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();
  Wire.begin(); // Khởi động I2C
  pinMode(interuptPin, INPUT);
  delay(1000);
  Serial.println("-----------------------------------");
  Serial.println("Begining...........................");
  displayLCD(1, 7, 1, "HELLO");
  delay(1000);
  displayLCD(1, 6, 1, "YOUR KEY");
  Serial.println(digitalRead(interuptPin));
}

void displayLCD(int clear, int col, int row, String str)
{
  if(clear) lcd.clear();
  lcd.setCursor(col, row);
  lcd.print(str);
}

uint16_t readPCF8575() {
  uint16_t data = 0xFFFF;
  Wire.requestFrom(PCF8575_ADDR, 2);
  if (Wire.available() == 2) {
    uint8_t lowByte = Wire.read();
    uint8_t highByte = Wire.read();
    data = (highByte << 8) | lowByte;
  }
  return data;
}

void loop() {
  uint16_t rawData = readPCF8575();
  if(rawData) {
    Serial.println("Data PCF8575: " + String(rawData));
    Serial.println("Interupt Pin: " + String(digitalRead(interuptPin)));
    for (int i = 0; i < 16; i++) {
      if (rawData & (1 << i)) {
        Serial.print("Key: ");
        Serial.println(keys[i]);
        displayLCD(0, 9, 2, String(keys[i]));
        while(rawData & (1 << i))
        {
          rawData = readPCF8575();
          delay(100);
        }
      }
    }
    Serial.println("------------------------------");
  }
  delay(100);
}
