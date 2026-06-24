import re

with open('src/gembot.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# We'll replace the existing Tilt/Look calculations section with our advanced one
advanced_mpu = '''
  // --- Organic Reactions ---
  static unsigned long continuousShakeStart = 0;
  static unsigned long continuousSpinStart = 0;
  static unsigned long restoreFaceMs = 0;
  static int previousExpressionId = 0;

  // Restore logic
  if (restoreFaceMs > 0 && now > restoreFaceMs) {
      if (currentState == APP_FACE) currentExpressionId = previousExpressionId;
      restoreFaceMs = 0;
  }

  // Spin detection (Pusing)
  if (fabs(gx) > 5.0f || fabs(gy) > 5.0f || fabs(gz) > 5.0f) {
      if (continuousSpinStart == 0) {
          continuousSpinStart = now;
          if (restoreFaceMs == 0 && currentState == APP_FACE) previousExpressionId = currentExpressionId;
      }
      else if (now - continuousSpinStart > 800) { // Spun for > 0.8s
          if (currentState == APP_FACE) {
              currentExpressionId = 11; // Pusing (Woozy)
              restoreFaceMs = now + 4000;
          }
      }
  } else {
      continuousSpinStart = 0;
  }

  // Shake detection (Marah -> Menangis)
  if (jerk > 20.0f) { // ~2G threshold
      if (continuousShakeStart == 0) {
          continuousShakeStart = now;
          if (restoreFaceMs == 0 && currentState == APP_FACE) previousExpressionId = currentExpressionId;
      }
      else {
          unsigned long shakeDuration = now - continuousShakeStart;
          if (currentState == APP_FACE) {
              if (shakeDuration > 2500) {
                  currentExpressionId = 29; // Nangis
                  restoreFaceMs = now + 5000;
              } else if (shakeDuration > 800) {
                  currentExpressionId = 3; // Marah
                  restoreFaceMs = now + 4000;
              }
          }
      }
  } else {
      continuousShakeStart = 0;
  }

  // Tilt/Look calculations
'''

# Find the spot to inject
content = content.replace('  // Tilt/Look calculations', advanced_mpu.strip())

with open('src/gembot.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
print("Patch applied")
