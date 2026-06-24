import re

with open('src/gembot.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Fix 1: Map expression 30 (Pusing)
content = content.replace("case 11: case 23: // Woozy / Melt", "case 11: case 23: case 30: // Woozy / Melt / Pusing")

# Fix 2: Map expression 31 (Nakal)
content = content.replace("case 8: case 12: // Smug / Cheeky", "case 8: case 12: case 31: // Smug / Cheeky / Nakal")

# Fix 3: Ensure exiting draw mode returns to APP_FACE
old_cmd_m = '''      } else if (text.startsWith("CMD:M")) {
         isDrawMode = false;
         String idStr = text.substring(5);'''

new_cmd_m = '''      } else if (text.startsWith("CMD:M")) {
         isDrawMode = false;
         currentState = APP_FACE;
         String idStr = text.substring(5);'''

content = content.replace(old_cmd_m, new_cmd_m)

old_cmd_c = '''      } else if (text == "CMD:C" || text == "CMD:CLEAR") {
         isDrawMode = false;
         currentExpressionId = -1;'''

new_cmd_c = '''      } else if (text == "CMD:C" || text == "CMD:CLEAR") {
         isDrawMode = false;
         currentState = APP_FACE;
         currentExpressionId = -1;'''

content = content.replace(old_cmd_c, new_cmd_c)

with open('src/gembot.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print("gembot.cpp patched.")
