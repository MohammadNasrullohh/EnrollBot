import re

with open('src/gembot.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

tears_code = '''  }

  // Tears Animation for Nangis (29)
  if (currentExpressionId == 29 || currentExpressionId == 6) { // Nangis or Sedih
      uint16_t tearColor = spr.color565(0, 150, 255); // Deep light blue
      
      // Left Tear
      int lTearCycle = now % 1500;
      if (lTearCycle < 1000) {
          int lDropScale = (lTearCycle > 800) ? (1000 - lTearCycle) / 200.0f * 3.0f * baseScale : 3.0f * baseScale;
          int lTearY = offsetY + (int)(35 * baseScale) + (int)((lTearCycle / 1000.0f) * 40 * baseScale);
          int lTearX = offsetX + (int)(40 * baseScale) + eyeCenterOffsetX;
          if (lDropScale > 0) {
              spr.fillCircle(lTearX, lTearY, lDropScale, tearColor);
              spr.fillTriangle(lTearX - lDropScale, lTearY, lTearX + lDropScale, lTearY, lTearX, lTearY - lDropScale * 2, tearColor);
          }
      }
      
      // Right Tear
      int rTearCycle = (now + 600) % 1300;
      if (rTearCycle < 900) {
          int rDropScale = (rTearCycle > 700) ? (900 - rTearCycle) / 200.0f * 3.0f * baseScale : 3.0f * baseScale;
          int rTearY = offsetY + (int)(35 * baseScale) + (int)((rTearCycle / 900.0f) * 40 * baseScale);
          int rTearX = offsetX + (int)(88 * baseScale) + eyeCenterOffsetX;
          if (rDropScale > 0) {
              spr.fillCircle(rTearX, rTearY, rDropScale, tearColor);
              spr.fillTriangle(rTearX - rDropScale, rTearY, rTearX + rDropScale, rTearY, rTearX, rTearY - rDropScale * 2, tearColor);
          }
      }
  }

  // Draw Chatbot Running Text'''

content = content.replace('  }\n\n  // Draw Chatbot Running Text', tears_code)

with open('src/gembot.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
print("Patch applied")
