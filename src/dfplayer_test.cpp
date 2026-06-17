#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <DFRobotDFPlayerMini.h>

// DFPlayer Mini diagnostic firmware for XIAO ESP32-C3.
//
// Wiring:
//   DFPlayer TX  -> XIAO D7 / RX
//   DFPlayer RX  -> XIAO D6 / TX
//   DFPlayer VCC -> XIAO 5V / VBUS
//   DFPlayer GND -> XIAO GND
//   Speaker      -> DFPlayer SPK1 and SPK2
//   SD card      -> FAT32, file at /mp3/0001.mp3
#define DFPLAYER_RX_PIN D7
#define DFPLAYER_TX_PIN D6

#define OLED_SDA_PIN D4
#define OLED_SCL_PIN D5
#define OLED_ADDR 0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

HardwareSerial dfSerial(1);
DFRobotDFPlayerMini dfPlayer;
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool oledReady = false;
bool dfReady = false;
uint8_t volumeLevel = 20;
unsigned long lastReportMs = 0;

const char* eventName(uint8_t type) {
  switch (type) {
    case TimeOut: return "TimeOut";
    case WrongStack: return "WrongStack";
    case DFPlayerCardInserted: return "CardInserted";
    case DFPlayerCardRemoved: return "CardRemoved";
    case DFPlayerCardOnline: return "CardOnline";
    case DFPlayerPlayFinished: return "PlayFinished";
    case DFPlayerError: return "DFPlayerError";
    case DFPlayerUSBInserted: return "USBInserted";
    case DFPlayerUSBRemoved: return "USBRemoved";
    case DFPlayerUSBOnline: return "USBOnline";
    case DFPlayerCardUSBOnline: return "CardUSBOnline";
    case DFPlayerFeedBack: return "FeedBack";
    default: return "Unknown";
  }
}

const char* errorName(int value) {
  switch (value) {
    case Busy: return "Busy";
    case Sleeping: return "Sleeping";
    case SerialWrongStack: return "SerialWrongStack";
    case CheckSumNotMatch: return "CheckSumNotMatch";
    case FileIndexOut: return "FileIndexOut";
    case FileMismatch: return "FileMismatch";
    case Advertise: return "Advertise";
    default: return "UnknownError";
  }
}

void showStatus(const char* title, const char* line1, const char* line2 = "", const char* line3 = "") {
  if (!oledReady) return;

  display.clearDisplay();
  display.setTextWrap(false);
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.print(title);
  display.drawLine(0, 11, 127, 11, SH110X_WHITE);
  display.setCursor(0, 18);
  display.print(line1);
  display.setCursor(0, 32);
  display.print(line2);
  display.setCursor(0, 46);
  display.print(line3);
  display.display();
}

void printHelp() {
  Serial.println();
  Serial.println("=== DFPlayer + SD Card Diagnostic ===");
  Serial.println("Wiring wajib:");
  Serial.println("  DFPlayer TX  -> XIAO D7 / RX");
  Serial.println("  DFPlayer RX  -> XIAO D6 / TX");
  Serial.println("  DFPlayer VCC -> XIAO 5V/VBUS");
  Serial.println("  DFPlayer GND -> XIAO GND");
  Serial.println("  Speaker merah/hitam -> SPK1/SPK2");
  Serial.println("SD wajib: FAT32, /mp3/0001.mp3");
  Serial.println();
  Serial.println("Serial commands:");
  Serial.println("  p = play /mp3/0001.mp3");
  Serial.println("  c = cek SD/file count");
  Serial.println("  s = stop");
  Serial.println("  + = volume up");
  Serial.println("  - = volume down");
  Serial.println();
}

int safeReadVolume() {
  int value = dfPlayer.readVolume();
  delay(120);
  return value;
}

int safeReadState() {
  int value = dfPlayer.readState();
  delay(120);
  return value;
}

int safeReadFileCounts() {
  int value = dfPlayer.readFileCounts(DFPLAYER_DEVICE_SD);
  delay(160);
  return value;
}

void checkSdCard() {
  if (!dfReady) {
    Serial.println("[CHECK] DFPlayer belum ready");
    showStatus("DF ERR", "module not ready", "cek wiring power", "");
    return;
  }

  Serial.println();
  Serial.println("[CHECK] Query DFPlayer...");
  showStatus("DF CHECK", "query volume/state", "query SD count", "");

  int vol = safeReadVolume();
  int state = safeReadState();
  int total = safeReadFileCounts();

  Serial.print("[CHECK] readVolume=");
  Serial.println(vol);
  Serial.print("[CHECK] readState=");
  Serial.println(state);
  Serial.print("[CHECK] SD total files=");
  Serial.println(total);

  char l1[22];
  char l2[22];
  char l3[22];
  snprintf(l1, sizeof(l1), "vol=%d state=%d", vol, state);
  snprintf(l2, sizeof(l2), "sd files=%d", total);
  snprintf(l3, sizeof(l3), "need /mp3/0001.mp3");
  showStatus("DF CHECK", l1, l2, l3);

  if (total <= 0) {
    Serial.println("[CHECK] SD belum terbaca atau file tidak terindeks.");
    Serial.println("[CHECK] Format SD ke FAT32, buat folder mp3, isi 0001.mp3.");
  } else {
    Serial.println("[CHECK] SD terlihat ada file. Lanjut play 0001.");
  }
}

void play0001() {
  if (!dfReady) {
    Serial.println("[PLAY] DFPlayer belum ready");
    return;
  }

  dfPlayer.volume(volumeLevel);
  delay(150);
  Serial.print("[PLAY] /mp3/0001.mp3 volume=");
  Serial.println(volumeLevel);
  dfPlayer.playMp3Folder(1);
  showStatus("DF PLAY", "/mp3/0001.mp3", "listen speaker", "event prints below");
}

void setup() {
  Serial.begin(115200);
  delay(1600);
  printHelp();

  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  oledReady = display.begin(OLED_ADDR, true);
  if (oledReady) {
    display.clearDisplay();
    display.display();
  }
  showStatus("DF START", "detecting module", "TX>D7 RX>D6", "baud 9600");

  dfSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  delay(1300);

  // ACK off keeps this test usable with common DFPlayer clones whose feedback
  // packets are noisy, but commands and SD queries are still printed below.
  dfReady = dfPlayer.begin(dfSerial, false, true);
  if (!dfReady) {
    Serial.println("[INIT] DFPlayer ERR: module tidak kebaca.");
    Serial.println("[INIT] Cek: VCC 5V, GND, TX->D7, RX->D6, dan modul LED nyala.");
    showStatus("DF INIT ERR", "module not found", "TX>D7 RX>D6", "VCC 5V + GND");
    return;
  }

  dfPlayer.setTimeOut(800);
  dfPlayer.outputDevice(DFPLAYER_DEVICE_SD);
  delay(250);
  dfPlayer.EQ(DFPLAYER_EQ_NORMAL);
  delay(120);
  dfPlayer.volume(volumeLevel);
  delay(250);

  Serial.println("[INIT] DFPlayer command mode OK (ACK off)");
  showStatus("DF INIT OK", "module detected", "checking SD", "");
  checkSdCard();
  delay(900);
  play0001();
  lastReportMs = millis();
}

void loop() {
  if (dfReady && dfPlayer.available()) {
    uint8_t type = dfPlayer.readType();
    int value = dfPlayer.read();

    Serial.print("[EVENT] type=");
    Serial.print(type);
    Serial.print(" ");
    Serial.print(eventName(type));
    Serial.print(" value=");
    Serial.print(value);
    if (type == DFPlayerError) {
      Serial.print(" ");
      Serial.print(errorName(value));
    }
    Serial.println();

    if (type == DFPlayerCardOnline || type == DFPlayerCardInserted) {
      showStatus("SD ONLINE", "card detected", "press c to check", "press p to play");
    } else if (type == DFPlayerCardRemoved) {
      showStatus("SD REMOVED", "card removed", "insert microSD", "");
    } else if (type == DFPlayerPlayFinished) {
      showStatus("DF DONE", "track finished", "press p replay", "");
    } else if (type == DFPlayerError) {
      char line[24];
      snprintf(line, sizeof(line), "err=%d %s", value, errorName(value));
      showStatus("DF ERROR", line, "cek SD/file/wiring", "");
    }
  }

  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == 'p' || c == 'P') {
      play0001();
    } else if (c == 'c' || c == 'C') {
      checkSdCard();
    } else if (c == 's' || c == 'S') {
      dfPlayer.stop();
      Serial.println("[STOP]");
      showStatus("DF STOP", "stopped", "press p to play", "");
    } else if (c == '+') {
      if (volumeLevel < 30) volumeLevel++;
      dfPlayer.volume(volumeLevel);
      Serial.print("[VOL] ");
      Serial.println(volumeLevel);
    } else if (c == '-') {
      if (volumeLevel > 0) volumeLevel--;
      dfPlayer.volume(volumeLevel);
      Serial.print("[VOL] ");
      Serial.println(volumeLevel);
    }
  }

  if (dfReady && millis() - lastReportMs > 10000UL) {
    lastReportMs = millis();
    Serial.println("[ALIVE] DFPlayer diagnostic running. p=play, c=check, +/- volume.");
  }
}
