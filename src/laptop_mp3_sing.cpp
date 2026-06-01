#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "driver/i2s.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C

#define OLED_WHITE SH110X_WHITE
#define OLED_BLACK SH110X_BLACK

#define MAX_BCLK_PIN D0
#define MAX_LRC_PIN D8
#define MAX_DIN_PIN D7

const uint32_t SERIAL_BAUD = 460800;
const uint32_t AUDIO_RATE = 16000;
const uint16_t SERIAL_READ_BYTES = 256;
const uint16_t AUDIO_FRAMES = 128;
const uint16_t AUDIO_BYTES_PER_BLOCK = AUDIO_FRAMES * 2;
const uint16_t AUDIO_RING_SIZE = 8192;
const uint16_t AUDIO_PREBUFFER_BYTES = 2048;

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool oledReady = false;
unsigned long lastAudioMs = 0;
unsigned long lastDrawMs = 0;
unsigned long lastIdleDrawMs = 0;
uint32_t audioBytes = 0;
float mouthLevel = 0.0f;
float smoothLevel = 0.0f;
bool wasPlaying = false;
bool audioBuffered = false;
int16_t lastSample = 0;

uint8_t serialBytes[SERIAL_READ_BYTES];
uint8_t audioRing[AUDIO_RING_SIZE];
uint16_t ringHead = 0;
uint16_t ringTail = 0;
uint16_t ringCount = 0;
int16_t stereoSamples[AUDIO_FRAMES * 2];

bool setupI2S() {
  i2s_config_t config = {};
  config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  config.sample_rate = AUDIO_RATE;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 8;
  config.dma_buf_len = 256;
  config.use_apll = false;
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

void writeSilence(uint16_t ms) {
  int16_t silence[256 * 2] = {};
  uint32_t loops = ms / 12 + 1;
  for (uint32_t i = 0; i < loops; i++) {
    size_t written = 0;
    i2s_write(I2S_NUM_0, silence, sizeof(silence), &written, portMAX_DELAY);
  }
}

uint16_t ringFree() {
  return AUDIO_RING_SIZE - ringCount;
}

void ringPush(const uint8_t* data, uint16_t len) {
  for (uint16_t i = 0; i < len; i++) {
    if (ringCount >= AUDIO_RING_SIZE) {
      ringTail = (ringTail + 1) % AUDIO_RING_SIZE;
      ringCount--;
    }
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

void drawPixelHeart(int x, int y) {
  display.drawPixel(x - 1, y, OLED_WHITE);
  display.drawPixel(x + 1, y, OLED_WHITE);
  display.drawPixel(x - 2, y + 1, OLED_WHITE);
  display.drawPixel(x + 2, y + 1, OLED_WHITE);
  display.drawPixel(x - 1, y + 2, OLED_WHITE);
  display.drawPixel(x + 1, y + 2, OLED_WHITE);
  display.drawPixel(x, y + 3, OLED_WHITE);
}

void drawMusicNote(int x, int y, bool tall) {
  display.drawFastVLine(x, y, tall ? 10 : 8, OLED_WHITE);
  display.drawFastHLine(x, y, 5, OLED_WHITE);
  display.fillCircle(x - 2, y + (tall ? 10 : 8), 2, OLED_WHITE);
}

void drawSoftSmile(int cx, int y, int w) {
  display.drawFastHLine(cx - w / 2 + 2, y, w - 4, OLED_WHITE);
  display.drawPixel(cx - w / 2 + 1, y - 1, OLED_WHITE);
  display.drawPixel(cx + w / 2 - 1, y - 1, OLED_WHITE);
  display.drawPixel(cx - w / 2, y - 2, OLED_WHITE);
  display.drawPixel(cx + w / 2, y - 2, OLED_WHITE);
}

void drawHappyEye(int x, int y, bool flip) {
  int dir = flip ? -1 : 1;
  display.drawFastHLine(x - 6, y + 3, 12, OLED_WHITE);
  display.drawPixel(x - 7, y + 2, OLED_WHITE);
  display.drawPixel(x + 7, y + 2, OLED_WHITE);
  display.drawPixel(x - 8, y + 1 + dir, OLED_WHITE);
  display.drawPixel(x + 8, y + 1 - dir, OLED_WHITE);
}

void drawTinyBlush(int x, int y) {
  display.drawFastHLine(x, y, 5, OLED_WHITE);
  display.drawFastHLine(x + 1, y + 2, 4, OLED_WHITE);
}

void drawTinySparkle(int x, int y) {
  display.drawPixel(x, y, OLED_WHITE);
  display.drawPixel(x - 1, y, OLED_WHITE);
  display.drawPixel(x + 1, y, OLED_WHITE);
  display.drawPixel(x, y - 1, OLED_WHITE);
  display.drawPixel(x, y + 1, OLED_WHITE);
}

void drawSurpriseEye(int x, int y) {
  display.fillCircle(x, y + 9, 8, OLED_WHITE);
  display.fillCircle(x, y + 9, 3, OLED_BLACK);
}

void drawSleepyEye(int x, int y) {
  display.drawFastHLine(x - 8, y + 9, 16, OLED_WHITE);
  display.drawPixel(x - 9, y + 8, OLED_WHITE);
  display.drawPixel(x + 9, y + 8, OLED_WHITE);
}

void drawCapsuleEye(int x, int y, int w, int h, bool blink, bool sparkle) {
  if (blink) {
    display.fillRoundRect(x - w / 2, y + h / 2 - 2, w, 4, 2, OLED_WHITE);
    return;
  }
  display.fillRoundRect(x - w / 2, y, w, h, w / 2, OLED_WHITE);
  if (sparkle && h > 12) {
    display.drawPixel(x - w / 2 + 4, y + 4, OLED_BLACK);
    display.drawPixel(x - w / 2 + 5, y + 4, OLED_BLACK);
  }
}

void drawMochiMouth(int cx, int y, int level) {
  if (level <= 0) {
    drawSoftSmile(cx, y, 24);
    return;
  }
  if (level == 3) {
    display.fillCircle(cx, y, 6, OLED_WHITE);
    display.fillCircle(cx, y, 2, OLED_BLACK);
    return;
  }
  if (level == 1) {
    display.fillRoundRect(cx - 8, y - 3, 16, 7, 4, OLED_WHITE);
    return;
  }
  display.fillRoundRect(cx - 7, y - 7, 14, 15, 7, OLED_WHITE);
  display.fillRoundRect(cx - 3, y - 2, 6, 7, 3, OLED_BLACK);
}

void drawDasaiMochiSinger(bool playing) {
  if (!oledReady) return;

  unsigned long now = millis();
  float idleBreath = sinf(now * 0.0020f);
  int bob = playing ? (int)(sinf(now * 0.008f) * 2.0f) : (int)(idleBreath * 2.0f);
  int open = playing ? 2 + (int)(mouthLevel * 13.0f) : 2;
  if (open > 16) open = 16;
  unsigned long idlePhase = (now / 2400UL) % 4UL;
  int idleLook = 0;
  if (idlePhase == 1) idleLook = 4;
  if (idlePhase == 3) idleLook = -4;
  int sway = playing ? (int)(sinf(now * 0.004f) * 3.0f) : idleLook;

  display.clearDisplay();

  int cx = 64 + sway;
  int eyeY = 15 + bob;
  bool idleBlink = !playing && ((now + 450UL) % 4200UL) < 150UL;
  bool playingBlink = playing && ((now / 180UL) % 16UL == 0);
  bool blink = idleBlink || playingBlink;
  bool happyPeak = playing && smoothLevel > 0.48f;
  uint8_t idleExpr = playing ? 0 : (uint8_t)((now / 5200UL) % 6UL);
  bool idleHappy = !playing && (idleExpr == 1 || idleExpr == 4);
  bool idleSurprise = !playing && idleExpr == 2 && ((now / 650UL) % 6UL) < 2UL;
  bool idleSleepy = !playing && idleExpr == 3;
  bool idleLove = !playing && idleExpr == 5;
  bool singingPeak = playing && smoothLevel > 0.70f;
  if ((happyPeak || idleHappy || idleLove) && !blink) {
    drawHappyEye(cx - 24, eyeY + 8, false);
    drawHappyEye(cx + 24, eyeY + 8, true);
  } else if (idleSurprise && !blink) {
    drawSurpriseEye(cx - 24, eyeY);
    drawSurpriseEye(cx + 24, eyeY);
  } else if (idleSleepy && !blink) {
    drawSleepyEye(cx - 24, eyeY);
    drawSleepyEye(cx + 24, eyeY);
  } else {
    int eyeH = blink ? 4 : (playing ? 20 : 18 + (int)(idleBreath * 2.0f));
    int eyeW = playing ? 16 : 17;
    if (!playing && idleExpr == 0) eyeW = 15;
    drawCapsuleEye(cx - 24, eyeY + (20 - eyeH) / 2, eyeW, eyeH, blink, true);
    drawCapsuleEye(cx + 24, eyeY + (20 - eyeH) / 2, eyeW, eyeH, blink, true);
  }

  int mx = cx;
  int my = 47 + bob / 2;
  if (playing) {
    if (mouthLevel < 0.16f) {
      drawMochiMouth(mx, my, 0);
    } else if (mouthLevel < 0.50f) {
      drawMochiMouth(mx, my, 1);
    } else {
      drawMochiMouth(mx, my, 2);
    }
  } else {
    int smileW = idleHappy ? 28 : 22 + (int)(idleBreath * 2.0f);
    if (idleSurprise) {
      drawMochiMouth(mx, my, 3);
    } else if (idleSleepy) {
      display.drawFastHLine(mx - 8, my, 16, OLED_WHITE);
    } else {
      drawSoftSmile(mx, my, smileW);
    }
    if (((now / 2600UL) % 5UL) == 1UL) {
      display.drawPixel(cx + 38, 24, OLED_WHITE);
      display.drawPixel(cx + 39, 24, OLED_WHITE);
      display.drawPixel(cx + 38, 25, OLED_WHITE);
    }
  }

  if ((playing && smoothLevel > 0.34f) || idleHappy || idleLove) {
    drawTinyBlush(cx - 48, 42 + bob / 2);
    drawTinyBlush(cx + 42, 42 + bob / 2);
  }

  if (idleLove) {
    drawPixelHeart(cx, 9 + (now / 420UL) % 3UL);
  }

  if (!playing && idleExpr == 4) {
    drawTinySparkle(cx + 38, 17);
  }

  if (playing && singingPeak) {
    drawTinySparkle(cx - 42, 15 + (now / 260UL) % 3UL);
    drawTinySparkle(cx + 42, 15 + ((now / 310UL) % 3UL));
  }

  if (playing && smoothLevel > 0.62f && ((now / 700UL) % 2UL == 0)) {
    drawMusicNote(13, 12 + (now / 360UL) % 3UL, false);
  }

  display.display();
}

void drainSerialToRing() {
  while (Serial.available() > 0 && ringFree() > 0) {
    int toRead = Serial.available();
    if (toRead > SERIAL_READ_BYTES) toRead = SERIAL_READ_BYTES;
    if (toRead > ringFree()) toRead = ringFree();
    if (toRead <= 0) return;

    int got = Serial.readBytes(serialBytes, toRead);
    if (got <= 0) return;
    ringPush(serialBytes, got);
    lastAudioMs = millis();
    audioBytes += got;
  }
}

void playBufferedAudioBlock() {
  if (!audioBuffered) {
    if (ringCount < AUDIO_PREBUFFER_BYTES) {
      memset(stereoSamples, 0, sizeof(stereoSamples));
      size_t written = 0;
      i2s_write(I2S_NUM_0, stereoSamples, sizeof(stereoSamples), &written, portMAX_DELAY);
      return;
    }
    audioBuffered = true;
  }

  if (audioBuffered && ringCount < AUDIO_BYTES_PER_BLOCK / 2 && millis() - lastAudioMs > 500UL) {
    audioBuffered = false;
  }

  double sum = 0.0;
  int16_t peak = 0;
  for (int i = 0; i < AUDIO_FRAMES; i++) {
    uint8_t lo = 0;
    uint8_t hi = 0;
    int16_t sample = 0;
    if (ringPopByte(lo) && ringPopByte(hi)) {
      sample = (int16_t)((uint16_t)lo | ((uint16_t)hi << 8));
      sample = (int16_t)((int32_t)sample / 8);
      lastSample = sample;
    } else {
      lastSample = (int16_t)((int32_t)lastSample * 7 / 8);
      sample = lastSample;
    }
    stereoSamples[i * 2] = sample;
    stereoSamples[i * 2 + 1] = sample;
    int16_t a = sample < 0 ? -sample : sample;
    if (a > peak) peak = a;
    sum += (double)sample * (double)sample;
  }

  size_t written = 0;
  i2s_write(I2S_NUM_0, stereoSamples, sizeof(stereoSamples), &written, portMAX_DELAY);

  float rms = sqrt(sum / max(1, (int)AUDIO_FRAMES));
  float level = rms / 3600.0f;
  if (peak > 9000) level = max(level, 0.68f);
  if (level > 1.0f) level = 1.0f;
  mouthLevel = mouthLevel * 0.55f + level * 0.45f;
  smoothLevel = smoothLevel * 0.82f + level * 0.18f;
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial.setTimeout(8);
  delay(800);
  Serial.println("Laptop MP3 -> MAX98357 + singing OLED");
  Serial.println("Node sends PCM 16000Hz mono s16le over USB serial");

  Wire.begin(D4, D5);
  oledReady = display.begin(OLED_ADDR, true);
  if (oledReady) {
    display.clearDisplay();
    display.display();
  }

  if (!setupI2S()) {
    if (oledReady) {
      display.clearDisplay();
      display.setTextColor(OLED_WHITE);
      display.setCursor(0, 20);
      display.print("I2S ERR");
      display.display();
    }
    while (true) delay(1000);
  }

  lastAudioMs = millis();
  drawDasaiMochiSinger(false);
}

void loop() {
  drainSerialToRing();
  playBufferedAudioBlock();
  drainSerialToRing();

  unsigned long now = millis();
  bool playing = audioBuffered || now - lastAudioMs < 450UL;
  if (playing != wasPlaying) {
    wasPlaying = playing;
    drawDasaiMochiSinger(playing);
  }

  if (!playing) {
    mouthLevel *= 0.82f;
    smoothLevel *= 0.92f;
  }

  if (playing && now - lastDrawMs > 140UL) {
    lastDrawMs = now;
    drawDasaiMochiSinger(playing);
  }

  if (!playing && now - lastIdleDrawMs > 700UL) {
    lastIdleDrawMs = now;
    drawDasaiMochiSinger(false);
  }
}
