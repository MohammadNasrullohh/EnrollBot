#include <Arduino.h>
#include <DFRobotDFPlayerMini.h>
#include "dasai_bitmaps.h"
#include "lopaka_screens.h"
#include "game_images.h"
#include <Wire.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include "driver/i2s.h"
#include "secrets.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C

#define OLED_WHITE SH110X_WHITE
#define OLED_BLACK SH110X_BLACK

#define MAX_BCLK_PIN D0
#define MAX_LRC_PIN D8
#define MAX_DIN_PIN D7
#define MIC_SD_PIN D1
#define DHT_PIN D2
#define DHT_TYPE DHT22
#define TOUCH_PIN D3

const uint16_t AUDIO_PORT = 7777;
const uint16_t TELEMETRY_PORT = 7788;
const uint32_t AUDIO_RATE = 16000;
const uint16_t AUDIO_FRAMES = 128;
const uint16_t AUDIO_BLOCK_BYTES = AUDIO_FRAMES * 2;
const uint16_t AUDIO_RING_SIZE = 24576;
const uint16_t AUDIO_PREBUFFER_BYTES = 6144;
const uint16_t READ_CHUNK = 1024;

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_MPU6050 mpu;
DHT dht(DHT_PIN, DHT_TYPE);
WiFiServer audioServer(AUDIO_PORT);
WiFiClient audioClient;
WiFiUDP telemetryUdp;
WiFiUDP micUdp;

bool oledReady = false;
bool mpuReady = false;
bool rawMpuMode = false;
uint8_t mpuAddr = 0x68;
uint8_t mpuWho = 0xFF;
bool dhtReady = false;
bool playing = false;
bool wasPlaying = false;
unsigned long lastAudioMs = 0;
unsigned long lastDrawMs = 0;
unsigned long lastMpuMs = 0;
unsigned long lastDhtMs = 0;
unsigned long lastTelemetryMs = 0;
uint32_t totalBytes = 0;
uint32_t playedBytes = 0;
float levelSmooth = 0.0f;
int16_t lastSample = 0;
// INMP441 Mic voice reactivity
volatile float micSmooth = 0.0f;    // 0.0 - 1.0 ambient mic loudness
volatile float micPeak  = 0.0f;    // short-term peak
unsigned long micActiveUntilMs = 0; // timestamp while voice is detected
unsigned long micShoutUntilMs  = 0; // timestamp when shout/loud detected
float faceLookX = 0.0f;
float faceLookY = 0.0f;
float faceLean = 0.0f;
float faceBob = 0.0f;
float faceDanceX = 0.0f;
float eyeSmoothH = 37.0f;
float cueSmooth = 0.0f;
float tiltX = 0.0f;
float tiltY = 0.0f;
float shakeSmooth = 0.0f;
float lastAx = 0.0f;
float lastAy = 0.0f;
float lastAz = 9.8f;
float rawAx = 0.0f;
float rawAy = 0.0f;
float rawAz = 9.8f;
float rawGx = 0.0f;
float rawGy = 0.0f;
float rawGz = 0.0f;
float tempC = NAN;
float humPct = NAN;
float tempMood = 0.0f;
float humidMood = 0.0f;
float shakeMeter = 0.0f;
bool angryMode = false;
bool superAngryMode = false;       // triggered after sustained hard shaking
unsigned long angryUntilMs = 0;
unsigned long superAngryUntilMs = 0;
unsigned long dizzyUntilMs = 0;
unsigned long sadUntilMs = 0;
unsigned long annoyedUntilMs = 0;
unsigned long shakeActiveStartMs = 0; // when continuous shaking started
bool touchDown = false;
bool lastTouchReading = false;
unsigned long touchChangedMs = 0;
unsigned long touchDownMs = 0;
unsigned long lastTapMs = 0;
unsigned long touchHappyUntilMs = 0;
unsigned long touchLoveUntilMs = 0;
unsigned long touchSleepyUntilMs = 0;
bool nodDetected = false;
unsigned long nodUntilMs = 0;
bool headShakeDetected = false;
unsigned long headShakeUntilMs = 0;
bool surprisedMode = false;
unsigned long surprisedUntilMs = 0;
bool curiousMode = false;
unsigned long curiousUntilMs = 0;
bool faceDownMode = false;
bool freefallMode = false;
unsigned long freefallUntilMs = 0;
unsigned long laughUntilMs = 0;
unsigned long glitchUntilMs = 0;
unsigned long pantUntilMs = 0;
unsigned long cryUntilMs = 0;
unsigned long lastActivityMs = 0;
bool isSleepingWithBubble = false;
int manualJoyX = 0;
int manualJoyY = 0;
unsigned long lastJoyMs = 0;
int manualMood = -1;
unsigned long manualMoodMs = 0;
float pitchHistory[4] = {0};
float yawHistory[4] = {0};
uint8_t histIdx = 0;
unsigned long lastHistMs = 0;
float sustainedTiltX = 0.0f;
unsigned long sustainedTiltStartMs = 0;
// Natural blink system
unsigned long nextBlinkMs = 0;
unsigned long blinkStartMs = 0;
bool isBlinking = false;
float blinkProgress = 0.0f;
bool winkLeft = false;
unsigned long nextWinkMs = 0;
unsigned long winkStartMs = 0;
bool isWinking = false;
float winkProgress = 0.0f;
// Idle glance & expression system
float idleGlanceTargetX = 0.0f;
float idleGlanceTargetY = 0.0f;
unsigned long nextGlanceMs = 0;
int idleExpr = 0;          // 0=normal,1=curious,2=shy,3=bored,4=excited,5=squint,6=thinking,7=smug
unsigned long nextIdleExprMs = 0;
// Breathing idle animation
float breathPhase = 0.0f;

// Pingpong Game state
enum GamePhase { GAME_IDLE, GAME_PLAYING, GAME_WAIT, GAME_OVER };
GamePhase gamePhase = GAME_IDLE;
unsigned long gameWaitStartMs = 0;
float ballX = 64.0f, ballY = 32.0f;
float ballVX = 3.0f, ballVY = 2.0f;
float paddlePlayerY = 32.0f, paddleAiY = 32.0f;
int scorePlayer = 0, scoreAi = 0;
unsigned long lastGameTickMs = 0;
unsigned long touchGameMs = 0;

void initPingpong(bool resetScore = false); // Forward declaration for updateTouch

struct FaceState {
  float eyeW, eyeH, eyeR;
  float eyeSpacing;
  float browDrop, browAngle;
  float mouthW, mouthH, mouthY, mouthCurve;
  float cheekRise;
};

FaceState currentFace = {26.0f, 37.0f, 10.0f, 48.0f, 0.0f, 0.0f, 16.0f, 6.0f, 35.0f, 4.0f, 0.0f};
FaceState targetFace = {26.0f, 37.0f, 10.0f, 48.0f, 0.0f, 0.0f, 16.0f, 6.0f, 35.0f, 4.0f, 0.0f};


enum AppState { STATE_FACE, STATE_MENU, STATE_GAMES, STATE_SENSOR, STATE_REMINDER };
AppState currentState = STATE_FACE;
bool req_lovestory = false;
int menuCursor = 0;
int gameCursor = 0;
char globalReminderText[64] = "Belum ada reminder";

uint8_t audioRing[AUDIO_RING_SIZE];
uint16_t ringHead = 0;
uint16_t ringTail = 0;
uint16_t ringCount = 0;
uint8_t readBuffer[READ_CHUNK];
int16_t stereoSamples[AUDIO_FRAMES * 2];

enum CueMood : uint8_t {
  CUE_IDLE,
  CUE_DREAMY,
  CUE_SHY,
  CUE_WORRIED,
  CUE_PLEADING,
  CUE_RUN,
  CUE_PRINCE,
  CUE_HAPPY,
  CUE_LONELY,
  CUE_RING,
  CUE_FINALE
};

struct LyricCue {
  uint16_t start10;
  uint16_t end10;
  CueMood mood;
};

// Approximate Love Story phrase map, stored as timing cues instead of lyrics.
const LyricCue LOVE_STORY_CUES[] = {
  {  0, 120, CUE_DREAMY}, {120, 220, CUE_DREAMY}, {220, 320, CUE_SHY},
  {320, 430, CUE_HAPPY},  {430, 560, CUE_WORRIED}, {560, 690, CUE_PLEADING},
  {690, 820, CUE_RUN},    {820, 960, CUE_PRINCE},  {960,1080, CUE_HAPPY},
  {1080,1210, CUE_SHY},   {1210,1350, CUE_WORRIED}, {1350,1500, CUE_PLEADING},
  {1500,1640, CUE_RUN},   {1640,1800, CUE_PRINCE}, {1800,1940, CUE_HAPPY},
  {1940,2080, CUE_WORRIED}, {2080,2220, CUE_RUN},  {2220,2370, CUE_PRINCE},
  {2370,2510, CUE_HAPPY}, {2510,2660, CUE_DREAMY}, {2660,2810, CUE_LONELY},
  {2810,2960, CUE_WORRIED}, {2960,3090, CUE_PLEADING}, {3090,3210, CUE_RING},
  {3210,3370, CUE_HAPPY}, {3370,3530, CUE_PRINCE}, {3530,3670, CUE_FINALE},
  {3670,3820, CUE_FINALE}, {3820,3960, CUE_DREAMY}
};
const uint8_t LOVE_STORY_CUE_COUNT = sizeof(LOVE_STORY_CUES) / sizeof(LOVE_STORY_CUES[0]);

CueMood cueMoodAt(float sec) {
  uint16_t t = (uint16_t)(sec * 10.0f);
  for (uint8_t i = 0; i < LOVE_STORY_CUE_COUNT; i++) {
    if (t >= LOVE_STORY_CUES[i].start10 && t < LOVE_STORY_CUES[i].end10) {
      return LOVE_STORY_CUES[i].mood;
    }
  }
  return CUE_IDLE;
}

float clampFloat(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

uint8_t readReg8(uint8_t addr, uint8_t reg, bool& ok) {
  ok = false;
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return 0xFF;
  if (Wire.requestFrom((int)addr, 1) != 1) return 0xFF;
  ok = true;
  return Wire.read();
}

void writeReg8(uint8_t addr, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

int16_t be16(uint8_t hi, uint8_t lo) {
  return (int16_t)(((uint16_t)hi << 8) | lo);
}

bool readRawMotion(uint8_t addr, int16_t& ax, int16_t& ay, int16_t& az,
                   int16_t& gx, int16_t& gy, int16_t& gz) {
  Wire.beginTransmission(addr);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)addr, 14) != 14) return false;
  uint8_t b[14];
  for (uint8_t i = 0; i < 14; i++) b[i] = Wire.read();
  ax = be16(b[0], b[1]);
  ay = be16(b[2], b[3]);
  az = be16(b[4], b[5]);
  gx = be16(b[8], b[9]);
  gy = be16(b[10], b[11]);
  gz = be16(b[12], b[13]);
  return true;
}

bool setupRawMPU() {
  for (uint8_t addr : { (uint8_t)0x68, (uint8_t)0x69 }) {
    bool ok = false;
    uint8_t who = readReg8(addr, 0x75, ok);
    if (ok && (who == 0x68 || who == 0x70 || who == 0x71)) {
      mpuAddr = addr;
      mpuWho = who;
      rawMpuMode = true;
      writeReg8(addr, 0x6B, 0x00);
      delay(50);
      writeReg8(addr, 0x1A, 0x03);
      writeReg8(addr, 0x1B, 0x08);
      writeReg8(addr, 0x1C, 0x08);
      return true;
    }
  }
  return false;
}

void setupMPU() {
  rawMpuMode = false;
  if (mpu.begin(0x68, &Wire)) {
    mpuReady = true;
    mpuAddr = 0x68;
    mpuWho = 0x68;
    mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    Serial.println("MPU6050 ready at 0x68");
    return;
  }

  mpuReady = setupRawMPU();
  if (mpuReady) {
    Serial.print("Raw MPU ready addr=0x");
    Serial.print(mpuAddr, HEX);
    Serial.print(" who=0x");
    Serial.println(mpuWho, HEX);
  } else {
    Serial.println("MPU not found at 0x68/0x69");
  }
}

void setupDHT() {
  dht.begin();
  dhtReady = true;
  Serial.println("DHT22 ready on D2");
}

// Forward declarations needed for updateTouch()
void initMonsterBattle();
void playerAttack();

void updateTouch() {
  unsigned long now = millis();
  bool reading = digitalRead(TOUCH_PIN) == HIGH;
  if (reading != lastTouchReading) {
    lastTouchReading = reading;
    touchChangedMs = now;
  }
  if (now - touchChangedMs < 100UL) return;
  if (reading == touchDown) return;

  touchDown = reading;
  if (touchDown) {
    touchDownMs = now;
    return;
  }

  unsigned long held = now - touchDownMs;

  if (currentState == STATE_FACE) {
    if (held > 2000UL) {
      touchSleepyUntilMs = now + 2600UL;
      touchHappyUntilMs = 0;
      touchLoveUntilMs = 0;
      Serial.println("touch hold -> sleepy");
    } else if (now - lastTapMs < 430UL) {
      laughUntilMs = now + 2500UL; // Double tap -> Laugh
      touchLoveUntilMs = 0;
      touchHappyUntilMs = 0;
      lastTapMs = 0;
      Serial.println("touch double -> laugh");
    } else {
      lastTapMs = now;
      if (held < 500UL) {
        currentState = STATE_MENU;
        menuCursor = 0;
      }
    }
  } else if (currentState == STATE_MENU) {
    if (held > 600UL) {
      if (menuCursor == 0) currentState = STATE_GAMES;
      else if (menuCursor == 1) currentState = STATE_SENSOR;
      else if (menuCursor == 2) currentState = STATE_REMINDER;
      else if (menuCursor == 3) {
        currentState = STATE_FACE;
        req_lovestory = true;
      }
      else if (menuCursor == 4) currentState = STATE_FACE;
    } else {
      menuCursor = (menuCursor + 1) % 5;
    }
  } else if (currentState == STATE_GAMES) {
    if (held > 600UL) {
      // Long hold = back to FACE as requested
      currentState = STATE_FACE;
      gamePhase = GAME_IDLE;
    } else {
      // Short tap = start game or hit
      if (gamePhase == GAME_IDLE || gamePhase == GAME_OVER) {
        initPingpong(true);
      } else if (gamePhase == GAME_PLAYING) {
        // Option: short tap to give the ball a random spin if it's on player's side
        if (ballX < 64.0f) {
           ballVY += (float)random(-10, 11) * 0.1f;
        }
      }
    }
  } else if (currentState == STATE_SENSOR || currentState == STATE_REMINDER) {
    if (held > 600UL) {
      currentState = STATE_FACE;
    } else {
      currentState = STATE_MENU;
    }
  }
}

void updateMPU() {
  if (!mpuReady) return;
  unsigned long now = millis();
  if (now - lastMpuMs < 20UL) return;
  lastMpuMs = now;

  float ax;
  float ay;
  float az;
  float gx;
  float gy;
  float gz;

  static bool mpuWasError = false;

  if (rawMpuMode) {
    int16_t rax, ray, raz, rgx, rgy, rgz;
    if (!readRawMotion(mpuAddr, rax, ray, raz, rgx, rgy, rgz)) {
      mpuWasError = true;
      setupRawMPU();
      return;
    }
    ax = ((float)rax / 8192.0f) * 9.80665f;
    ay = ((float)ray / 8192.0f) * 9.80665f;
    az = ((float)raz / 8192.0f) * 9.80665f;
    gx = ((float)rgx / 65.5f) * 0.0174533f;
    gy = ((float)rgy / 65.5f) * 0.0174533f;
    gz = ((float)rgz / 65.5f) * 0.0174533f;
  } else {
    sensors_event_t a;
    sensors_event_t g;
    sensors_event_t temp;
    mpu.getEvent(&a, &g, &temp);
    ax = a.acceleration.x;
    ay = a.acceleration.y;
    az = a.acceleration.z;
    gx = g.gyro.x;
    gy = g.gyro.y;
    gz = g.gyro.z;
  }

  rawAx = ax; rawAy = ay; rawAz = az;
  rawGx = gx; rawGy = gy; rawGz = gz;

  // Sanity check: if I2C glitched, ax/ay/az will be 0 or garbage
  // A real sensor at rest has gravity ~9.8 m/s². Skip if total accel is impossible.
  float totalAccel = fabsf(ax) + fabsf(ay) + fabsf(az);
  if (totalAccel < 1.0f || totalAccel > 60.0f) {
      lastAx = ax; lastAy = ay; lastAz = az;
      return; // bad read, skip
  }

  if (mpuWasError) {
      lastAx = ax; lastAy = ay; lastAz = az;
      mpuWasError = false;
      return;
  }

  float targetTiltX = clampFloat(ax / 9.8f, -1.0f, 1.0f);
  float targetTiltY = clampFloat(ay / 9.8f, -1.0f, 1.0f);

  float jerk = fabsf(ax - lastAx) + fabsf(ay - lastAy) + fabsf(az - lastAz);

  // Large deadzone — ignore all small movements and I2C noise
  // Only real physical shaking (>3.0 delta) counts
  if (jerk < 3.0f) jerk = 0.0f;

  // Only use jerk for shake — gyro causes false positives from I2C noise
  float targetShake = clampFloat(jerk * 0.10f, 0.0f, 1.0f);

  tiltX += (targetTiltX - tiltX) * 0.18f;
  tiltY += (targetTiltY - tiltY) * 0.18f;
  shakeSmooth = shakeSmooth * 0.80f + targetShake * 0.20f;

  if (targetShake > 0.0f) {
    shakeMeter += targetShake * 0.05f;
  } else {
    shakeMeter -= 0.04f;  // Fast decay — returns to calm quickly
  }
  shakeMeter = clampFloat(shakeMeter, 0.0f, 1.0f);

  // Very high thresholds — only extreme continuous shaking triggers angry
  if (shakeMeter > 0.95f) {
    dizzyUntilMs = now + 3000UL;
    angryUntilMs = now + 3000UL;
    if (shakeActiveStartMs == 0) shakeActiveStartMs = now;
    // Super angry after 2+ seconds of heavy shaking
    if (now - shakeActiveStartMs > 2000UL) {
      superAngryUntilMs = now + 4000UL;
      cryUntilMs = now + 6000UL; // Crying after very heavy shaking
    }
  } else if (shakeMeter > 0.80f) {
    angryUntilMs = now + 2000UL;
    if (shakeActiveStartMs == 0) shakeActiveStartMs = now;
  } else if (shakeMeter > 0.60f) {
    dizzyUntilMs = now + 1500UL;
    shakeActiveStartMs = 0; // reset sustained tracker if calmer
  } else if (shakeMeter > 0.40f) {
    annoyedUntilMs = now + 600UL;
    shakeActiveStartMs = 0;
  } else {
    shakeActiveStartMs = 0; // fully calm, reset
  }
  
  if (faceDownMode) {
    if (now - sustainedTiltStartMs > 1500UL) glitchUntilMs = now + 1500UL; // Glitch when completely face down
    if (now - sustainedTiltStartMs > 3000UL) sadUntilMs = now + 2000UL;
  }

  angryMode = now < angryUntilMs;
  superAngryMode = now < superAngryUntilMs;

  if (now - lastHistMs > 100UL) {
    pitchHistory[histIdx] = ay;
    yawHistory[histIdx] = gz;
    histIdx = (histIdx + 1) % 4;
    lastHistMs = now;
  }
  float pMax = pitchHistory[0], pMin = pitchHistory[0];
  float yMax = yawHistory[0], yMin = yawHistory[0];
  for (int i = 1; i < 4; i++) {
    if (pitchHistory[i] > pMax) pMax = pitchHistory[i];
    if (pitchHistory[i] < pMin) pMin = pitchHistory[i];
    if (yawHistory[i] > yMax) yMax = yawHistory[i];
    if (yawHistory[i] < yMin) yMin = yawHistory[i];
  }
  if (pMax - pMin > 5.0f && !angryMode) {
    nodDetected = true;
    nodUntilMs = now + 1200UL;
  }
  if (yMax - yMin > 4.0f && !angryMode) {
    headShakeDetected = true;
    headShakeUntilMs = now + 1000UL;
  }
  if (jerk > 8.0f) {
    surprisedMode = true;
    surprisedUntilMs = now + 900UL;
  }
  if (fabsf(targetTiltX) > 0.45f) {
    if (sustainedTiltStartMs == 0) sustainedTiltStartMs = now;
    else if (now - sustainedTiltStartMs > 700UL) {
      curiousMode = true;
      curiousUntilMs = now + 500UL;
    }
  } else {
    sustainedTiltStartMs = 0;
  }
  faceDownMode = (az > 14.0f);
  
  float gForce = sqrt(ax*ax + ay*ay + az*az);
  if (gForce < 2.5f) {
      freefallMode = true;
      freefallUntilMs = now + 1000UL;
  }

  lastAx = ax;
  lastAy = ay;
  lastAz = az;
}

void sendTelemetry() {
  unsigned long now = millis();
  if (now - lastTelemetryMs < 200UL) return;
  lastTelemetryMs = now;
  if (WiFi.status() != WL_CONNECTED) return;

  // Determine current expression name and speech text
  const char* exprName = "NORMAL";
  const char* speech = "";
  if (now < freefallUntilMs) { exprName = "FREEFALL"; speech = "AAAA!"; }
  else if (now < glitchUntilMs) { exprName = "GLITCH"; speech = "ERR.."; }
  else if (now < cryUntilMs) { exprName = "MENANGIS"; speech = "HUHU.."; }
  else if (superAngryMode) { exprName = "SUPER MARAH"; speech = "GRRR!!"; }
  else if (angryMode) { exprName = "MARAH"; speech = "HEH!"; }
  else if (now < laughUntilMs) { exprName = "KETAWA"; speech = "HAHA!"; }
  else if (now < dizzyUntilMs) { exprName = "PUSING"; speech = "PUSING~"; }
  else if (now < pantUntilMs) { exprName = "KEPANASAN"; speech = "PANAS!"; }
  else if (now < sadUntilMs) { exprName = "SEDIH"; speech = "HMM.."; }
  else if (now < surprisedUntilMs) { exprName = "KAGET"; speech = "WAH!"; }
  else if (now < nodUntilMs) { exprName = "ANGGUK"; speech = "IYA!"; }
  else if (now < headShakeUntilMs) { exprName = "GELENG"; speech = "NGGAK!"; }
  else if (now < touchLoveUntilMs) { exprName = "SAYANG"; speech = "<3"; }
  else if (now < touchHappyUntilMs) { exprName = "SENANG"; speech = "HEH~"; }
  else if (now < annoyedUntilMs) { exprName = "KESAL"; speech = "ISH.."; }
  else if (now < touchSleepyUntilMs || (touchDown && (now - touchDownMs > 2000UL))) { exprName = "NGANTUK"; speech = "zzz"; }
  else if (now < micShoutUntilMs) { exprName = "TERIAKAN"; speech = "HEI!"; }
  else if (playing) { exprName = "MUSIK"; speech = "~LA~"; }
  else if (faceDownMode) { exprName = "SEDIH"; speech = "HMM.."; }

  char telemetryJson[800];
  snprintf(telemetryJson, sizeof(telemetryJson),
    "{"
    "\"mpu\":%d,\"dht\":%d,\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f,"
    "\"gx\":%.3f,\"gy\":%.3f,\"gz\":%.3f,\"tiltX\":%.2f,\"tiltY\":%.2f,"
    "\"shake\":%.2f,\"shakeMeter\":%.2f,\"angry\":%d,\"temp\":%.1f,\"hum\":%.0f,"
    "\"nod\":%d,\"headShake\":%d,\"surprised\":%d,\"curious\":%d,\"faceUp\":%d,\"faceDown\":%d,\"touch\":%d,"
    "\"game\":%d,\"scoreP\":%d,\"scoreA\":%d,"
    "\"laugh\":%d,\"glitch\":%d,\"pant\":%d,\"cry\":%d,\"sleep\":%d,"
    "\"dizzy\":%d,\"sad\":%d,\"annoyed\":%d,\"love\":%d,"
    "\"inmp\":%d,\"max\":%d,\"req_lovestory\":%d,"
    "\"state\":%d,"
    "\"expr\":\"%s\""
    "}",
    (mpuReady || rawMpuMode) ? 1 : 0, dhtReady ? 1 : 0,
    rawAx, rawAy, rawAz, rawGx, rawGy, rawGz,
    tiltX, tiltY, shakeSmooth, shakeMeter, angryMode ? 1 : 0,
    isnan(tempC) ? -99.0f : tempC,
    isnan(humPct) ? -1.0f : humPct,
    (now < nodUntilMs) ? 1 : 0,
    (now < headShakeUntilMs) ? 1 : 0,
    (now < surprisedUntilMs) ? 1 : 0,
    (now < curiousUntilMs) ? 1 : 0,
    0,
    faceDownMode ? 1 : 0,
    touchDown ? 1 : 0,
    (currentState == STATE_GAMES ? (int)gamePhase : 0), scorePlayer, scoreAi,
    (now < laughUntilMs) ? 1 : 0,
    (now < glitchUntilMs) ? 1 : 0,
    (now < pantUntilMs) ? 1 : 0,
    (now < cryUntilMs) ? 1 : 0,
    (now < touchSleepyUntilMs || (touchDown && (now - touchDownMs > 2000UL))) ? 1 : 0,
    (now < dizzyUntilMs) ? 1 : 0,
    (now < sadUntilMs) ? 1 : 0,
    (now < annoyedUntilMs) ? 1 : 0,
    (now < touchLoveUntilMs) ? 1 : 0,
    (int)(levelSmooth * 100), playing ? 1 : 0, req_lovestory ? 1 : 0,
    (int)currentState,
    exprName
  );

  if (req_lovestory) req_lovestory = false;

  telemetryUdp.beginPacket(IPAddress(255, 255, 255, 255), TELEMETRY_PORT);
  telemetryUdp.write((const uint8_t*)telemetryJson, strlen(telemetryJson));
  telemetryUdp.endPacket();
}

void updateDHT() {
  if (!dhtReady) return;
  unsigned long now = millis();
  if (now - lastDhtMs < 2200UL) return;
  lastDhtMs = now;

  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    Serial.println("DHT read failed");
    return;
  }

  tempC = t;
  humPct = h;

  float hot = clampFloat((t - 28.0f) / 7.0f, 0.0f, 1.0f);
  float cold = clampFloat((22.0f - t) / 7.0f, 0.0f, 1.0f);
  float targetTempMood = hot - cold;
  float targetHumidMood = clampFloat((h - 68.0f) / 25.0f, 0.0f, 1.0f);

  tempMood += (targetTempMood - tempMood) * 0.35f;
  humidMood += (targetHumidMood - humidMood) * 0.35f;
  
  if (t > 31.0f) {
      pantUntilMs = now + 5000UL;
  }

  Serial.print("DHT T=");
  Serial.print(t, 1);
  Serial.print("C H=");
  Serial.print(h, 0);
  Serial.println("%");
}

void drawStatus(const char* title, const char* line1, const char* line2 = "") {
  if (!oledReady) return;
  display.clearDisplay();
  display.setTextWrap(false);
  display.fillRect(0, 0, SCREEN_WIDTH, 12, OLED_WHITE);
  display.setTextColor(OLED_BLACK);
  display.setTextSize(1);
  display.setCursor(3, 2);
  display.print(title);
  display.setTextColor(OLED_WHITE);
  display.setCursor(3, 22);
  display.print(line1);
  display.setCursor(3, 38);
  display.print(line2);
  display.display();
}

void drawMochiSmile(int cx, int y, int w, int depth) {
  for (int i = 0; i <= w; i++) {
    float t = (float)i / (float)w;
    float arc = 4.0f * t * (1.0f - t);
    int x = cx - w / 2 + i;
    int yy = y + (int)(arc * depth);
    display.fillCircle(x, yy, 1, OLED_WHITE);
  }
  display.fillCircle(cx - w / 2, y - 1, 1, OLED_WHITE);
  display.fillCircle(cx + w / 2, y - 1, 1, OLED_WHITE);
}

void drawTinyHeart(int hx, int hy) {
  display.fillCircle(hx - 2, hy, 2, OLED_WHITE);
  display.fillCircle(hx + 2, hy, 2, OLED_WHITE);
  display.fillTriangle(hx - 5, hy + 1, hx + 5, hy + 1, hx, hy + 7, OLED_WHITE);
}

void drawSweatDrop(int x, int y) {
  display.fillCircle(x, y + 7, 3, OLED_WHITE);
  display.fillTriangle(x - 2, y + 5, x + 2, y + 5, x, y, OLED_WHITE);
}

void drawSpark(int sx, int sy, int size) {
  display.drawPixel(sx, sy, OLED_WHITE);
  for (int i = 1; i <= size; i++) {
    display.drawPixel(sx, sy - i, OLED_WHITE);
    display.drawPixel(sx, sy + i, OLED_WHITE);
    display.drawPixel(sx - i, sy, OLED_WHITE);
    display.drawPixel(sx + i, sy, OLED_WHITE);
  }
}

void drawHappyEye(int cx, int cy, int w) {
  int half = w / 2;
  for (int t = 0; t < 3; t++) {
    for (int x = -half; x <= half; x++) {
      float n = (float)x / (float)half;
      int y = cy + (int)((1.0f - n * n) * 6.0f) + t;
      display.drawPixel(cx + x, y, OLED_WHITE);
    }
  }
}

void drawSleepyEye(int x, int y, int w) {
  display.fillRoundRect(x, y + 13, w, 6, 3, OLED_WHITE);
}

void drawDizzyEye(int cx, int cy) {
  display.drawCircle(cx, cy, 7, OLED_WHITE);
  display.drawCircle(cx + 1, cy, 4, OLED_WHITE);
  display.drawPixel(cx - 2, cy - 1, OLED_WHITE);
  display.drawPixel(cx + 3, cy + 2, OLED_WHITE);
}

void drawShiverMarks(int x, int y) {
  display.drawLine(x, y, x + 3, y + 3, OLED_WHITE);
  display.drawLine(x + 3, y + 3, x, y + 6, OLED_WHITE);
  display.drawLine(x + 7, y + 1, x + 10, y + 4, OLED_WHITE);
  display.drawLine(x + 10, y + 4, x + 7, y + 7, OLED_WHITE);
}

void drawAngerMark(int x, int y) {
  display.drawLine(x, y, x + 5, y + 5, OLED_WHITE);
  display.drawLine(x + 5, y, x, y + 5, OLED_WHITE);
  display.drawLine(x + 1, y, x + 6, y + 5, OLED_WHITE);
  display.drawLine(x + 6, y, x + 1, y + 5, OLED_WHITE);
}

void drawBlushLines(int cx, int cy) {
  for (int i = 0; i < 3; i++) {
    int x = cx - 5 + i * 4;
    display.drawLine(x, cy + 2, x + 2, cy, OLED_WHITE);
  }
}

void drawDasaiMouth(int cx, int y) {
  display.drawBitmap(cx - 8, y, img_normal_mouth, 16, 6, OLED_WHITE);
}

void drawCrossEye(int cx, int cy, int size) {
  for (int i = -2; i <= 2; i++) {
    display.drawLine(cx - size + i, cy - size, cx + size + i, cy + size, OLED_WHITE);
    display.drawLine(cx - size + i, cy + size, cx + size + i, cy - size, OLED_WHITE);
  }
}

void drawHollowEye(int cx, int cy, int rx, int ry) {
  for (int i = 0; i < 3; i++) {
    display.drawRoundRect(cx - rx + i, cy - ry + i, (rx-i)*2, (ry-i)*2, 6, OLED_WHITE);
  }
}

void drawCaretEye(int cx, int cy, int size) {
  for (int i = 0; i < 3; i++) {
    display.drawLine(cx - size, cy + i, cx, cy - size + i, OLED_WHITE);
    display.drawLine(cx, cy - size + i, cx + size, cy + i, OLED_WHITE);
  }
}

void drawTTEye(int cx, int cy, int w, int h) {
  display.fillRect(cx - w/2, cy - h/2, w, 4, OLED_WHITE);
  display.fillRect(cx - 2, cy - h/2, 4, h, OLED_WHITE);
}

void drawDashEye(int cx, int cy, int w) {
  display.fillRect(cx - w/2, cy - 2, w, 4, OLED_WHITE);
}

void drawAngryLineEye(int cx, int cy, int w, int h, bool left) {
  for (int i = -1; i <= 2; i++) {
    if (left) display.drawLine(cx - w/2 + i, cy - h/2, cx + w/2 + i, cy + h/2, OLED_WHITE);
    else display.drawLine(cx - w/2 + i, cy + h/2, cx + w/2 + i, cy - h/2, OLED_WHITE);
  }
}

void drawSoftBrows(int leftX, int rightX, int y, int w, int style) {
  int lx1 = leftX + 1;
  int lx2 = leftX + w - 1;
  int rx1 = rightX + 1;
  int rx2 = rightX + w - 1;
  if (style == 1) {
    display.drawLine(lx1, y + 4, lx2, y, OLED_WHITE);
    display.drawLine(rx1, y, rx2, y + 4, OLED_WHITE);
  } else if (style == 2) {
    display.drawLine(lx1, y, lx2, y + 3, OLED_WHITE);
    display.drawLine(rx1, y + 3, rx2, y, OLED_WHITE);
  } else if (style == 3) {
    display.drawLine(lx1, y + 2, lx2, y + 1, OLED_WHITE);
    display.drawLine(rx1, y + 1, rx2, y + 2, OLED_WHITE);
  }
}

void drawCatEars(int cx, int topY, int lean, bool wiggle, float amount) {
  unsigned long now = millis();
  int w = wiggle ? (int)(sinf(now * 0.009f) * amount * 3.0f) : 0;
  int lx = cx - 30 + lean;
  int rx = cx + 30 + lean;
  display.drawTriangle(lx - 4, topY + 7, lx + 5, topY + 7, lx + w, topY - 4, OLED_WHITE);
  display.drawTriangle(rx - 5, topY + 7, rx + 4, topY + 7, rx - w, topY - 4, OLED_WHITE);
}

void drawPupils(int lx, int rx, int ly, int ry, int w, int lh, int rh, int lookX, int lookY) {
  int pupilR = 3;
  int maxPX = w / 2 - pupilR - 2;
  int maxPYL = lh / 2 - pupilR - 2;
  int maxPYR = rh / 2 - pupilR - 2;
  if (maxPX < 1) maxPX = 1;
  if (maxPYL < 1) maxPYL = 1;
  if (maxPYR < 1) maxPYR = 1;
  int px = (int)clampFloat(lookX * 0.35f, -(float)maxPX, (float)maxPX);
  int pyL = (int)clampFloat(lookY * 0.5f, -(float)maxPYL, (float)maxPYL);
  int pyR = (int)clampFloat(lookY * 0.5f, -(float)maxPYR, (float)maxPYR);
  display.fillCircle(lx + w / 2 + px, ly + lh / 2 + pyL, pupilR, OLED_BLACK);
  display.drawPixel(lx + w / 2 + px - 1, ly + lh / 2 + pyL - 1, OLED_WHITE);
  display.fillCircle(rx + w / 2 + px, ry + rh / 2 + pyR, pupilR, OLED_BLACK);
  display.drawPixel(rx + w / 2 + px - 1, ry + rh / 2 + pyR - 1, OLED_WHITE);
}

void drawHeartEye(int cx, int cy, int sz) {
  int r = sz > 6 ? sz / 3 : 2;
  display.fillCircle(cx - r, cy - r / 2, r, OLED_WHITE);
  display.fillCircle(cx + r, cy - r / 2, r, OLED_WHITE);
  display.fillTriangle(cx - r * 2, cy, cx + r * 2, cy, cx, cy + r * 2, OLED_WHITE);
}

void drawFloatingNote(int x, int y, bool beamed) {
  display.drawFastVLine(x + 3, y, 9, OLED_WHITE);
  display.fillCircle(x + 1, y + 9, 2, OLED_WHITE);
  if (beamed) {
    display.drawFastVLine(x + 8, y + 2, 7, OLED_WHITE);
    display.fillCircle(x + 6, y + 9, 2, OLED_WHITE);
    display.drawFastHLine(x + 3, y, 5, OLED_WHITE);
    display.drawFastHLine(x + 3, y + 2, 5, OLED_WHITE);
  }
}

void drawZzz(int x, int y) {
  unsigned long now = millis();
  int d = (int)(sinf(now * 0.002f) * 2.0f);
  display.setTextSize(1);
  display.setTextColor(OLED_WHITE);
  display.setCursor(x + d, y);
  display.print("z");
  display.setCursor(x + 5 + (int)((float)d * 0.7f), y - 7);
  display.print("z");
  display.setCursor(x + 9 + (int)((float)d * 0.5f), y - 12);
  display.print("Z");
}

void drawBlushDots(int cx, int cy) {
  for (int i = 0; i < 3; i++) {
    display.drawPixel(cx - 4 + i * 3, cy, OLED_WHITE);
    display.drawPixel(cx - 3 + i * 3, cy + 1, OLED_WHITE);
  }
}

void drawMochiTinyMouth(int cx, int y, bool active) {
  float voice = levelSmooth;
  if (!active || voice < 0.10f) {
    drawMochiSmile(cx, y, 25, 6);
    return;
  }

  int smileW = 24 + (int)(voice * 8.0f);
  int smileD = 5 + (int)(voice * 4.0f);
  if (smileW > 32) smileW = 32;
  if (smileD > 9) smileD = 9;
  int syllable = (millis() / 145UL) % 4UL;
  drawMochiSmile(cx, y - 1 + (syllable == 1 ? 1 : 0), smileW, smileD);

  if (voice > 0.28f) {
    int open = 2 + (int)(voice * 6.0f);
    if (open > 8) open = 8;
    int ow = syllable == 2 ? 10 : 8;
    display.fillRoundRect(cx - ow / 2, y + 4, ow, open, 3, OLED_WHITE);
    if (open > 4) {
      display.fillRoundRect(cx - ow / 2 + 2, y + 6, ow - 4, open - 3, 2, OLED_BLACK);
    }
  }
}

void drawSurprisedMouth(int cx, int y) {
  display.drawCircle(cx, y + 3, 5, OLED_WHITE);
  display.drawCircle(cx, y + 3, 4, OLED_WHITE);
}

void drawWavyMouth(int cx, int y) {
  unsigned long now = millis();
  for (int i = -8; i <= 8; i++) {
    float wave = sinf((float)i * 0.4f + now * 0.008f) * 1.5f;
    display.drawPixel(cx + i, y + (int)wave, OLED_WHITE);
    display.drawPixel(cx + i, y + (int)wave + 1, OLED_WHITE);
  }
}

void drawPoutMouth(int cx, int y) {
  display.fillRoundRect(cx - 4, y, 8, 5, 2, OLED_WHITE);
  display.fillRoundRect(cx - 2, y + 1, 4, 3, 1, OLED_BLACK);
}

void drawMochi(bool active) {
  if (!oledReady) return;
  unsigned long now = millis();
  float songSec = (float)playedBytes / (float)(AUDIO_RATE * 2UL);
  CueMood cue = active ? cueMoodAt(songSec) : CUE_IDLE;
  float targetCue = active ? 1.0f : 0.0f;
  cueSmooth += (targetCue - cueSmooth) * 0.12f;
  float voice = active ? levelSmooth : 0.0f;
  float beat = active ? fabsf(sinf(now * 0.0105f)) : 0.0f;
  float phrase = active ? fabsf(sinf(now * 0.0017f)) : fabsf(sinf(now * 0.0009f));
  float danceBeat = active ? sinf(now * 0.0125f) : 0.0f;
  float danceStep = active ? (danceBeat >= 0.0f ? 1.0f : -1.0f) : 0.0f;
  float motion = mpuReady ? shakeSmooth : 0.0f;
  float tiltLookX = mpuReady ? tiltX * 4.0f : 0.0f;
  float tiltLookY = mpuReady ? tiltY * 2.2f : 0.0f;

  if (!active && motion < 0.1f && now >= nextIdleExprMs) {
    idleExpr = random(0, 10); // 0=Normal,1=Curious,2=Shy,3=Bored,4=Excited,5=Squint,6=Thinking,7=Smug,8=BigEyes,9=HalfClosed
    nextIdleExprMs = now + random(4000, 10000);
    // Occasionally wink during idle
    if (random(0, 3) == 0 && !isWinking) {
      isWinking = true;
      winkLeft = random(0, 2) == 0;
      winkStartMs = now + random(500, 2000);
      winkProgress = 0.0f;
    }
  }

  // Wink system (separate from blink)
  if (isWinking && now >= winkStartMs && !isBlinking) {
    long elapsed = now - winkStartMs;
    if (elapsed < 80) winkProgress = elapsed / 80.0f;
    else if (elapsed < 500) winkProgress = 1.0f;
    else if (elapsed < 600) winkProgress = 1.0f - (elapsed - 500) / 100.0f;
    else { isWinking = false; winkProgress = 0.0f; nextWinkMs = now + random(8000, 20000); }
  }

  if (now > nextBlinkMs && !isBlinking) {
    isBlinking = true;
    blinkStartMs = now;
    nextBlinkMs = now + random(2000, 6000);
  }
  if (isBlinking) {
    long elapsed = now - blinkStartMs;
    if (elapsed < 60) blinkProgress = elapsed / 60.0f;
    else if (elapsed < 140) blinkProgress = 1.0f - (elapsed - 60) / 80.0f;
    else { isBlinking = false; blinkProgress = 0.0f; }
  }

  float hotMood = tempMood > 0.0f ? tempMood : 0.0f;
  float coldMood = tempMood < 0.0f ? -tempMood : 0.0f;
  float humid = humidMood;
  bool sad = now < sadUntilMs;
  bool angry = !sad && mpuReady && (angryMode || superAngryMode);
  bool superAngry = !sad && mpuReady && superAngryMode;
  bool dizzy = !sad && !angry && (now < dizzyUntilMs);
  bool annoyed = !sad && !angry && !dizzy && (now < annoyedUntilMs);
  bool touchHappy = now < touchHappyUntilMs;
  bool touchLove = now < touchLoveUntilMs;
  bool touchSleepy = now < touchSleepyUntilMs || (touchDown && (now - touchDownMs > 2000UL));
  bool nodding = now < nodUntilMs;
  bool headShaking = now < headShakeUntilMs;
  bool surprised = now < surprisedUntilMs;
  bool curious = now < curiousUntilMs;
  bool freefalling = now < freefallUntilMs;
  float targetBob = active ? sinf(now * 0.007f) * (1.8f + voice * 2.0f) - beat * voice * 3.0f
                           : sinf(now * 0.0018f) * 1.0f;
  // Beat sync: extra drop on strong beat
  if (active) {
    float beatPunch = fabsf(sinf(now * 0.0105f));
    if (beatPunch > 0.85f) targetBob -= voice * 5.0f; // sharp head drop on beat
  }
  // Random organic idle glance system
  if (!active && now >= nextGlanceMs) {
    idleGlanceTargetX = (float)((int)((now / 7) % 13) - 6) * 1.4f;
    idleGlanceTargetY = (float)((int)((now / 11) % 7) - 3) * 0.7f;
    nextGlanceMs = now + 1800UL + (now % 3000UL);
  }
  float targetLookX = active ? sinf(now * 0.0022f) * (4.0f + voice * 3.0f)
                             : idleGlanceTargetX + sinf(now * 0.0009f) * 1.2f;
  float targetLookY = active ? sinf(now * 0.0016f + 1.2f) * (1.2f + voice * 2.0f)
                             : idleGlanceTargetY + sinf(now * 0.0007f) * 0.6f;
  float targetLean = active ? sinf(now * 0.0032f) * voice * 4.0f
                            : sinf(now * 0.0015f) * 1.5f;
  float targetDanceX = active ? danceStep * (1.5f + voice * 5.5f) : 0.0f;
  targetLookX += tiltLookX;
  targetLookY += tiltLookY;
  targetLean += tiltX * 5.0f;
  targetBob += tiltY * 3.5f;
  if (!active) targetBob += motion * 2.0f;
  targetLookY += hotMood * 2.0f - coldMood * 0.7f;
  targetBob += hotMood * 1.2f;
  targetDanceX += sinf(now * 0.045f) * coldMood * 2.8f;
  if (motion > 0.18f) {
    targetLookX += sinf(now * 0.018f) * motion * 3.2f;
    targetLookY += cosf(now * 0.016f) * motion * 1.6f;
    targetDanceX += sinf(now * 0.020f) * motion * 2.4f;
  }
  if (angry) {
    targetLookY += 1.0f;
    targetDanceX *= 0.35f;
    targetLean += sinf(now * 0.024f) * 0.9f;
  }
  if (touchHappy || touchLove) {
    targetLookY -= 1.4f;
    targetBob -= fabsf(sinf(now * 0.012f)) * 1.5f;
    targetDanceX += sinf(now * 0.013f) * (touchLove ? 3.0f : 1.5f);
  }
  if (touchSleepy) {
    targetLookY += 2.0f;
    targetDanceX *= 0.15f;
    targetBob += 1.2f;
  }
  if (nodding) {
    targetBob -= fabsf(sinf(now * 0.015f)) * 3.0f;
  }
  if (headShaking) {
    targetLookX += sinf(now * 0.025f) * 5.0f;
    targetDanceX += sinf(now * 0.028f) * 3.0f;
  }
  if (faceDownMode) {
    targetLookY += 2.5f;
    targetBob += 1.5f;
  }

  switch (cue) {
    case CUE_DREAMY:
      targetLookY -= 1.0f;
      targetBob *= 0.75f;
      targetDanceX *= 0.35f;
      break;
    case CUE_SHY:
      targetLookX -= 3.0f;
      targetLookY += 1.2f;
      targetDanceX *= 0.45f;
      break;
    case CUE_WORRIED:
      targetLookX += sinf(now * 0.0055f) * 2.0f;
      targetLookY += 1.5f;
      targetDanceX *= 0.30f;
      break;
    case CUE_PLEADING:
      targetLookY -= 2.2f;
      targetBob -= 1.0f;
      targetDanceX *= 0.25f;
      break;
    case CUE_RUN:
      targetDanceX *= 1.45f;
      targetLean *= 1.35f;
      targetBob -= beat * 2.0f;
      break;
    case CUE_PRINCE:
      targetLookY -= 0.8f;
      targetDanceX *= 1.15f;
      break;
    case CUE_HAPPY:
      targetDanceX *= 1.25f;
      targetBob -= beat * 1.4f;
      break;
    case CUE_LONELY:
      targetLookY += 2.0f;
      targetDanceX *= 0.20f;
      targetBob += 1.0f;
      break;
    case CUE_RING:
      targetLookY -= 3.0f;
      targetDanceX *= 0.55f;
      targetBob -= 1.5f;
      break;
    case CUE_FINALE:
      targetDanceX *= 1.60f;
      targetBob -= beat * 2.2f;
      break;
    default:
      break;
  }

  if (now - lastJoyMs < 3000UL) {
    targetLookX = (float)manualJoyX;
    targetLookY = (float)manualJoyY;
  }
  if (active) {
    targetBob -= voice * 20.0f;
  }
  // Mic bob: gentle head-nod when hearing voice (not playing music)
  if (!active && now < micActiveUntilMs) {
    float micBob = sinf(now * 0.011f) * micSmooth * 4.0f;
    targetBob += micBob;
    targetLookX += sinf(now * 0.007f) * micSmooth * 2.0f;
  }

  float smoothX = 0.10f;
  float smoothY = 0.09f;
  faceBob += (targetBob - faceBob) * 0.14f;
  faceLookX += (targetLookX - faceLookX) * smoothX;
  faceLookY += (targetLookY - faceLookY) * smoothY;
  faceLean += (targetLean - faceLean) * 0.12f;
  faceDanceX += (targetDanceX - faceDanceX) * 0.16f;

  int dx = (int)(faceLookX + faceDanceX);
  int dy = (int)(faceLookY + faceBob);

  // --- 1. Set Target Face based on State ---
  breathPhase += 0.002f;
  if (breathPhase > 6.28f) breathPhase = 0.0f;
  float breathe = sinf(breathPhase) * 0.8f; // gentle breathing offset

  if (freefallUntilMs > millis()) {
      // Freefall: Screaming O_O wide eyes
      targetFace = {32.0f, 44.0f, 16.0f, 55.0f, -8.0f, 0.0f, 20.0f, 22.0f, 40.0f, -12.0f, 0.0f};
  } else if (now < laughUntilMs) {
      // Laughing: > < squinty eyes, huge D mouth
      targetFace = {30.0f, 10.0f, 4.0f, 48.0f, -6.0f, 15.0f, 32.0f, 26.0f, 36.0f, 20.0f, 24.0f};
      dy += (int)(sinf(now * 0.06f) * 4.0f); // laughing shake
  } else if (now < glitchUntilMs) {
      // Glitch: Flat wide eyes, straight mouth
      targetFace = {36.0f, 6.0f, 0.0f, 50.0f, 0.0f, 0.0f, 28.0f, 4.0f, 38.0f, 0.0f, 0.0f};
      dx += (int)(sinf(now * 0.1f) * 8.0f); // Fast horizontal shaking
  } else if (now < cryUntilMs) {
      // Crying: Droopy heavy brows, trembling mouth
      targetFace = {24.0f, 38.0f, 10.0f, 46.0f, 18.0f, -15.0f, 14.0f, 8.0f, 38.0f, -6.0f, 0.0f};
      dy += (int)(sinf(now * 0.05f) * 2.0f);
  } else if (now < pantUntilMs) {
      // Panting: Squinty eyes, open mouth
      targetFace = {26.0f, 16.0f, 6.0f, 48.0f, 5.0f, 5.0f, 20.0f, 24.0f, 36.0f, -4.0f, 0.0f};
      dy += (int)(sinf(now * 0.03f) * 3.0f); // Heavy breathing
  } else if (active || voice > 0.15f) {
      // Music / Active: Happy squinting eyes, big smile
      targetFace = {28.0f, 26.0f, 10.0f, 48.0f, 0.0f, 0.0f, 26.0f, 14.0f, 31.0f, 12.0f, 18.0f};
  } else if (superAngryMode) {
      // SUPER ANGRY (sustained shake) — extreme V-brows, hard frown, trembling
      float tremble = sinf(now * 0.05f) * 2.0f; // rapid micro-trembler
      targetFace = {30.0f, 40.0f, 10.0f, 50.0f,
                    20.0f, 18.0f,
                    24.0f, 10.0f, 37.0f, -8.0f, 0.0f};
      dx += (int)tremble; dy += (int)(fabsf(tremble) * 0.5f);
  } else if (angryMode) {
      // Angry: Hard angled brows matching Lopaka bitmap style
      targetFace = {28.0f, 38.0f, 10.0f, 49.0f,
                    18.0f, 16.0f,
                    22.0f, 10.0f, 36.0f, -6.0f, 0.0f};
  } else if (now < sadUntilMs) {
      // Sad: Droopy brows, slight frown, looking down
      targetFace = {24.0f, 35.0f, 10.0f, 46.0f, 12.0f, -10.0f, 18.0f, 6.0f, 37.0f, -3.0f, 0.0f};
  } else if (now < surprisedUntilMs) {
      // Surprised: Tall round eyes
      targetFace = {22.0f, 42.0f, 11.0f, 50.0f, -6.0f, 0.0f, 10.0f, 16.0f, 38.0f, 0.0f, 0.0f};
  } else if (nodding || touchHappy) {
      // Happy: Squinty arch eyes, big smile
      targetFace = {28.0f, 24.0f, 10.0f, 48.0f, 0.0f, 0.0f, 26.0f, 14.0f, 31.0f, 12.0f, 20.0f};
  } else if (faceDownMode) {
      // Sad/face down: Droopy
      targetFace = {24.0f, 35.0f, 10.0f, 46.0f, 12.0f, -10.0f, 18.0f, 6.0f, 37.0f, -3.0f, 0.0f};
  } else if (now < dizzyUntilMs || headShakeDetected) {
      // Dizzy: squished flat eyes (the swirl is handled in drawing below)
      targetFace = {40.0f, 10.0f, 2.0f, 55.0f, 0.0f, 0.0f, 10.0f, 4.0f, 35.0f, 0.0f, 0.0f};
  } else if (touchSleepy) {
      // Sleepy: Flat half-closed eyes with breathing
      targetFace = {26.0f, 9.0f + breathe * 0.5f, 4.0f, 48.0f, 0.0f, 0.0f, 16.0f, 5.0f, 36.0f, 0.0f, 0.0f};
  } else if (now < annoyedUntilMs) {
      // Annoyed: Slightly lowered brows, flat mouth
      targetFace = {26.0f, 32.0f, 9.0f, 48.0f, 7.0f, 5.0f, 18.0f, 6.0f, 35.0f, 0.0f, 0.0f};
  } else if (!active && now < micShoutUntilMs) {
      // SHOUT/LOUD: Wide startled eyes, raised brows, big O mouth
      targetFace = {26.0f, 40.0f + fabsf(sinf(now * 0.010f)) * 3.0f, 13.0f, 50.0f,
                    -8.0f, 0.0f, 14.0f, 18.0f, 38.0f, 0.0f, 0.0f};
      dy -= 3; // head pops up slightly
  } else if (!active && now < micActiveUntilMs) {
      // LISTENING / TALKING: Alert eyes, animated open mouth
      float openness = 6.0f + micSmooth * 20.0f; // mouth opens with volume
      float eyeW = 26.0f + micSmooth * 4.0f;
      float eyeH = 34.0f + micSmooth * 6.0f;     // eyes widen when louder
      targetFace = {eyeW, eyeH, 10.0f, 48.0f,
                    0.0f, 0.0f,
                    18.0f + micSmooth * 6.0f, openness, 36.0f,
                    micSmooth * 4.0f, micSmooth * 6.0f};
  } else {
      // Normal or Idle Expressions
      switch (idleExpr) {
        case 1: // Curious - head tilts implied by MPU, eyes slightly diff size
          targetFace = {26.0f, 37.0f + breathe, 12.0f, 48.0f, 5.0f, 8.0f, 16.0f, 6.0f, 35.0f, 4.0f, 0.0f};
          break;
        case 2: // Shy - smaller eyes, look away with slight blush
          targetFace = {24.0f, 28.0f + breathe, 12.0f, 46.0f, 0.0f, 0.0f, 14.0f, 5.0f, 37.0f, 2.0f, 12.0f};
          dx -= 6; dy += 4;
          break;
        case 3: // Bored - half closed, looking down
          targetFace = {26.0f, 16.0f + breathe, 8.0f, 48.0f, 0.0f, 0.0f, 18.0f, 4.0f, 35.0f, -2.0f, 0.0f};
          dy += 6;
          break;
        case 4: // Excited - slightly larger, bright eyes
          targetFace = {28.0f, 42.0f + breathe, 14.0f, 50.0f, -2.0f, 0.0f, 20.0f, 10.0f, 34.0f, 8.0f, 0.0f};
          dy -= 2;
          break;
        case 5: // Squint - suspicious
          targetFace = {24.0f, 10.0f + breathe, 4.0f, 46.0f, 2.0f, -2.0f, 14.0f, 4.0f, 36.0f, -1.0f, 0.0f};
          break;
        case 6: // Thinking - looking up and away
          targetFace = {24.0f, 34.0f + breathe, 12.0f, 48.0f, 0.0f, 0.0f, 12.0f, 6.0f, 36.0f, 2.0f, 0.0f};
          dx -= 5; dy -= 4;
          break;
        case 7: // Smug - slight smirk, one brow up
          targetFace = {26.0f, 34.0f + breathe, 12.0f, 48.0f, 2.0f, 5.0f, 20.0f, 7.0f, 35.0f, 3.0f, 0.0f};
          dx += 2;
          break;
        case 8: // BigEyes - soft, cute look
          targetFace = {32.0f, 46.0f + breathe, 16.0f, 52.0f, -4.0f, 0.0f, 16.0f, 8.0f, 34.0f, 6.0f, 8.0f};
          break;
        case 9: // HalfClosed - relaxed
          targetFace = {28.0f, 22.0f + breathe, 10.0f, 50.0f, 2.0f, 0.0f, 16.0f, 5.0f, 36.0f, 2.0f, 0.0f};
          break;
        default: // Normal with gentle breathing (taller, more pill-shaped eyes)
          targetFace = {28.0f, 42.0f + breathe, 14.0f, 50.0f, 0.0f, 0.0f, 18.0f, 6.0f, 35.0f, 4.0f, 0.0f};
          break;
      }
  }

  if (curiousMode && idleExpr != 1) {
      targetFace.browDrop = 5.0f;
      targetFace.browAngle = 8.0f;
  }

  // --- 2. Smooth Interpolation ---
  float easing = 0.25f;
  currentFace.eyeW += (targetFace.eyeW - currentFace.eyeW) * easing;
  currentFace.eyeH += (targetFace.eyeH - currentFace.eyeH) * easing;
  currentFace.eyeR += (targetFace.eyeR - currentFace.eyeR) * easing;
  currentFace.eyeSpacing += (targetFace.eyeSpacing - currentFace.eyeSpacing) * easing;
  currentFace.browDrop += (targetFace.browDrop - currentFace.browDrop) * easing;
  currentFace.browAngle += (targetFace.browAngle - currentFace.browAngle) * easing;
  currentFace.mouthW += (targetFace.mouthW - currentFace.mouthW) * easing;
  currentFace.mouthH += (targetFace.mouthH - currentFace.mouthH) * easing;
  currentFace.mouthY += (targetFace.mouthY - currentFace.mouthY) * easing;
  currentFace.mouthCurve += (targetFace.mouthCurve - currentFace.mouthCurve) * easing;
  currentFace.cheekRise += (targetFace.cheekRise - currentFace.cheekRise) * easing;

  // --- 3. Procedural Drawing ---
  display.clearDisplay();
  
  int cx = SCREEN_WIDTH / 2 + dx;
  int cy = SCREEN_HEIGHT / 2 + dy - 15; // Eyes base height
  
  // --- 3. Draw Eyes ---
  // Dizzy: make eyes wobble/spin visually
  float dizzyWobbleL = 0.0f, dizzyWobbleR = 0.0f;
  if (now < dizzyUntilMs) {
    float t = (float)(now % 800) / 800.0f; // 0→1 repeating cycle
    dizzyWobbleL =  sinf(t * 6.28f) * 5.0f;   // left eye swings up-down
    dizzyWobbleR = -sinf(t * 6.28f) * 5.0f;   // right eye opposite phase → "spiral" look
  }

  // Left Eye
  int lx = cx - currentFace.eyeSpacing / 2;
  int ly = cy + (int)dizzyWobbleL;
  float actualLeftH = currentFace.eyeH;
  bool doWinkLeft = isWinking && winkLeft;
  bool doWinkRight = isWinking && !winkLeft;
  if ((isBlinking || doWinkLeft) && (!dizzy && !touchSleepy)) {
      float prog = doWinkLeft ? winkProgress : blinkProgress;
      actualLeftH = currentFace.eyeH * (1.0f - prog) + 2.0f * prog;
  }
  display.fillRoundRect(lx - currentFace.eyeW/2, ly - actualLeftH/2, currentFace.eyeW, (int)actualLeftH, currentFace.eyeR, OLED_WHITE);

  // Right Eye
  int rx = cx + currentFace.eyeSpacing / 2;
  int ry = cy + (int)dizzyWobbleR;
  float actualRightH = currentFace.eyeH;
  if ((isBlinking || doWinkRight) && (!dizzy && !touchSleepy)) {
      float prog = doWinkRight ? winkProgress : blinkProgress;
      actualRightH = currentFace.eyeH * (1.0f - prog) + 2.0f * prog;
  }
  display.fillRoundRect(rx - currentFace.eyeW/2, ry - actualRightH/2, currentFace.eyeW, (int)actualRightH, currentFace.eyeR, OLED_WHITE);

  // Beat-sync eye squint during music (no pupils — just shape change)
  if (active && voice > 0.3f) {
    float beatPunch = fabsf(sinf(now * 0.0105f));
    if (beatPunch > 0.82f) {
      // Quick squint on beat — mask bottom half with black strip
      int squintH = (int)(actualLeftH * 0.35f * voice);
      display.fillRect(lx - currentFace.eyeW/2, ly + (int)(actualLeftH/2) - squintH,
                       (int)currentFace.eyeW, squintH + 2, OLED_BLACK);
      display.fillRect(rx - currentFace.eyeW/2, ry + (int)(actualRightH/2) - squintH,
                       (int)currentFace.eyeW, squintH + 2, OLED_BLACK);
    }
  }

  // Blush dots for shy / love
  if ((idleExpr == 2 || touchLove) && !active && !angryMode && !superAngryMode) {
    drawBlushDots(lx - 4, ly + (int)(currentFace.eyeH * 0.7f));
    drawBlushDots(rx + 4, ry + (int)(currentFace.eyeH * 0.7f));
  }
  
  // Mouth
  int mx = cx;
  int my = cy + currentFace.mouthY;
  display.fillRoundRect(mx - currentFace.mouthW/2, my - currentFace.mouthH/2, currentFace.mouthW, currentFace.mouthH, currentFace.mouthH/3, OLED_WHITE);
  
  // Masking for smiles/frowns on mouth
  if (currentFace.mouthCurve > 0.5f) { // Smile: Mask top
      display.fillCircle(mx, my - currentFace.mouthH/2 - currentFace.mouthCurve, currentFace.mouthW, OLED_BLACK);
  } else if (currentFace.mouthCurve < -0.5f) { // Frown: Mask bottom
      display.fillCircle(mx, my + currentFace.mouthH/2 - currentFace.mouthCurve, currentFace.mouthW, OLED_BLACK);
  }

  // Masking for Happy Cheeks (bottom of eyes)
  if (currentFace.cheekRise > 1.0f) {
      display.fillCircle(lx, ly + currentFace.eyeH/2 + 15 - currentFace.cheekRise, 15, OLED_BLACK);
      display.fillCircle(rx, ry + currentFace.eyeH/2 + 15 - currentFace.cheekRise, 15, OLED_BLACK);
  }

  // Masking for Angry/Sad Brows (Top of eyes using big triangles)
  if (abs(currentFace.browDrop) > 1.0f || abs(currentFace.browAngle) > 1.0f) {
      // Left brow
      int l_tl_x = lx - 30, l_tl_y = ly - 40;
      int l_tr_x = lx + 30, l_tr_y = ly - 40;
      int l_bl_x = lx - 30, l_bl_y = ly - actualLeftH/2 + currentFace.browDrop - currentFace.browAngle;
      int l_br_x = lx + 30, l_br_y = ly - actualLeftH/2 + currentFace.browDrop + currentFace.browAngle;
      display.fillTriangle(l_tl_x, l_tl_y, l_tr_x, l_tr_y, l_bl_x, l_bl_y, OLED_BLACK);
      display.fillTriangle(l_tr_x, l_tr_y, l_br_x, l_br_y, l_bl_x, l_bl_y, OLED_BLACK);

      // Right brow (mirrored angle)
      int r_tl_x = rx - 30, r_tl_y = ry - 40;
      int r_tr_x = rx + 30, r_tr_y = ry - 40;
      int r_bl_x = rx - 30, r_bl_y = ry - actualRightH/2 + currentFace.browDrop + currentFace.browAngle;
      int r_br_x = rx + 30, r_br_y = ry - actualRightH/2 + currentFace.browDrop - currentFace.browAngle;
      
      display.fillTriangle(r_tl_x, r_tl_y, r_tr_x, r_tr_y, r_bl_x, r_bl_y, OLED_BLACK);
      display.fillTriangle(r_tr_x, r_tr_y, r_br_x, r_br_y, r_bl_x, r_bl_y, OLED_BLACK);
  }

  // Panting Tongue
  if (now < pantUntilMs) {
      int tongueW = 8;
      int tongueH = 10 + (int)(sinf(now * 0.05f) * 4.0f); // panting motion
      display.fillRoundRect(mx - tongueW/2, my + currentFace.mouthH/2, tongueW, tongueH, 3, OLED_WHITE);
      display.drawLine(mx, my + currentFace.mouthH/2, mx, my + currentFace.mouthH/2 + tongueH - 2, OLED_BLACK); // tongue line
  }

  // Crying Tears
  if (now < cryUntilMs) {
      float tearDrop = (float)(now % 1000) / 1000.0f; // 0 to 1
      int tearY = ly + currentFace.eyeH/2 + (int)(tearDrop * 20.0f);
      int tearSize = 4 - (int)(tearDrop * 2.0f);
      if (tearSize > 0) {
        display.fillCircle(lx - 5, tearY, tearSize, OLED_WHITE);
        display.fillCircle(rx + 5, tearY, tearSize, OLED_WHITE);
      }
  }

  // Glitch
  if (now < glitchUntilMs) {
      // draw random static lines
      for(int i=0; i<5; i++) {
          int gy = random(cy - 20, cy + 20);
          display.drawLine(cx - 30, gy, cx + 30, gy, OLED_BLACK);
          display.drawLine(cx - 32 + random(0, 5), gy + 1, cx + 28 + random(0, 5), gy + 1, OLED_WHITE);
      }
      display.setCursor(cx - 10, cy - 25);
      display.setTextSize(1);
      display.print("ERR");
  }

  // Zzz for sleepy
  if (!dizzy && touchSleepy) {
      display.setCursor(rx + 20, ry - 10);
      display.setTextSize(1);
      display.setTextColor(OLED_WHITE);
      display.print("Zzz");
      
      // Anime Sleep Bubble
      float bubbleAnim = sinf(now * 0.002f);
      if (bubbleAnim > 0) {
          int bubR = 4 + (int)(bubbleAnim * 8.0f);
          display.drawCircle(mx + 10, my - 5, bubR, OLED_WHITE);
          display.drawCircle(mx + 12, my - 7, bubR/3, OLED_WHITE); // reflection
      }
  }

  display.display();
}

void initPingpong(bool resetScore) {
  ballX = 64.0f; ballY = 32.0f;
  ballVX = 2.0f;
  ballVY = (float)random(-15, 16) * 0.1f;
  paddlePlayerY = 32.0f; paddleAiY = 32.0f;
  if (resetScore) {
    scorePlayer = 0;
    scoreAi = 0;
  }
  gamePhase = GAME_WAIT;
  gameWaitStartMs = millis();
  lastGameTickMs = millis();
}

void updateGame() {
  if (currentState != STATE_GAMES) return;
  if (gamePhase != GAME_PLAYING && gamePhase != GAME_WAIT) return;
  
  unsigned long now = millis();
  unsigned long elapsed = now - lastGameTickMs;
  if (elapsed < 16) return; // fixed ~60fps tick
  lastGameTickMs = now;
  float dt = elapsed * 0.06f;
  if (dt > 2.5f) dt = 2.5f;

  if (gamePhase == GAME_WAIT) {
    if (now - gameWaitStartMs > 3000UL) {
      gamePhase = GAME_PLAYING;
    }
  }

  // Move ball only if playing
  if (gamePhase == GAME_PLAYING) {
    ballX += ballVX * dt;
    ballY += ballVY * dt;

    // Bounce top/bottom
    if (ballY < 0.0f) { ballY = 0.0f; ballVY = -ballVY; }
    if (ballY > 63.0f) { ballY = 63.0f; ballVY = -ballVY; }
  }

  // Player paddle: use web joystick (manualJoyY) OR MPU tilt
  float targetPlayerY;
  if (millis() - lastJoyMs < 500 && manualJoyY != 0) {
    // Web control: manualJoyY -100..100 mapped to 8..56
    targetPlayerY = 32.0f + (manualJoyY / 100.0f) * 24.0f;
  } else {
    // MPU tilt control
    targetPlayerY = 32.0f + (tiltY * 40.0f);
  }
  if (targetPlayerY < 8.0f) targetPlayerY = 8.0f;
  if (targetPlayerY > 56.0f) targetPlayerY = 56.0f;
  paddlePlayerY += (targetPlayerY - paddlePlayerY) * 0.35f;

  // AI logic (tracks ball with limited speed)
  float aiSpeed = 1.6f * dt;
  if (paddleAiY < ballY - 4.0f) paddleAiY += aiSpeed;
  if (paddleAiY > ballY + 4.0f) paddleAiY -= aiSpeed;
  if (paddleAiY < 8.0f) paddleAiY = 8.0f;
  if (paddleAiY > 56.0f) paddleAiY = 56.0f;

  // Collision with Player Paddle (left: x=8)
  if (ballX < 12.0f && ballX > 6.0f) {
    if (ballY > paddlePlayerY - 10.0f && ballY < paddlePlayerY + 10.0f) {
      ballX = 12.0f;
      ballVX = -ballVX;
      // Add english/spin based on hit position
      ballVY += (ballY - paddlePlayerY) * 0.15f;
      // Increase speed slightly
      if (ballVX < 6.0f) ballVX *= 1.05f;
    }
  }

  // Collision with AI Paddle (right: x=120)
  if (ballX > 116.0f && ballX < 122.0f) {
    if (ballY > paddleAiY - 10.0f && ballY < paddleAiY + 10.0f) {
      ballX = 116.0f;
      ballVX = -ballVX;
      ballVY += (ballY - paddleAiY) * 0.15f;
      if (ballVX > -6.0f) ballVX *= 1.05f;
    }
  }

  // Score
  if (gamePhase == GAME_PLAYING) {
    if (ballX < 0.0f) {
      scoreAi++;
      if (scoreAi >= 5) gamePhase = GAME_OVER;
      else initPingpong(); // reset round
    } else if (ballX > 128.0f) {
      scorePlayer++;
      if (scorePlayer >= 5) gamePhase = GAME_OVER;
      else initPingpong(); // reset round
    }
  }
}

void drawPingpong() {
  if (!oledReady) return;
  display.clearDisplay();

  if (gamePhase == GAME_IDLE) {
    display.setTextSize(1);
    display.setTextColor(OLED_WHITE);
    display.setCursor(40, 10);
    display.print("PINGPONG");
    display.setCursor(25, 28);
    display.print("Hold: Keluar");
    display.setCursor(25, 40);
    display.print("Tap: Mulai");
    display.display();
    return;
  }
  
  if (gamePhase == GAME_OVER) {
    display.setTextSize(1);
    display.setTextColor(OLED_WHITE);
    display.setCursor(30, 20);
    if (scorePlayer >= 5) display.print("KAMU MENANG!");
    else display.print("KAMU KALAH!");
    display.setCursor(20, 40);
    display.print("Tap untuk Main");
    display.display();
    return;
  }

  // Draw Center Dashed Line
  for (int y = 0; y < 64; y += 4) {
    display.drawPixel(64, y, OLED_WHITE);
  }

  // Draw Scores
  display.setTextSize(2);
  display.setTextColor(OLED_WHITE);
  display.setCursor(30, 4);
  display.print(scorePlayer);
  display.setCursor(85, 4);
  display.print(scoreAi);

  // Draw Ball
  if (gamePhase == GAME_PLAYING) {
    display.fillRect((int)ballX - 1, (int)ballY - 1, 3, 3, OLED_WHITE);
  } else if (gamePhase == GAME_WAIT) {
    display.setTextSize(1);
    display.setCursor(42, 24);
    display.print("READY?");
    
    unsigned long now = millis();
    int timeLeft = 3 - ((now - gameWaitStartMs) / 1000);
    if (timeLeft < 1) timeLeft = 1;
    
    display.setTextSize(2);
    display.setCursor(58, 38);
    display.print(timeLeft);
  }

  // Draw Paddles
  // Player (Left)
  display.fillRect(8, (int)paddlePlayerY - 8, 3, 16, OLED_WHITE);
  // AI (Right)
  display.fillRect(117, (int)paddleAiY - 8, 3, 16, OLED_WHITE);

  display.display();
}

void drawMenu(const char* title, const char** options, int numOptions, int cursor) {
  display.setTextSize(1);
  display.setTextColor(OLED_WHITE);
  display.setCursor(0, 0);
  display.print(title);
  display.drawLine(0, 10, SCREEN_WIDTH, 10, OLED_WHITE);
  
  for(int i=0; i<numOptions; i++) {
    int y = 15 + i*10;
    if (i == cursor) {
      display.fillRect(0, y-1, SCREEN_WIDTH, 10, OLED_WHITE);
      display.setTextColor(OLED_BLACK);
    } else {
      display.setTextColor(OLED_WHITE);
    }
    display.setCursor(5, y);
    display.print(options[i]);
  }
}

void drawUI() {
  if (!oledReady) return;
  display.clearDisplay();
  
  if (currentState == STATE_MENU) {
    const char* menuOpts[] = {"Games", "Dht", "Reminder", "Musik", "Kembali"};
    drawMenu("MENU UTAMA", menuOpts, 5, menuCursor);
  } else if (currentState == STATE_GAMES) {
    drawPingpong();
    return; // drawPingpong calls display.display() itself
  } else if (currentState == STATE_SENSOR) {
    // Nice DHT22 display
    display.setTextSize(1);
    display.setTextColor(OLED_WHITE);
    display.setCursor(30, 0);
    display.print("DHT22");
    display.drawLine(0, 9, 128, 9, OLED_WHITE);
    // Temperature with big font
    display.drawRoundRect(2, 12, 60, 38, 4, OLED_WHITE);
    display.setCursor(8, 16);
    display.print("Suhu");
    display.setTextSize(2);
    display.setCursor(6, 26);
    if (!isnan(tempC)) {
      display.print((int)tempC);
      display.print("C");
    } else display.print("--C");
    display.setTextSize(1);
    // Humidity
    display.drawRoundRect(66, 12, 60, 38, 4, OLED_WHITE);
    display.setCursor(72, 16);
    display.print("Humid");
    display.setTextSize(2);
    display.setCursor(70, 26);
    if (!isnan(humPct)) {
      display.print((int)humPct);
      display.print("%");
    } else display.print("--%");
    display.setTextSize(1);
    // Status bar
    display.setCursor(20, 54);
    display.print("[tap balik]");
  } else if (currentState == STATE_REMINDER) {
    display.setTextSize(1);
    display.setTextColor(OLED_WHITE);
    display.setCursor(0, 0);
    display.print("REMINDER");
    display.drawLine(0, 10, SCREEN_WIDTH, 10, OLED_WHITE);
    
    display.setCursor(0, 20);
    display.setTextWrap(true);
    display.print(globalReminderText);
    display.setTextWrap(false);
  }
  
  display.display();
}

bool setupI2S() {
  i2s_config_t config = {};
  config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  config.sample_rate = AUDIO_RATE;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 12;
  config.dma_buf_len = AUDIO_FRAMES;
  config.use_apll = false; // ESP32-C3 does not support APLL
  config.tx_desc_auto_clear = true;
  config.fixed_mclk = 0;

  i2s_pin_config_t pins = {};
  pins.bck_io_num = MAX_BCLK_PIN;
  pins.ws_io_num = MAX_LRC_PIN;
  pins.data_out_num = MAX_DIN_PIN;
  pins.data_in_num = I2S_PIN_NO_CHANGE;

  if (i2s_driver_install(I2S_NUM_0, &config, 0, nullptr) != ESP_OK) return false;
  if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) return false;
  i2s_zero_dma_buffer(I2S_NUM_0);
  return true;
}

uint16_t ringFree() {
  return AUDIO_RING_SIZE - ringCount;
}

void ringReset() {
  ringHead = 0;
  ringTail = 0;
  ringCount = 0;
  playing = false;
  playedBytes = 0;
  levelSmooth = 0.0f;
  lastSample = 0;
  cueSmooth = 0.0f;
}

void ringPush(const uint8_t* data, uint16_t len) {
  uint16_t space = ringFree();
  if (len > space) len = space;
  for (uint16_t i = 0; i < len; i++) {
    audioRing[ringHead] = data[i];
    ringHead = (ringHead + 1) % AUDIO_RING_SIZE;
    ringCount++;
  }
}

bool ringPopByte(uint8_t& value) {
  if (ringCount == 0) return false;
  value = audioRing[ringTail];
  ringTail = (ringTail + 1) % AUDIO_RING_SIZE;
  ringCount--;
  return true;
}

void acceptClient() {
  if (audioClient && audioClient.connected()) return;
  WiFiClient next = audioServer.available();
  if (!next) return;

  if (audioClient) audioClient.stop();
  audioClient = next;
  audioClient.setNoDelay(true);
  ringReset();
  totalBytes = 0;
  lastAudioMs = millis();
  Serial.println("audio client connected");
  drawStatus("WIFI AUDIO", "Client connected", "buffering...");
}

void readClientToRing() {
  if (!audioClient || !audioClient.connected()) return;
  while (audioClient.available() > 0 && ringFree() > 0) {
    int want = audioClient.available();
    if (want > READ_CHUNK) want = READ_CHUNK;
    if (want > ringFree()) want = ringFree();
    if (want <= 0) return;
    int got = audioClient.read(readBuffer, want);
    if (got <= 0) return;
    ringPush(readBuffer, got);
    totalBytes += got;
    lastAudioMs = millis();
  }
}

void writeAudioBlock() {
  if (!playing) {
    if (ringCount < AUDIO_PREBUFFER_BYTES) {
      memset(stereoSamples, 0, sizeof(stereoSamples));
      size_t written = 0;
      i2s_write(I2S_NUM_0, stereoSamples, sizeof(stereoSamples), &written, portMAX_DELAY);
      return;
    }
    playing = true;
  }

  if (playing && ringCount < AUDIO_BLOCK_BYTES / 2 && millis() - lastAudioMs > 700UL) {
    playing = false;
  }

  double sum = 0.0;
  for (uint16_t i = 0; i < AUDIO_FRAMES; i++) {
    uint8_t lo = 0;
    uint8_t hi = 0;
    int16_t sample;
    if (ringPopByte(lo) && ringPopByte(hi)) {
      sample = (int16_t)((uint16_t)lo | ((uint16_t)hi << 8));
      sample = (int16_t)((int32_t)sample / 3);
      lastSample = sample;
      playedBytes += 2;
    } else {
      lastSample = (int16_t)((int32_t)lastSample * 7 / 8);
      sample = lastSample;
    }
    stereoSamples[i * 2] = sample;
    stereoSamples[i * 2 + 1] = sample;
    sum += (double)sample * (double)sample;
  }

  size_t written = 0;
  i2s_write(I2S_NUM_0, stereoSamples, sizeof(stereoSamples), &written, portMAX_DELAY);

  float rms = sqrt(sum / AUDIO_FRAMES);
  float level = rms / 3600.0f;
  if (level > 1.0f) level = 1.0f;
  levelSmooth = levelSmooth * 0.84f + level * 0.16f;
}

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  drawStatus("WIFI AUDIO", "Connecting...", WIFI_SSID);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 12000UL) {
    delay(250);
  }

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("OwiAudio", "12345678");
    drawStatus("WIFI AP", "SSID OwiAudio", "IP 192.168.4.1");
    Serial.println("AP mode: OwiAudio / 12345678");
  } else {
    char ip[22];
    snprintf(ip, sizeof(ip), "%s", WiFi.localIP().toString().c_str());
    drawStatus("WIFI AUDIO", ip, "Port 7777");
    Serial.print("WiFi IP: ");
    Serial.println(ip);
  }
}

void micTask(void *pvParameters) {
  uint8_t micBuf[1024];
  size_t bytesRead;
  while(true) {
    if (i2s_read(I2S_NUM_0, micBuf, sizeof(micBuf), &bytesRead, portMAX_DELAY) == ESP_OK) {
      if (bytesRead > 0) {
        // Broadcast mic audio to dashboard
        micUdp.beginPacket(IPAddress(255,255,255,255), 7799);
        micUdp.write(micBuf, bytesRead);
        micUdp.endPacket();

        // --- Compute RMS of mic input for face expressions ---
        // INMP441 is 32-bit I2S, upper 24 bits used. Shift right 8.
        int32_t *samples32 = (int32_t*)micBuf;
        uint32_t count = bytesRead / 4;
        double sum = 0.0;
        int32_t peak = 0;
        for (uint32_t i = 0; i < count; i++) {
          int32_t s = samples32[i] >> 8;  // 24-bit value
          sum += (double)s * (double)s;
          if (abs(s) > peak) peak = abs(s);
        }
        float rms = (count > 0) ? sqrtf((float)(sum / count)) : 0.0f;
        // Normalize: INMP441 typical max ~800000 at loud speech
        float level = rms / 600000.0f;
        if (level > 1.0f) level = 1.0f;
        float peakLevel = (float)peak / 800000.0f;
        if (peakLevel > 1.0f) peakLevel = 1.0f;

        micSmooth = micSmooth * 0.80f + level * 0.20f;
        micPeak   = micPeak   * 0.90f + peakLevel * 0.10f;

        unsigned long now = millis();
        if (micSmooth > 0.04f) micActiveUntilMs = now + 600;
        if (micSmooth > 0.35f) micShoutUntilMs  = now + 800;
      }
    }
    vTaskDelay(1);
  }
}

void setup() {
  Serial.begin(115200);
  delay(700);
  pinMode(TOUCH_PIN, INPUT);
  Wire.begin(D4, D5);
  oledReady = display.begin(OLED_ADDR, true);
  if (oledReady) {
    display.clearDisplay();
    display.display();
  }
  setupMPU();
  setupDHT();

  if (!setupI2S()) {
    drawStatus("I2S ERR", "MAX D0 D8 D7", "SD wajib 3V3");
    while (true) delay(1000);
  }

  setupWiFi();
  telemetryUdp.begin(TELEMETRY_PORT);
  micUdp.begin(7799);
  audioServer.begin();
  audioServer.setNoDelay(true);
  Serial.println("TCP audio server on port 7777");
  xTaskCreatePinnedToCore(micTask, "MicTask", 4096, NULL, 1, NULL, 0);
}

void handleSerial() {
  while (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0) continue;
    unsigned long now = millis();
    if (cmd.startsWith("J")) {
      sscanf(cmd.c_str(), "J%d,%d", &manualJoyX, &manualJoyY);
      lastJoyMs = now;
    } else if (cmd.startsWith("M")) {
      sscanf(cmd.c_str(), "M%d", &manualMood);
      manualMoodMs = now;
    } else if (cmd.startsWith("R")) {
      strncpy(globalReminderText, cmd.c_str() + 1, sizeof(globalReminderText) - 1);
      globalReminderText[sizeof(globalReminderText) - 1] = '\0';
    } else if (cmd == "G") {
      currentState = STATE_GAMES;
      initPingpong();
      Serial.println("cmd: Start Pingpong");
    } else if (cmd == "A") {
      // Ignore attack in pingpong (handled by MPU tilt)
      Serial.println("cmd: Player Attack ignored in Pingpong");
    } else if (cmd == "P") {
      if (currentState == STATE_FACE) {
        currentState = STATE_MENU;
        menuCursor = 0;
      } else if (currentState == STATE_MENU) {
        menuCursor = (menuCursor + 1) % 5;
      } else if (currentState == STATE_GAMES) {
        if (gamePhase == GAME_IDLE || gamePhase == GAME_OVER) {
          initPingpong(true);
        } else if (gamePhase == GAME_PLAYING && ballX < 64.0f) {
          ballVY += (float)random(-10, 11) * 0.1f;
        }
      } else {
        currentState = STATE_MENU;
        menuCursor = 0;
      }
      Serial.println("cmd: Tap Owi -> Menu/Next");
    } else if (cmd == "O") {
      // Simulate Hold
      if (currentState == STATE_MENU) {
        if (menuCursor == 0) currentState = STATE_GAMES;
        else if (menuCursor == 1) currentState = STATE_SENSOR;
        else if (menuCursor == 2) currentState = STATE_REMINDER;
        else if (menuCursor == 3) {
          currentState = STATE_FACE;
          req_lovestory = true;
        }
        else if (menuCursor == 4) currentState = STATE_FACE;
        if (currentState == STATE_GAMES) gamePhase = GAME_IDLE;
      } else if (currentState == STATE_GAMES) {
        currentState = STATE_FACE;
        gamePhase = GAME_IDLE;
      } else if (currentState == STATE_FACE) {
        touchSleepyUntilMs = now + 2600UL;
      }
      Serial.println("cmd: Hold Owi -> OK");
    } else if (cmd == "D") {
      laughUntilMs = now + 2500UL;
      currentState = STATE_FACE;
      Serial.println("cmd: Double Click -> Laugh");
    } else if (cmd == "E") {
      touchLoveUntilMs = now + 2000UL;
      Serial.println("cmd: Pet Owi -> Love");
    } else if (cmd == "F") {
      glitchUntilMs = now + 1500UL;
      Serial.println("cmd: Flip Face -> Glitch");
    } else if (cmd == "C") {
      currentState = STATE_FACE;
      gamePhase = GAME_IDLE;
      Serial.println("cmd: Back to Face");
    } else if (cmd.startsWith("K")) {
      // Web paddle: K-100..K100
      int val = cmd.substring(1).toInt();
      manualJoyY = constrain(val, -100, 100);
      lastJoyMs = now;
    }
  }
}

void loop() {
  handleSerial();
  updateTouch();
  updateMPU();
  updateDHT();
  updateGame();
  sendTelemetry();
  acceptClient();
  readClientToRing();
  writeAudioBlock();
  readClientToRing();

  unsigned long now = millis();
  unsigned long drawInterval = (currentState == STATE_GAMES) ? 25UL : (playing ? 40UL : 75UL);
  if (playing != wasPlaying || now - lastDrawMs > drawInterval) {
    wasPlaying = playing;
    lastDrawMs = now;
    if (currentState == STATE_FACE) {
      drawMochi(playing);
    } else {
      drawUI();
    }
  }
}
