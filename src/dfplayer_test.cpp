#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <DFRobotDFPlayerMini.h>

#define DFPLAYER_RX_PIN D6      // Normal: XIAO receives from DFPlayer TX
#define DFPLAYER_TX_PIN D7      // Normal: XIAO sends to DFPlayer RX through 1K resistor
#define DFPLAYER_ALT_RX_PIN D7  // Fallback if TX/RX are swapped
#define DFPLAYER_ALT_TX_PIN D6
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C

#define OLED_WHITE SH110X_WHITE
#define OLED_BLACK SH110X_BLACK

HardwareSerial dfSerial(1);
DFRobotDFPlayerMini dfPlayer;
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

uint8_t currentTrack = 1;
unsigned long lastAutoPlayMs = 0;
bool oledReady = false;
bool dfReady = false;

void oledMessage(const char* title, const char* line1, const char* line2 = "") {
  if (!oledReady) return;
  display.clearDisplay();
  display.setTextColor(OLED_WHITE);
  display.setTextWrap(false);
  display.setTextSize(1);
  display.fillRect(0, 0, SCREEN_WIDTH, 12, OLED_WHITE);
  display.setTextColor(OLED_BLACK);
  display.setCursor(3, 2);
  display.print(title);
  display.setTextColor(OLED_WHITE);
  display.setTextSize(1);
  display.setCursor(4, 22);
  display.print(line1);
  display.setCursor(4, 38);
  display.print(line2);
  display.display();
}

void printHelp() {
  Serial.println();
  Serial.println("DFPlayer test ready");
  Serial.println("Wiring:");
  Serial.println("  DFPlayer TX  -> XIAO D6");
  Serial.println("  DFPlayer RX  -> XIAO D7 lewat resistor 1K");
  Serial.println("  DFPlayer VCC -> 5V/VBUS");
  Serial.println("  DFPlayer GND -> GND");
  Serial.println("  Speaker      -> SPK1 + SPK2");
  Serial.println();
  Serial.println("Serial commands:");
  Serial.println("  1/2/3 = play track");
  Serial.println("  n     = next");
  Serial.println("  p     = previous");
  Serial.println("  s     = stop");
  Serial.println("  +     = volume up");
  Serial.println("  -     = volume down");
  Serial.println();
}

void playTrack(uint8_t track) {
  currentTrack = track;
  Serial.print("Play /mp3/");
  if (track < 10) Serial.print("000");
  else if (track < 100) Serial.print("00");
  else if (track < 1000) Serial.print("0");
  Serial.print(track);
  Serial.println(".mp3");
  dfPlayer.playMp3Folder(track);
  char line[20];
  snprintf(line, sizeof(line), "PLAY %04u.mp3", track);
  oledMessage("DFPLAYER OK", line, "speaker check");
}

void setup() {
  Serial.begin(115200);
  delay(1200);
  printHelp();

  Wire.begin(D4, D5);
  oledReady = display.begin(OLED_ADDR, true);
  if (oledReady) {
    display.clearDisplay();
    display.display();
    oledMessage("DFPLAYER TEST", "Mencari module...", "TX D6  RX D7");
  } else {
    Serial.println("OLED ERR");
  }

  Serial.println("Mencari DFPlayer normal TX->D6 RX->D7...");
  oledMessage("DFPLAYER TEST", "Coba TX>D6 RX>D7", "tunggu...");
  dfSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  delay(1200);
  dfReady = dfPlayer.begin(dfSerial, false, true);

  if (!dfReady) {
    dfSerial.end();
    delay(300);
    Serial.println("Normal gagal, coba kabel kebalik TX->D7 RX->D6...");
    oledMessage("DFPLAYER TEST", "Coba TX>D7 RX>D6", "fallback...");
    dfSerial.begin(9600, SERIAL_8N1, DFPLAYER_ALT_RX_PIN, DFPLAYER_ALT_TX_PIN);
    delay(1200);
    dfReady = dfPlayer.begin(dfSerial, false, true);
  }

  if (!dfReady) {
    Serial.println("DFPlayer ERR");
    Serial.println("Cek: VCC 5V/VBUS, GND, TX/RX, microSD FAT32, folder /mp3/0001.mp3.");
    oledMessage("DFPLAYER ERR", "Cek VCC/GND/SD", "coba tukar TX RX");
    while (true) {
      delay(1000);
      Serial.println("DFPlayer belum kebaca...");
    }
  }

  Serial.println("DFPlayer OK");
  oledMessage("DFPLAYER OK", "Module terbaca", "Play track 1");
  dfPlayer.volume(22);
  delay(300);
  playTrack(1);
  lastAutoPlayMs = millis();
}

void loop() {
  if (dfPlayer.available()) {
    Serial.print("DF event type=");
    Serial.print(dfPlayer.readType());
    Serial.print(" value=");
    Serial.println(dfPlayer.read());
  }

  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '1') playTrack(1);
    else if (c == '2') playTrack(2);
    else if (c == '3') playTrack(3);
    else if (c == 'n') {
      currentTrack++;
      if (currentTrack > 3) currentTrack = 1;
      playTrack(currentTrack);
    } else if (c == 'p') {
      currentTrack = currentTrack <= 1 ? 3 : currentTrack - 1;
      playTrack(currentTrack);
    } else if (c == 's') {
      Serial.println("Stop");
      dfPlayer.stop();
    } else if (c == '+') {
      Serial.println("Volume +");
      dfPlayer.volumeUp();
    } else if (c == '-') {
      Serial.println("Volume -");
      dfPlayer.volumeDown();
    }
  }

  if (millis() - lastAutoPlayMs > 10000UL) {
    lastAutoPlayMs = millis();
    currentTrack++;
    if (currentTrack > 3) currentTrack = 1;
    playTrack(currentTrack);
  }
}
