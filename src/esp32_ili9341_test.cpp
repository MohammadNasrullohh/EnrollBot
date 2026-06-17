#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// ESP32 DOIT DevKit V1 -> ILI9341 SPI
constexpr int TFT_CS = 5;
constexpr int TFT_RST = 4;
constexpr int TFT_DC = 2;
constexpr int TFT_MOSI = 23;
constexpr int TFT_SCLK = 18;
constexpr int TFT_MISO = 19;

constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 320;
constexpr uint32_t FRAME_MS = 40;  // 25 FPS target.

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16* frame = nullptr;

enum FaceMood : uint8_t {
  MOOD_NEUTRAL,
  MOOD_HAPPY,
  MOOD_CURIOUS,
  MOOD_SHY,
  MOOD_SURPRISED,
  MOOD_GRUMPY,
  MOOD_COUNT
};

struct FacePose {
  float eyeW;
  float eyeH;
  float eyeY;
  float eyeGap;
  float pupilR;
  float browLift;
  float browTilt;
  float mouthW;
  float mouthCurve;
  float mouthOpen;
};

const FacePose POSES[MOOD_COUNT] = {
    // eyeW, eyeH, eyeY, gap, pupil, browY, browTilt, mouthW, curve, open
    {67, 102, 76, 22, 12, -13,  0, 44,  9,  0},  // Neutral
    {71,  82, 87, 18, 11, -14,  2, 68, 17,  0},  // Happy
    {65, 104, 72, 27, 13, -18, -6, 42,  7,  0},  // Curious
    {63,  94, 82, 25, 12, -13,  3, 42, 13,  0},  // Shy
    {76, 119, 67, 13,  9, -20,  0, 28,  0, 25},  // Surprised
    {67,  70, 94, 20, 10, -10, 12, 48, -8,  0},  // Grumpy
};

FacePose currentPose = POSES[MOOD_NEUTRAL];
FacePose targetPose = POSES[MOOD_NEUTRAL];
FaceMood mood = MOOD_NEUTRAL;

unsigned long nextMoodMs = 0;
unsigned long nextLookMs = 0;
unsigned long nextBlinkMs = 0;
unsigned long blinkStartedMs = 0;
bool blinking = false;

float targetLookX = 0;
float targetLookY = 0;
float smoothLookX = 0;
float smoothLookY = 0;
float moodBlend = 0;

float lerpFloat(float current, float target, float amount) {
  return current + (target - current) * amount;
}

void updatePose(float amount) {
  currentPose.eyeW = lerpFloat(currentPose.eyeW, targetPose.eyeW, amount);
  currentPose.eyeH = lerpFloat(currentPose.eyeH, targetPose.eyeH, amount);
  currentPose.eyeY = lerpFloat(currentPose.eyeY, targetPose.eyeY, amount);
  currentPose.eyeGap = lerpFloat(currentPose.eyeGap, targetPose.eyeGap, amount);
  currentPose.pupilR = lerpFloat(currentPose.pupilR, targetPose.pupilR, amount);
  currentPose.browLift = lerpFloat(currentPose.browLift, targetPose.browLift, amount);
  currentPose.browTilt = lerpFloat(currentPose.browTilt, targetPose.browTilt, amount);
  currentPose.mouthW = lerpFloat(currentPose.mouthW, targetPose.mouthW, amount);
  currentPose.mouthCurve = lerpFloat(currentPose.mouthCurve, targetPose.mouthCurve, amount);
  currentPose.mouthOpen = lerpFloat(currentPose.mouthOpen, targetPose.mouthOpen, amount);
}

void chooseNextMood(unsigned long now) {
  static const FaceMood sequence[] = {
      MOOD_HAPPY,
      MOOD_CURIOUS,
      MOOD_NEUTRAL,
      MOOD_SHY,
      MOOD_HAPPY,
      MOOD_SURPRISED,
      MOOD_NEUTRAL,
      MOOD_GRUMPY,
      MOOD_HAPPY,
  };
  static uint8_t index = 0;

  mood = sequence[index];
  index = (index + 1) % (sizeof(sequence) / sizeof(sequence[0]));
  targetPose = POSES[mood];
  moodBlend = 0;
  nextMoodMs = now + 4800UL + random(0, 2600);

  if (mood == MOOD_SHY) {
    targetLookX = -7;
    targetLookY = 6;
  } else if (mood == MOOD_CURIOUS) {
    targetLookX = 8;
    targetLookY = -5;
  } else if (mood == MOOD_GRUMPY) {
    targetLookX = 0;
    targetLookY = 3;
  } else {
    targetLookX = random(-8, 9);
    targetLookY = random(-5, 6);
  }
}

float blinkAmount(unsigned long now) {
  if (!blinking && now >= nextBlinkMs) {
    blinking = true;
    blinkStartedMs = now;
  }

  if (!blinking) return 0.0f;

  unsigned long elapsed = now - blinkStartedMs;
  constexpr unsigned long blinkDuration = 260UL;
  if (elapsed >= blinkDuration) {
    blinking = false;
    nextBlinkMs = now + 4400UL + random(0, 4200);
    return 0.0f;
  }

  float phase = static_cast<float>(elapsed) / blinkDuration;
  return sinf(phase * PI);
}

void drawThickLine(Adafruit_GFX& gfx, int x1, int y1, int x2, int y2,
                   uint16_t color, int thickness) {
  for (int offset = -thickness / 2; offset <= thickness / 2; ++offset) {
    gfx.drawLine(x1, y1 + offset, x2, y2 + offset, color);
  }
}

void drawEye(Adafruit_GFX& gfx, int x, int y, int w, int h,
             int pupilX, int pupilY, int pupilRadius, float lid) {
  int visibleH = max(5, static_cast<int>(h * (1.0f - lid * 0.94f)));
  int visibleY = y + (h - visibleH) / 2;
  int radius = max(4, min(w, visibleH) / 3);

  gfx.fillRoundRect(x, visibleY, w, visibleH, radius, ILI9341_WHITE);
  if (visibleH <= 17) return;

  int maxPupilX = max(0, w / 2 - pupilRadius - 5);
  int maxPupilY = max(0, visibleH / 2 - pupilRadius - 5);
  int px = x + w / 2 + constrain(pupilX, -maxPupilX, maxPupilX);
  int py = visibleY + visibleH / 2 + constrain(pupilY, -maxPupilY, maxPupilY);

  gfx.fillCircle(px, py, pupilRadius, ILI9341_BLACK);
  gfx.fillCircle(px - pupilRadius / 3, py - pupilRadius / 3,
                 max(2, pupilRadius / 4), ILI9341_WHITE);
}

void drawMouth(Adafruit_GFX& gfx, int centerX, int y, int width,
               int curve, int openAmount) {
  if (openAmount > 3) {
    int h = max(8, openAmount);
    gfx.fillRoundRect(centerX - width / 2, y - h / 2, width, h, h / 2,
                      ILI9341_WHITE);
    gfx.fillRoundRect(centerX - width / 2 + 5, y - h / 2 + 5,
                      width - 10, max(3, h - 10), max(2, h / 3),
                      ILI9341_BLACK);
    return;
  }

  int half = max(4, width / 2);
  for (int x = -half; x <= half; ++x) {
    float n = static_cast<float>(x) / half;
    int yy = y + static_cast<int>((1.0f - n * n) * curve);
    gfx.fillCircle(centerX + x, yy, 2, ILI9341_WHITE);
  }
}

void drawFaceToBuffer(unsigned long now) {
  frame->fillScreen(ILI9341_BLACK);

  float breathe = sinf(now * 0.0025f);
  float sway = sinf(now * 0.00135f);
  float micro = sinf(now * 0.0065f);
  int bobY = static_cast<int>(roundf(breathe * 4.0f));
  int swayX = static_cast<int>(roundf(sway * 3.0f));

  if (now >= nextMoodMs) chooseNextMood(now);
  if (now >= nextLookMs) {
    nextLookMs = now + 1700UL + random(0, 2300);
    if (mood != MOOD_SHY && mood != MOOD_CURIOUS && mood != MOOD_GRUMPY) {
      targetLookX = random(-9, 10);
      targetLookY = random(-6, 7);
    }
  }

  moodBlend = min(1.0f, moodBlend + 0.035f);
  float morphSpeed = 0.055f + sinf(moodBlend * PI) * 0.035f;
  updatePose(morphSpeed);
  smoothLookX = lerpFloat(smoothLookX, targetLookX, 0.075f);
  smoothLookY = lerpFloat(smoothLookY, targetLookY, 0.075f);

  float lid = blinkAmount(now);
  if (mood == MOOD_HAPPY) lid = max(lid, 0.15f);
  if (mood == MOOD_GRUMPY) lid = max(lid, 0.24f);

  int eyeW = static_cast<int>(currentPose.eyeW);
  int eyeH = static_cast<int>(currentPose.eyeH);
  int eyeY = static_cast<int>(currentPose.eyeY) + bobY;
  int gap = static_cast<int>(currentPose.eyeGap);
  int leftX = SCREEN_W / 2 - gap / 2 - eyeW + swayX;
  int rightX = SCREEN_W / 2 + gap / 2 + swayX;

  int lookX = static_cast<int>(smoothLookX + micro * 0.8f);
  int lookY = static_cast<int>(smoothLookY);
  int pupilR = static_cast<int>(currentPose.pupilR);

  drawEye(*frame, leftX, eyeY, eyeW, eyeH, lookX, lookY, pupilR, lid);
  drawEye(*frame, rightX, eyeY, eyeW, eyeH, lookX, lookY, pupilR, lid);

  // Brows carry emotion without changing the unified eye style.
  int browY = eyeY + static_cast<int>(currentPose.browLift);
  int browTilt = static_cast<int>(currentPose.browTilt);
  if (mood != MOOD_NEUTRAL && mood != MOOD_HAPPY) {
    drawThickLine(*frame, leftX + 5, browY - browTilt,
                  leftX + eyeW - 5, browY + browTilt, ILI9341_WHITE, 4);
    drawThickLine(*frame, rightX + 5, browY + browTilt,
                  rightX + eyeW - 5, browY - browTilt, ILI9341_WHITE, 4);
  }

  int mouthY = 231 + bobY;
  drawMouth(*frame, SCREEN_W / 2 + swayX, mouthY,
            static_cast<int>(currentPose.mouthW),
            static_cast<int>(currentPose.mouthCurve),
            static_cast<int>(currentPose.mouthOpen));

  // Subtle details are expression-specific and stay inside the face.
  if (mood == MOOD_SHY) {
    for (int i = 0; i < 3; ++i) {
      frame->drawLine(31 + i * 8, 211 + bobY, 36 + i * 8, 211 + bobY,
                      ILI9341_PINK);
      frame->drawLine(184 + i * 8, 211 + bobY, 189 + i * 8, 211 + bobY,
                      ILI9341_PINK);
    }
  } else if (mood == MOOD_SURPRISED) {
    frame->drawCircle(120 + swayX, mouthY, 19, ILI9341_WHITE);
  } else if (mood == MOOD_GRUMPY) {
    frame->drawLine(206, 82 + bobY, 220, 96 + bobY, ILI9341_RED);
    frame->drawLine(220, 82 + bobY, 206, 96 + bobY, ILI9341_RED);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  pinMode(TFT_DC, OUTPUT);
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, HIGH);
  delay(30);
  digitalWrite(TFT_RST, LOW);
  delay(100);
  digitalWrite(TFT_RST, HIGH);
  delay(160);

  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
  tft.begin(27000000);
  tft.setRotation(0);
  tft.setTextWrap(false);
  tft.fillScreen(ILI9341_BLACK);

  frame = new GFXcanvas16(SCREEN_W, SCREEN_H);
  if (frame == nullptr || frame->getBuffer() == nullptr) {
    Serial.println("Framebuffer allocation failed");
    while (true) delay(1000);
  }

  randomSeed(esp_random());
  unsigned long now = millis();
  nextMoodMs = now + 4200UL;
  nextLookMs = now + 1500UL;
  nextBlinkMs = now + 3500UL;

  Serial.println("OWI full-screen face ready");
  Serial.println("Framebuffer anti-flicker, SPI 27 MHz");

  drawFaceToBuffer(now);
  tft.drawRGBBitmap(0, 0, frame->getBuffer(), SCREEN_W, SCREEN_H);
}

void loop() {
  static unsigned long lastFrameMs = 0;
  unsigned long now = millis();
  if (now - lastFrameMs < FRAME_MS) return;
  lastFrameMs = now;

  drawFaceToBuffer(now);
  tft.drawRGBBitmap(0, 0, frame->getBuffer(), SCREEN_W, SCREEN_H);
}
