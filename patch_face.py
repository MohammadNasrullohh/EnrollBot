import re

with open('src/gembot.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Replace the switch case and lerp speed
new_switch = '''
  switch (currentExpressionId) {
      case 0: // Normal
          break;
      case 1: case 7: case 17: case 24: // Senang / Excited / Party / Delight
          targetEyeScaleY = 0.4f;
          targetMouthScaleX = 1.5f;
          targetMouthScaleY = 1.5f;
          targetMouthOffsetY = -4.0f;
          exprBobY = sin(now * 0.005f) * 6.0f; 
          break;
      case 2: case 10: case 18: // Love / Cozy / Relieved
          targetEyeScaleY = 0.4f;
          targetEyeScaleX = 1.2f;
          targetMouthScaleX = 1.2f;
          targetMouthOffsetY = 2.0f;
          exprBobY = sin(now * 0.003f) * 4.0f;
          break;
      case 3: case 27: // Marah / Grumpy
          targetEyeScaleY = 0.35f;
          targetEyeOffsetY = 5.0f;
          targetMouthScaleX = 0.7f;
          targetMouthScaleY = 0.5f;
          targetMouthOffsetY = 8.0f;
          targetPupilScale = 0.6f;
          break;
      case 4: case 22: case 28: // Kaget / Wow / Amazed
          targetEyeScaleX = 1.3f;
          targetEyeScaleY = 1.3f;
          targetMouthScaleX = 0.5f;
          targetMouthScaleY = 2.5f;
          targetMouthOffsetY = 5.0f;
          targetPupilScale = 0.5f;
          break;
      case 5: case 15: // Ngantuk / Bored
          targetEyeScaleY = 0.15f;
          targetMouthScaleX = 0.8f;
          targetPupilScale = 0.9f;
          break;
      case 6: case 29: // Sedih / Nangis
          targetEyeScaleY = 0.5f;
          targetEyeOffsetY = 8.0f;
          targetMouthScaleX = 0.6f;
          targetMouthScaleY = 0.8f;
          targetMouthOffsetY = 12.0f;
          targetPupilScale = 0.8f;
          exprLookY = 6.0f; 
          break;
      case 8: case 12: // Smug / Cheeky
          targetEyeScaleY = 0.45f;
          targetMouthScaleX = 1.4f;
          targetMouthOffsetY = -6.0f;
          exprLookX = 8.0f;
          exprLookY = -4.0f;
          break;
      case 9: case 25: // Takut / Guilty
          targetEyeScaleX = 0.8f;
          targetEyeScaleY = 0.8f;
          targetEyeOffsetY = -4.0f;
          targetMouthScaleX = 0.5f;
          targetMouthScaleY = 0.5f;
          targetPupilScale = 0.4f;
          exprLookX = sin(now * 0.04f) * 4.0f; // Shivering
          break;
      case 11: case 23: // Woozy / Melt
          targetEyeScaleX = 0.6f;
          targetEyeScaleY = 0.3f;
          targetMouthScaleX = 1.8f;
          targetMouthScaleY = 0.3f;
          exprLookX = sin(now * 0.015f) * 12.0f;
          exprLookY = cos(now * 0.015f) * 12.0f;
          break;
      case 13: case 20: // Bashful / Giggle
          targetEyeScaleY = 0.3f;
          targetMouthScaleX = 1.2f;
          targetMouthScaleY = 0.8f;
          targetMouthOffsetY = -2.0f;
          exprBobY = sin(now * 0.01f) * 3.0f;
          break;
      case 14: case 21: // Focus / Determined
          targetEyeScaleX = 0.9f;
          targetEyeScaleY = 0.7f;
          targetEyeOffsetY = 4.0f;
          targetMouthScaleX = 0.6f;
          targetMouthOffsetY = 5.0f;
          targetPupilScale = 0.7f;
          break;
      case 16: // Nope
          targetEyeScaleX = 0.9f;
          targetEyeScaleY = 0.5f;
          targetEyeOffsetY = 2.0f;
          targetMouthScaleX = 0.4f;
          targetMouthOffsetY = 10.0f;
          exprLookX = -12.0f;
          break;
      case 19: // Suspicious
          targetEyeScaleY = 0.4f;
          targetEyeScaleX = 0.8f;
          targetEyeOffsetY = 5.0f;
          targetMouthScaleX = 0.7f;
          exprLookX = 14.0f;
          targetPupilScale = 0.8f;
          break;
      case 26: // Daydream
          targetEyeScaleY = 0.5f;
          targetEyeScaleX = 0.9f;
          targetMouthScaleX = 0.8f;
          exprLookY = -12.0f;
          exprBobY = sin(now * 0.003f) * 5.0f;
          break;
      case 50: // Heart / Curious
          targetEyeScaleX = 1.1f;
          targetEyeScaleY = 1.1f;
          targetMouthScaleX = 1.2f;
          targetMouthOffsetY = 2.0f;
          exprBobY = sin(now * 0.006f) * 4.0f;
          break;
      case 51: // Haha / Nod
          targetEyeScaleY = 0.3f;
          targetMouthScaleX = 1.5f;
          targetMouthScaleY = 1.5f;
          targetMouthOffsetY = -4.0f;
          exprBobY = sin(now * 0.015f) * 8.0f;
          break;
      default: // Normal
          break;
  }
'''

content = re.sub(r'switch \(currentExpressionId\) \{.*?\n  \}', new_switch.strip(), content, flags=re.DOTALL)

lerp_logic = '''
  // LERP for ultra-smooth morphing (GIF-like organic feel)
  float lerpSpeed = 0.05f; 
  float eyeLerpSpeed = 0.05f;

  if (targetEyeScaleY <= 0.1f) {
      eyeLerpSpeed = 0.3f; // Blink close fast
  } else if (currentEyeScaleY < 0.2f && targetEyeScaleY > 0.4f) {
      eyeLerpSpeed = 0.15f; // Blink open relatively fast
  }

  currentEyeScaleX += (targetEyeScaleX - currentEyeScaleX) * lerpSpeed;
  currentEyeScaleY += (targetEyeScaleY - currentEyeScaleY) * eyeLerpSpeed;
  currentEyeOffsetY += (targetEyeOffsetY - currentEyeOffsetY) * lerpSpeed;
  currentMouthScaleX += (targetMouthScaleX - currentMouthScaleX) * lerpSpeed;
  currentMouthScaleY += (targetMouthScaleY - currentMouthScaleY) * lerpSpeed;
  currentMouthOffsetY += (targetMouthOffsetY - currentMouthOffsetY) * lerpSpeed;
  currentPupilScale += (targetPupilScale - currentPupilScale) * lerpSpeed;

  // Idle animations (Base) with organic breathing
  float baseBobY = sin(now * 0.002f) * 4.0f;
  float baseIdleLookX = sin(now * 0.0007f) * cos(now * 0.0005f) * 8.0f;
  float baseIdleLookY = sin(now * 0.0009f) * cos(now * 0.0004f) * 5.0f;
  
  float breathingScale = 1.0f + (sin(now * 0.0015f) * 0.03f); // 3% breathing scale
'''

content = re.sub(r'// LERP for ultra-smooth morphing.*?float baseIdleLookY = sin\(now \* 0.0009f\) \* cos\(now \* 0.0004f\) \* 5.0f;', lerp_logic.strip(), content, flags=re.DOTALL)

# Inject breathingScale into baseScale
content = content.replace('float baseScale = 1.8f;', 'float baseScale = 1.8f * breathingScale;')

with open('src/gembot.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
print("Patch applied")
