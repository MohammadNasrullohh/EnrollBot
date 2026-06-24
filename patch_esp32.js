const fs = require('fs');
let code = fs.readFileSync('src/main_tft_only.cpp', 'utf8');

const faceStructCode = `
WiFiUDP cmdUdp;

struct FaceParam {
  float leftEyeW, leftEyeH, leftEyeX, leftEyeY;
  float rightEyeW, rightEyeH, rightEyeX, rightEyeY;
  float mouthW, mouthH, mouthX, mouthY;
  float mouthCurve; // 1 = smile, -1 = sad, 0 = flat
};

FaceParam faceNormal = { 26, 37, 20, 13,  26, 37, 84, 13,  28, 8, 50, 46,  0 };
FaceParam faceHappy  = { 26, 20, 20, 20,  26, 20, 84, 20,  36, 18, 46, 42,  1 };
FaceParam faceSad    = { 26, 26, 20, 16,  26, 26, 84, 16,  26, 12, 51, 46, -1 };
FaceParam faceAngry  = { 26, 16, 20, 24,  26, 16, 84, 24,  20, 6, 54, 46,  -1 };
FaceParam faceDizzy  = { 16, 16, 26, 24,  32, 32, 80, 16,  16, 20, 56, 42,  0 };
FaceParam faceCheeky = { 26, 37, 20, 13,  26, 12, 84, 26,  26, 10, 51, 42,  1 };

FaceParam currentFace = faceNormal;
FaceParam targetFace = faceNormal;

void updateMorph() {
  float speed = 0.2f;
  currentFace.leftEyeW += (targetFace.leftEyeW - currentFace.leftEyeW) * speed;
  currentFace.leftEyeH += (targetFace.leftEyeH - currentFace.leftEyeH) * speed;
  currentFace.leftEyeX += (targetFace.leftEyeX - currentFace.leftEyeX) * speed;
  currentFace.leftEyeY += (targetFace.leftEyeY - currentFace.leftEyeY) * speed;
  currentFace.rightEyeW += (targetFace.rightEyeW - currentFace.rightEyeW) * speed;
  currentFace.rightEyeH += (targetFace.rightEyeH - currentFace.rightEyeH) * speed;
  currentFace.rightEyeX += (targetFace.rightEyeX - currentFace.rightEyeX) * speed;
  currentFace.rightEyeY += (targetFace.rightEyeY - currentFace.rightEyeY) * speed;
  currentFace.mouthW += (targetFace.mouthW - currentFace.mouthW) * speed;
  currentFace.mouthH += (targetFace.mouthH - currentFace.mouthH) * speed;
  currentFace.mouthX += (targetFace.mouthX - currentFace.mouthX) * speed;
  currentFace.mouthY += (targetFace.mouthY - currentFace.mouthY) * speed;
  currentFace.mouthCurve += (targetFace.mouthCurve - currentFace.mouthCurve) * speed;
}
`;

// Insert the face struct before pushScaledSpriteCustom
code = code.replace('void pushScaledSpriteCustom() {', faceStructCode + '\nvoid pushScaledSpriteCustom() {');

// Replace drawMochi
const newDrawMochi = `
void drawMochi() {
  spr.fillSprite(0);
  
  int mouthOffset = 0;
  if (currentRmsVolume > 0.05f) {
    mouthOffset = (int)(currentRmsVolume * 25.0f);
  }
  
  // Eyes
  spr.fillRoundRect(currentFace.leftEyeX + eyeOffsetX, currentFace.leftEyeY + eyeOffsetY, currentFace.leftEyeW, currentFace.leftEyeH, 8, 1);
  spr.fillRoundRect(currentFace.rightEyeX + eyeOffsetX, currentFace.rightEyeY + eyeOffsetY, currentFace.rightEyeW, currentFace.rightEyeH, 8, 1);

  // Mouth
  int mx = currentFace.mouthX;
  int my = currentFace.mouthY + mouthOffset;
  int mw = currentFace.mouthW;
  int mh = currentFace.mouthH;

  if (currentFace.mouthCurve > 0.5f) {
    spr.fillCircle(mx + mw/2, my, mw/2, 1);
    spr.fillRect(mx, my - mw/2, mw, mw/2, 0);
  } else if (currentFace.mouthCurve < -0.5f) {
    spr.fillCircle(mx + mw/2, my + mw/2, mw/2, 1);
    spr.fillRect(mx, my + mw/2, mw, mw/2, 0);
  } else {
    spr.fillRoundRect(mx, my, mw, mh, 4, 1);
  }

  // Blinking animation
  unsigned long now = millis();
  static unsigned long nextBlink = 2000;
  static bool blinking = false;
  static unsigned long blinkStart = 0;
  
  if (!blinking && now > nextBlink) {
    blinking = true;
    blinkStart = now;
    nextBlink = now + random(2000, 5000);
    playToneAsync(1200.0f, 50);
  }
  
  if (blinking) {
    long elapsed = now - blinkStart;
    int blinkHeight = 0;
    if (elapsed < 80) {
      blinkHeight = (elapsed * 37) / 80;
    } else if (elapsed < 160) {
      blinkHeight = 37 - ((elapsed - 80) * 37) / 80;
    } else {
      blinking = false;
    }
    if (blinkHeight > 0) {
      spr.fillRect(10, 0, 108, blinkHeight, 0); // top cover
      spr.fillRect(10, 50 - blinkHeight, 108, blinkHeight + 14, 0); // bottom cover
    }
  }

  static unsigned long nextGlance = 1000;
  if (now > nextGlance && !blinking) {
    if (random(0,3) == 0) {
      eyeOffsetX = 0;
      eyeOffsetY = 0;
    } else {
      eyeOffsetX = random(-3, 4);
      eyeOffsetY = random(-2, 3);
      playToneAsync(600.0f, 80);
      playToneAsync(800.0f, 100);
    }
    nextGlance = now + random(500, 2000);
  }

  pushScaledSpriteCustom();
}
`;

const oldDrawMochiRegex = /void drawMochi\(\) \{[\s\S]*?pushScaledSpriteCustom\(\);\n\}/;
code = code.replace(oldDrawMochiRegex, newDrawMochi.trim());

// Add cmdUdp.begin
code = code.replace('audioServer.begin();', 'audioServer.begin();\n  cmdUdp.begin(7789);');

// Add parsing to loop
const loopUdp = `
  int packetSize = cmdUdp.parsePacket();
  if (packetSize) {
    char buf[64];
    int len = cmdUdp.read(buf, sizeof(buf)-1);
    if (len > 0) {
      buf[len] = 0;
      String cmdStr = String(buf);
      if (cmdStr.startsWith("CMD:M")) {
        String m = cmdStr.substring(4);
        if (m == "M0") targetFace = faceNormal;
        else if (m == "M1") targetFace = faceHappy;
        else if (m == "M6") targetFace = faceSad;
        else if (m == "M3") targetFace = faceAngry;
        else if (m == "M30") targetFace = faceDizzy;
        else if (m == "M31") targetFace = faceCheeky;
      }
    }
  }
  updateMorph();
`;
code = code.replace('drawMochi();', loopUdp + '\n  drawMochi();');

fs.writeFileSync('src/main_tft_only.cpp', code);
console.log('Patched main_tft_only.cpp');
