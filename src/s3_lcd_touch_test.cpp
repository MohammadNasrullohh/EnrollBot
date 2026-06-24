#include <Arduino.h>
#include <TFT_eSPI.h>

#define TOUCH_PIN 7

TFT_eSPI display = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("GEMBOT2 S3 LCD TOUCH TEST");
  Serial.printf("TFT pins: MISO=13 MOSI=11 SCLK=12 CS=10 DC=9 RST=14 TOUCH=7\n");
  pinMode(TOUCH_PIN, INPUT_PULLDOWN);
  display.init();
  display.setRotation(0);
  display.fillScreen(TFT_BLACK);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawString("GEMBOT2", 30, 40, 4);
  display.drawString("LCD TEST", 30, 80, 2);
  Serial.println("display init done");
}

void loop() {
  static uint8_t color = 0;
  static unsigned long lastColor = 0;
  unsigned long now = millis();
  int touch = digitalRead(TOUCH_PIN);
  Serial.printf("touch=%d\n", touch);
  if (now - lastColor > 1000) {
    lastColor = now;
    color = (color + 1) % 4;
    if (color == 0) display.fillScreen(TFT_RED);
    else if (color == 1) display.fillScreen(TFT_GREEN);
    else if (color == 2) display.fillScreen(TFT_BLUE);
    else display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_WHITE, color == 3 ? TFT_BLACK : TFT_BLUE);
    display.drawString("GEMBOT2", 30, 40, 4);
    display.drawString(touch ? "TOUCH ON" : "TOUCH OFF", 30, 90, 2);
  }
  delay(250);
}
