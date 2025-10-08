#include <Wire.h>
#include <BH1750.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 20, 4);
BH1750 lightMeter;

void displayLCD();

void setup() {
  Serial.begin(115200);
  Wire.begin();
  lightMeter.begin();
  lcd.init();
  lcd.backlight();
  delay(2000);

  Serial.println("Khoi dong cam bien BH1750...");
  displayLCD(1, 5, 1, "LIGHT VALUE");
}

void loop() {
  float lux = lightMeter.readLightLevel();
  
  Serial.print("Cuong do anh sang: ");
  Serial.print(lux);
  Serial.println(" lux");
  displayLCD(0, 7, 2, String(lux));
  delay(1000);
}

void displayLCD(int clear, int col, int row, String msg) {
  if(clear) lcd.clear();
  lcd.setCursor(col, row);
  lcd.print(msg);
}