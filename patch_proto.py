import re

with open('src/gembot.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

content = content.replace("void handleTouchAction(bool isHold);", "void handleTouchAction(bool isHold);\nvoid handleTouchRelease();")

with open('src/gembot.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
print("Patch applied")
