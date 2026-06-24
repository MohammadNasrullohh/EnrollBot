import re

with open('src/gembot.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

bad_touch = '''  void handleTouchAction(bool isHold) {
    if (currentState == APP_FACE) {'''

good_touch = '''  void handleTouchAction(bool isHold) {
    if (currentState == APP_DRAW && !isHold) {
        currentState = APP_FACE;
        isDrawMode = false;
        return;
    }
    if (currentState == APP_FACE) {'''

content = content.replace(bad_touch, good_touch)

with open('src/gembot.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
