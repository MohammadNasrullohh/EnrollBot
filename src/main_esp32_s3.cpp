#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI display = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  Serial.println("INIT DISPLAY TEST");
  
  display.init();
  display.setRotation(1); // Landscape
  display.fillScreen(TFT_BLACK);
  Serial.println("DISPLAY INIT OK");
}

void loop() {
  Serial.println("RED");
  display.fillScreen(TFT_RED);
  delay(1000);
  
  Serial.println("GREEN");
  display.fillScreen(TFT_GREEN);
  delay(1000);
  
  Serial.println("BLUE");
  display.fillScreen(TFT_BLUE);
  delay(1000);
}
