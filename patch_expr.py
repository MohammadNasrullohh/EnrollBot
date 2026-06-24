import re

with open('src/gembot.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Replace CMD:D and CMD:E
content = content.replace('currentExpressionId = 1; // Senang', 'currentExpressionId = 51; // Haha')
content = content.replace('currentExpressionId = 24; // Delight', 'currentExpressionId = 50; // Love')

# Add to switch (currentExpressionId)
switch_patch = '''    switch (currentExpressionId) {
        case 50: // Love
            targetEyeScaleY = 0.5f;
            targetMouthScaleX = 1.2f;
            targetMouthScaleY = 1.2f;
            exprBobY = sin(now * 0.005f) * 6.0f; 
            break;
        case 51: // Haha
            targetEyeScaleY = 0.5f;
            targetMouthScaleX = 1.5f;
            targetMouthScaleY = 1.8f;
            exprBobY = sin(now * 0.008f) * 10.0f; 
            break;'''

content = content.replace('    switch (currentExpressionId) {', switch_patch)

# Replace Pupils logic
old_pupils = '''  if (currentEyeScaleY > 0.2f) { 
      spr.fillCircle(offsetX + (int)(28 * baseScale) + eyeCenterOffsetX + pX + pLookX, 
                     offsetY + (int)(13 * baseScale) + (int)(currentEyeOffsetY * baseScale) + eyeCenterOffsetY + pY + pLookY, 
                     pupilRadius, TFT_BLACK);
      spr.fillCircle(offsetX + (int)(76 * baseScale) + eyeCenterOffsetX + pX + pLookX, 
                     offsetY + (int)(13 * baseScale) + (int)(currentEyeOffsetY * baseScale) + eyeCenterOffsetY + pY + pLookY, 
                     pupilRadius, TFT_BLACK);
  }'''

new_pupils = '''  if (currentEyeScaleY > 0.2f && currentExpressionId != 50 && currentExpressionId != 51) { 
      spr.fillCircle(offsetX + (int)(28 * baseScale) + eyeCenterOffsetX + pX + pLookX, 
                     offsetY + (int)(13 * baseScale) + (int)(currentEyeOffsetY * baseScale) + eyeCenterOffsetY + pY + pLookY, 
                     pupilRadius, TFT_BLACK);
      spr.fillCircle(offsetX + (int)(76 * baseScale) + eyeCenterOffsetX + pX + pLookX, 
                     offsetY + (int)(13 * baseScale) + (int)(currentEyeOffsetY * baseScale) + eyeCenterOffsetY + pY + pLookY, 
                     pupilRadius, TFT_BLACK);
  } else if (currentExpressionId == 50) {
      spr.setTextSize(3);
      spr.setTextColor(TFT_RED);
      spr.drawString("<3", offsetX + (int)(28 * baseScale) + eyeCenterOffsetX + pX + pLookX - 16, 
                          offsetY + (int)(13 * baseScale) + (int)(currentEyeOffsetY * baseScale) + eyeCenterOffsetY + pY + pLookY - 8);
      spr.drawString("<3", offsetX + (int)(76 * baseScale) + eyeCenterOffsetX + pX + pLookX - 16, 
                          offsetY + (int)(13 * baseScale) + (int)(currentEyeOffsetY * baseScale) + eyeCenterOffsetY + pY + pLookY - 8);
  } else if (currentExpressionId == 51) {
      spr.setTextSize(3);
      spr.setTextColor(TFT_BLACK);
      spr.drawString(">", offsetX + (int)(28 * baseScale) + eyeCenterOffsetX + pX + pLookX - 8, 
                          offsetY + (int)(13 * baseScale) + (int)(currentEyeOffsetY * baseScale) + eyeCenterOffsetY + pY + pLookY - 8);
      spr.drawString("<", offsetX + (int)(76 * baseScale) + eyeCenterOffsetX + pX + pLookX - 8, 
                          offsetY + (int)(13 * baseScale) + (int)(currentEyeOffsetY * baseScale) + eyeCenterOffsetY + pY + pLookY - 8);
  }'''

content = content.replace(old_pupils, new_pupils)

with open('src/gembot.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
print("gembot.cpp patched with expressions!")
