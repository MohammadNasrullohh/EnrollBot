import re

with open('src/gembot.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Replace blink logic and lerp logic
old_anim_logic = '''  unsigned long blinkCycle = now % 4000;
  if (targetEyeScaleY > 0.5f) {
      if (blinkCycle < 120) {
          targetEyeScaleY = 0.1f;
      } else if (blinkCycle > 250 && blinkCycle < 350 && (now % 12000) < 4000) {
          targetEyeScaleY = 0.1f;
      }
  }

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
  
  float breathingScale = 1.0f + (sin(now * 0.0015f) * 0.03f); // 3% breathing scale'''

new_anim_logic = '''  // --- Alive Animation Logic ---
  unsigned long blinkCycle = now % 4500;
  
  // Blink Anticipation (Widen before blink)
  if (targetEyeScaleY > 0.5f) {
      if (blinkCycle > 4250 && blinkCycle <= 4350) {
          targetEyeScaleY *= 1.2f; // Anticipation widen
      } else if (blinkCycle > 4350 && blinkCycle <= 4500) {
          targetEyeScaleY = 0.05f; // Squashed closed
      }
  }

  // Physics Variables for Spring-Damper
  static float vEyeScaleX = 0, vEyeScaleY = 0;
  static float vMouthScaleX = 0, vMouthScaleY = 0;
  
  float spring = 0.15f;
  float damp = 0.70f;

  vEyeScaleX += (targetEyeScaleX - currentEyeScaleX) * spring;
  vEyeScaleY += (targetEyeScaleY - currentEyeScaleY) * spring;
  vMouthScaleX += (targetMouthScaleX - currentMouthScaleX) * spring;
  vMouthScaleY += (targetMouthScaleY - currentMouthScaleY) * spring;

  vEyeScaleX *= damp;
  vEyeScaleY *= damp;
  vMouthScaleX *= damp;
  vMouthScaleY *= damp;

  currentEyeScaleX += vEyeScaleX;
  currentEyeScaleY += vEyeScaleY;
  currentMouthScaleX += vMouthScaleX;
  currentMouthScaleY += vMouthScaleY;

  // Prevent negative scale
  if (currentEyeScaleX < 0.05f) currentEyeScaleX = 0.05f;
  if (currentEyeScaleY < 0.05f) currentEyeScaleY = 0.05f;
  if (currentMouthScaleX < 0.05f) currentMouthScaleX = 0.05f;
  if (currentMouthScaleY < 0.05f) currentMouthScaleY = 0.05f;

  // Linear LERP for position to avoid jitter
  float lerpSpeed = 0.15f;
  currentEyeOffsetY += (targetEyeOffsetY - currentEyeOffsetY) * lerpSpeed;
  currentMouthOffsetY += (targetMouthOffsetY - currentMouthOffsetY) * lerpSpeed;
  currentPupilScale += (targetPupilScale - currentPupilScale) * lerpSpeed;

  // Saccadic Eye Darts (Randomly changing target positions instead of sine waves)
  static float saccadeTargetX = 0;
  static float saccadeTargetY = 0;
  static unsigned long nextSaccadeMs = 0;
  if (now > nextSaccadeMs) {
      saccadeTargetX = (random(100) / 100.0f - 0.5f) * 12.0f; // Dart between -6 and +6
      saccadeTargetY = (random(100) / 100.0f - 0.5f) * 8.0f;
      nextSaccadeMs = now + random(500, 2500); // Wait 0.5s to 2.5s before darting again
  }
  static float currentSaccadeX = 0, currentSaccadeY = 0;
  currentSaccadeX += (saccadeTargetX - currentSaccadeX) * 0.3f; // Fast snap
  currentSaccadeY += (saccadeTargetY - currentSaccadeY) * 0.3f;

  float baseBobY = sin(now * 0.002f) * 4.0f; // Body still breathes smoothly
  float baseIdleLookX = currentSaccadeX;
  float baseIdleLookY = currentSaccadeY;
  
  float breathingScale = 1.0f + (sin(now * 0.0015f) * 0.03f); // 3% breathing scale'''

content = content.replace(old_anim_logic, new_anim_logic)

# Replace baseScale and verticalSpread
old_layout = '''  uint16_t faceColor = spr.color565(0, 200, 255); // Biru (Cyan-ish Blue)
  float baseScale = 1.8f * breathingScale;
  float verticalSpread = 1.4f; // Spread eyes and mouth vertically'''

new_layout = '''  uint16_t faceColor = spr.color565(0, 200, 255); // Biru (Cyan-ish Blue)
  float baseScale = 2.4f * breathingScale; // MUCH LARGER!
  float verticalSpread = 1.2f; // Adjusted for the larger scale'''

content = content.replace(old_layout, new_layout)

with open('src/gembot.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print("Alive physics applied.")
