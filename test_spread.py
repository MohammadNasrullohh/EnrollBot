import re

with open('src/gembot.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# I will replace the layout calculation in drawFace
layout_code = '''  uint16_t faceColor = spr.color565(0, 200, 255); // Biru (Cyan-ish Blue)
  float baseScale = 1.8f * breathingScale;
  float verticalSpread = 1.4f; // Spread eyes and mouth vertically

  int faceTotalHeight = (int)(64 * verticalSpread * baseScale);
  int offsetX = (240 - (int)(128 * baseScale)) / 2 + (int)finalLookX;
  int offsetY = (320 - faceTotalHeight) / 2 + (int)finalBobY + (int)finalLookY;

  // Centering offsets
  int eyeCenterOffsetY = (int)((37 * baseScale - 37 * baseScale * currentEyeScaleY) / 2.0f);
  int eyeCenterOffsetX = (int)((26 * baseScale - 26 * baseScale * currentEyeScaleX) / 2.0f);

  int mouthOffsetX = (240 - (int)(128 * baseScale)) / 2 + (int)(finalLookX * 0.8f);
  int mouthOffsetY = (320 - faceTotalHeight) / 2 + (int)(finalBobY * 1.1f) + (int)(finalLookY * 0.8f);
  int mCenterOffsetX = (int)((16 * baseScale - 16 * baseScale * currentMouthScaleX) / 2.0f);
  int mCenterOffsetY = (int)((6 * baseScale - 6 * baseScale * currentMouthScaleY) / 2.0f);

  // Layer 7 (Mouth)
  drawBitmapScaled(mouthOffsetX + (int)(57 * baseScale) + mCenterOffsetX, 
                   mouthOffsetY + (int)(48 * verticalSpread * baseScale) + (int)(currentMouthOffsetY * baseScale) + mCenterOffsetY, 
                   image_Layer_7_bits, 16, 6, 
                   baseScale * currentMouthScaleX, baseScale * currentMouthScaleY, faceColor);

  // Layer 8 (Right Eye)
  drawBitmapScaled(offsetX + (int)(76 * baseScale) + eyeCenterOffsetX, 
                   offsetY + (int)(13 * verticalSpread * baseScale) + (int)(currentEyeOffsetY * baseScale) + eyeCenterOffsetY, 
                   image_Layer_8_bits, 26, 37, 
                   baseScale * currentEyeScaleX, baseScale * currentEyeScaleY, faceColor);
  
  // Layer 9 (Left Eye)
  drawBitmapScaled(offsetX + (int)(28 * baseScale) + eyeCenterOffsetX, 
                   offsetY + (int)(13 * verticalSpread * baseScale) + (int)(currentEyeOffsetY * baseScale) + eyeCenterOffsetY, 
                   image_Layer_9_bits, 26, 37, 
                   baseScale * currentEyeScaleX, baseScale * currentEyeScaleY, faceColor);

  // Pupils (Hitam)
  int pupilRadius = (int)(4 * baseScale * currentPupilScale);
  int pX = (int)(13 * baseScale * currentEyeScaleX); 
  int pY = (int)(18 * baseScale * currentEyeScaleY); 
  
  int pLookX = (int)(finalLookX * 0.6f); 
  int pLookY = (int)(finalLookY * 0.6f);

  if (currentEyeScaleY > 0.2f && currentExpressionId != 50 && currentExpressionId != 51) { 
      spr.fillCircle(offsetX + (int)(28 * baseScale) + eyeCenterOffsetX + pX + pLookX, 
                     offsetY + (int)(13 * verticalSpread * baseScale) + (int)(currentEyeOffsetY * baseScale) + eyeCenterOffsetY + pY + pLookY, 
                     pupilRadius, TFT_BLACK);
      spr.fillCircle(offsetX + (int)(76 * baseScale) + eyeCenterOffsetX + pX + pLookX, 
                     offsetY + (int)(13 * verticalSpread * baseScale) + (int)(currentEyeOffsetY * baseScale) + eyeCenterOffsetY + pY + pLookY, 
                     pupilRadius, TFT_BLACK);
  } else if (currentExpressionId == 50) {
      spr.setTextSize(3);
      spr.setTextColor(TFT_RED);
      spr.drawString("<3", offsetX + (int)(28 * baseScale) + eyeCenterOffsetX + pX + pLookX - 16, 
                          offsetY + (int)(13 * verticalSpread * baseScale) + (int)(currentEyeOffsetY * baseScale) + eyeCenterOffsetY + pY + pLookY - 8);
      spr.drawString("<3", offsetX + (int)(76 * baseScale) + eyeCenterOffsetX + pX + pLookX - 16, 
                          offsetY + (int)(13 * verticalSpread * baseScale) + (int)(currentEyeOffsetY * baseScale) + eyeCenterOffsetY + pY + pLookY - 8);
  } else if (currentExpressionId == 51) {
      spr.setTextSize(3);
      spr.setTextColor(TFT_BLACK);
      spr.drawString(">", offsetX + (int)(28 * baseScale) + eyeCenterOffsetX + pX + pLookX - 8, 
                          offsetY + (int)(13 * verticalSpread * baseScale) + (int)(currentEyeOffsetY * baseScale) + eyeCenterOffsetY + pY + pLookY - 8);
      spr.drawString("<", offsetX + (int)(76 * baseScale) + eyeCenterOffsetX + pX + pLookX - 8, 
                          offsetY + (int)(13 * verticalSpread * baseScale) + (int)(currentEyeOffsetY * baseScale) + eyeCenterOffsetY + pY + pLookY - 8);
  }

  // Tears Animation for Nangis (29)
  if (currentExpressionId == 29 || currentExpressionId == 6) { // Nangis or Sedih
      uint16_t tearColor = spr.color565(0, 150, 255); // Deep light blue
      
      // Left Tear
      int lTearCycle = now % 1500;
      if (lTearCycle < 1000) {
          int lDropScale = (lTearCycle > 800) ? (1000 - lTearCycle) / 200.0f * 3.0f * baseScale : 3.0f * baseScale;
          int lTearY = offsetY + (int)(35 * verticalSpread * baseScale) + (int)((lTearCycle / 1000.0f) * 40 * baseScale);
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
          int rTearY = offsetY + (int)(35 * verticalSpread * baseScale) + (int)((rTearCycle / 900.0f) * 40 * baseScale);
          int rTearX = offsetX + (int)(88 * baseScale) + eyeCenterOffsetX;
          if (rDropScale > 0) {
              spr.fillCircle(rTearX, rTearY, rDropScale, tearColor);
              spr.fillTriangle(rTearX - rDropScale, rTearY, rTearX + rDropScale, rTearY, rTearX, rTearY - rDropScale * 2, tearColor);
          }
      }
  }

  // Draw Chatbot Running Text'''

pattern = re.compile(r'  uint16_t faceColor = spr\.color565\(0, 200, 255\);.*?\n  // Draw Chatbot Running Text', re.DOTALL)
content = pattern.sub(layout_code, content)

with open('src/gembot.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print("Applied spread layout.")
