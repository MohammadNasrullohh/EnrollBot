import re

with open('src/gembot.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

release_func = '''
void handleTouchRelease() {
  if (currentState == APP_LISTENING && voiceRecording) {
    voiceRecording = false;
    if (webSocket.isConnected()) webSocket.sendTXT("{\\"event\\":\\"stop_record\\"}");
    currentState = APP_FACE;
  }
}
'''

content = content.replace("void handleTouch() {", release_func + "\nvoid handleTouch() {")

old_touch_logic = '''    if (touchStartTime > 0) {
      if (!touchHandled && (now - touchStartTime > 20)) {
        handleTouchAction(false); // Trigger tap action on release
      }
      touchStartTime = 0;'''

new_touch_logic = '''    if (touchStartTime > 0) {
      if (!touchHandled && (now - touchStartTime > 20)) {
        handleTouchAction(false); // Trigger tap action on release
      } else if (touchHandled) {
        handleTouchRelease();
      }
      touchStartTime = 0;'''

content = content.replace(old_touch_logic, new_touch_logic)

with open('src/gembot.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
print("Patch applied")
