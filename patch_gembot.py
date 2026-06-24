import re

with open('src/gembot.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# 1. Add includes and global variables
include_str = '''#include "secrets.h"

#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>

HardwareSerial mySoftwareSerial(2); // Use UART2
DFRobotDFPlayerMini myDFPlayer;
'''
content = content.replace('#include "secrets.h"', include_str)

# 2. Add to setup()
setup_str = '''void setup() {
  Serial.begin(115200);

  mySoftwareSerial.begin(9600, SERIAL_8N1, 16, 17); // RX2=16, TX2=17
  if (!myDFPlayer.begin(mySoftwareSerial)) {
    Serial.println(F("Unable to begin DFPlayer:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
  } else {
    Serial.println(F("DFPlayer Mini online."));
    myDFPlayer.volume(20);  // Set volume value. From 0 to 30
  }
'''
content = content.replace('void setup() {\n  Serial.begin(115200);', setup_str)

# 3. Replace APP_MUSIK logic
old_musik = '''      if (musicCursor == musicCount - 1) {
        currentState = APP_MENU;
      } else {
        String cmd = (musicCursor == 2) ? "CMD:TEST_MAX" : (String("CMD:PLAY:") + String(musicCursor + 1));
        webSocket.sendTXT(cmd);
        currentState = APP_FACE;
      }'''

new_musik = '''      if (musicCursor == musicCount - 1) {
        currentState = APP_MENU;
      } else {
        if (musicCursor == 0) {
            myDFPlayer.playMp3Folder(2); // Asumsi MBG di 0002.mp3
        } else if (musicCursor == 1) {
            myDFPlayer.playMp3Folder(1); // User bilang Love Story di 0001.mp3
        } else if (musicCursor == 2) {
            myDFPlayer.playMp3Folder(3); // Test Max di 0003.mp3
        }
        currentState = APP_FACE;
      }'''

content = content.replace(old_musik, new_musik)

with open('src/gembot.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
print("Patched gembot.cpp")
