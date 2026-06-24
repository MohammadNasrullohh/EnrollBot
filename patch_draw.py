import re

with open('src/gembot.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

bad = '''      } else if (text == "CMD:W") {
         isDrawMode = true;
      } else if (text.startsWith("CMD:T:")) {'''

good = '''      } else if (text == "CMD:W") {
         isDrawMode = true;
         currentState = APP_DRAW;
         spr.fillSprite(TFT_BLACK);
         spr.pushSprite(0, 0);
      } else if (text.startsWith("CMD:T:")) {'''

content = content.replace(bad, good)

with open('src/gembot.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
