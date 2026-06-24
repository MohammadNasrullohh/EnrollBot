import re

with open('src/gembot.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

draw_listening = '''
void drawListening() {
  spr.fillSprite(TFT_BLACK);
  spr.setTextColor(TFT_WHITE);
  spr.setTextDatum(MC_DATUM);
  spr.drawString("Mendengarkan...", 120, 40, 4);

  // Smooth animation using sin()
  unsigned long ms = millis();
  float pulse = (sin(ms * 0.005) + 1.0) / 2.0; // 0.0 to 1.0
  int radius1 = 40 + (pulse * 20);
  int radius2 = 60 + (pulse * 30);
  int radius3 = 80 + (pulse * 40);

  spr.drawCircle(120, 160, radius3, spr.color565(0, 100, 255));
  spr.drawCircle(120, 160, radius2, spr.color565(0, 180, 255));
  spr.fillCircle(120, 160, radius1, spr.color565(0, 255, 255));
  
  spr.setTextColor(TFT_BLACK);
  spr.drawString("MIC", 120, 160, 2);
}

'''

content = content.replace("void drawScreen() {", draw_listening + "void drawScreen() {")

content = content.replace("else if (currentState == APP_MUSIK) drawMusik();", "else if (currentState == APP_MUSIK) drawMusik();\n  else if (currentState == APP_LISTENING) drawListening();")

with open('src/gembot.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
print("Patch applied")
