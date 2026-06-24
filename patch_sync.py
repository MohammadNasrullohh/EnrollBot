import re

with open('src/gembot.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

bad_cmd_c = '''      } else if (text == "CMD:C" || text == "CMD:CLEAR") {
         isDrawMode = false;
         currentExpressionId = -1;
      } else if (text == "CMD:P") {'''

good_cmd_c = '''      } else if (text == "CMD:C" || text == "CMD:CLEAR") {
         isDrawMode = false;
         currentState = APP_FACE;
         currentExpressionId = -1;
      } else if (text == "CMD:P") {'''

content = content.replace(bad_cmd_c, good_cmd_c)

bad_cmd_w = '''     } else if (text == "CMD:W") {
        isDrawMode = true;
        currentState = APP_DRAW;
        spr.fillSprite(TFT_BLACK);
        spr.pushSprite(0, 0);
     } else if (text.startsWith("CMD:DRW:")) {'''

good_cmd_w = '''     } else if (text.startsWith("CMD:W")) {
        isDrawMode = true;
        currentState = APP_DRAW;
        uint16_t bgColor = TFT_BLACK;
        if (text.length() > 5 && text[5] == ':') {
            bgColor = text.substring(6).toInt();
        }
        spr.fillSprite(bgColor);
        spr.pushSprite(0, 0);
     } else if (text.startsWith("CMD:DRW:")) {'''

content = content.replace(bad_cmd_w, good_cmd_w)

# Also fix the CMD:M to ensure it sets currentState = APP_FACE
bad_cmd_m = '''       } else if (text.startsWith("CMD:M")) {
        isDrawMode = false;
        String idStr = text.substring(5);'''

good_cmd_m = '''       } else if (text.startsWith("CMD:M")) {
        isDrawMode = false;
        currentState = APP_FACE;
        String idStr = text.substring(5);'''

content = content.replace(bad_cmd_m, good_cmd_m)

with open('src/gembot.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
