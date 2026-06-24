import re

with open('src/gembot.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

ws_patch = '''      } else if (text.startsWith("CMD:DFP:")) {
          String dfpCmd = text.substring(8);
          if (dfpCmd.startsWith("PLAY:")) {
              int track = dfpCmd.substring(5).toInt();
              myDFPlayer.playMp3Folder(track);
          } else if (dfpCmd == "STOP") {
              myDFPlayer.stop();
          } else if (dfpCmd == "PAUSE") {
              myDFPlayer.pause();
          } else if (dfpCmd == "RESUME") {
              myDFPlayer.start();
          } else if (dfpCmd.startsWith("VOL:")) {
              int vol = dfpCmd.substring(4).toInt();
              myDFPlayer.volume(vol);
          }
      } else if (text == "CMD:W") {'''

content = content.replace('      } else if (text == "CMD:W") {', ws_patch)

with open('src/gembot.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
print("gembot.cpp patched!")
