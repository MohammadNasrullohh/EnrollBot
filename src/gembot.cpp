#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <math.h>
#include <DHT.h>

#include <WiFi.h>
#include <WebSocketsClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "freertos/ringbuf.h"
#include "secrets.h"

WebSocketsClient webSocket;
HTTPClient audioHttp;
WiFiClient* audioStream = nullptr;

bool voiceRecording = false;
bool isDrawMode = false;

enum AppState {
  APP_FACE,
  APP_MENU,
  APP_PINGPONG,
  APP_SUHU,
  APP_PENGINGAT,
  APP_DRAW,
  APP_MUSIK
};
AppState currentState = APP_FACE;

unsigned long voiceStartedMs = 0;
int voiceSamplesSent = 0;
uint8_t voicePacket[1024];
int voicePacketBytes = 0;

TFT_eSPI display = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&display);
float voiceLevel = 0.0f;

RingbufHandle_t audioRingBuf = NULL;
const size_t RING_BUF_SIZE = 49152;

void playBeep(float freq, int duration_ms);

#define I2S_BCLK 26
#define I2S_LRC 25
#define I2S_DOUT 27

// INMP441 I2S Microphone Pins
#define I2S_MIC_BCLK 32
#define I2S_MIC_LRC 33
#define I2S_MIC_DIN 34

// Touch Sensor Pin
#define TOUCH_PIN 13
bool lastTouchState = false;

extern unsigned long nodUntilMs;
extern unsigned long curiousUntilMs;
void handleTouchAction(bool isHold);
unsigned long touchStartTime = 0;
bool touchHandled = false;

// DHT22 Pin
#define DHTPIN 14
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
float currentSuhu = 0.0f;
float currentLembap = 0.0f;
unsigned long lastDhtRead = 0;

// Pingpong game variables
float ballX = 120.0f, ballY = 160.0f;
float ballVX = 3.0f, ballVY = -4.0f;
float paddleX = 120.0f;
int score = 0;
bool gameOver = false;

float currentToneFreq = 0.0f;
unsigned long toneEndMs = 0;
uint audioBytesRead = 0;
unsigned long lastAudioPlayedMs = 0;

int currentExpressionId = -1;

void audioTask(void *pvParameters) {
  int16_t buffer[256];
  size_t bytes_written;
  static int tone_i = 0;
  static float lastToneFreq = 0;
  
  while (true) {
    if (millis() < toneEndMs && currentToneFreq > 0) {
      if (currentToneFreq != lastToneFreq) {
        tone_i = 0;
        lastToneFreq = currentToneFreq;
      }
      for (int k = 0; k < 128; k+=2) {
        int16_t sample = (int16_t)(sin(2 * PI * currentToneFreq * tone_i / 16000.0) * 8000);
        buffer[k] = sample;
        buffer[k+1] = sample;
        tone_i++;
      }
      i2s_write(I2S_NUM_0, buffer, 128 * sizeof(int16_t), &bytes_written, portMAX_DELAY);
    } else {
       lastToneFreq = 0;
       size_t item_size = 0;
       void *data = xRingbufferReceive(audioRingBuf, &item_size, pdMS_TO_TICKS(30));
       if (data != NULL) {
           int16_t *mono = (int16_t*)data;
           int samples = item_size / 2;
           int k = 0;
           for (int i=0; i<samples; i++) {
               buffer[k++] = mono[i]; // left
               buffer[k++] = mono[i]; // right
               if (k >= 256) {
                   i2s_write(I2S_NUM_0, buffer, k * sizeof(int16_t), &bytes_written, portMAX_DELAY);
                   k = 0;
               }
           }
           if (k > 0) {
               i2s_write(I2S_NUM_0, buffer, k * sizeof(int16_t), &bytes_written, portMAX_DELAY);
           }
           vRingbufferReturnItem(audioRingBuf, data);
           lastAudioPlayedMs = millis();
       } else {
           vTaskDelay(pdMS_TO_TICKS(5));
       }
    }
  }
}

void startAudioStream(String file, String vol) {
  audioHttp.end();
  audioStream = nullptr;
  String url = "http://" + String(WS_HOST) + ":" + String(WS_PORT) + "/stream?file=" + file + "&vol=" + vol;
  Serial.println("Streaming audio: " + url);
  audioHttp.begin(url);
  int httpCode = audioHttp.GET();
  if (httpCode == HTTP_CODE_OK) {
      audioStream = audioHttp.getStreamPtr();
      xRingbufferReceiveUpTo(audioRingBuf, NULL, 0, 0); // clear ringbuffer
  } else {
      audioHttp.end();
  }
}

void readAudioStream() {
  if (!audioStream) return;
  if (!audioStream->connected()) {
      audioHttp.end();
      audioStream = nullptr;
      return;
  }
  int avail = audioStream->available();
  if (avail > 0) {
     uint8_t readBuffer[512];
     int freeSpace = xRingbufferGetCurFreeSize(audioRingBuf);
     int want = min(avail, 512);
     if (want > freeSpace) want = freeSpace;
     if (want > 0) {
         int got = audioStream->read(readBuffer, want);
         if (got > 0) {
             xRingbufferSend(audioRingBuf, readBuffer, got, pdMS_TO_TICKS(10));
         }
     }
  }
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_CONNECTED) {
    Serial.printf("[WS] Connected to url: %s\n", payload);
    webSocket.sendTXT("{\"type\":\"auth\",\"role\":\"owibot\"}");
  } else if (type == WStype_TEXT) {
    String text = (char*)payload;
    if (text.startsWith("AUDIO:")) {
       int firstColon = text.indexOf(':', 6);
       if (firstColon != -1) {
          String file = text.substring(6, firstColon);
          String vol = text.substring(firstColon + 1);
          startAudioStream(file, vol);
       } else {
          startAudioStream(text.substring(6), "0.50");
       }
    } else if (text == "VOICE:DONE") {
       // Server finished processing our voice
    } else if (text == "CMD:W") {
       isDrawMode = true;
    } else if (text.startsWith("CMD:M")) {
       isDrawMode = false;
       String idStr = text.substring(5);
       currentExpressionId = idStr.toInt();
    } else if (text == "CMD:C" || text == "CMD:CLEAR") {
       isDrawMode = false;
       currentExpressionId = -1;
    } else if (text == "CMD:P") {
       handleTouchAction(false);
    } else if (text == "CMD:O") {
       handleTouchAction(true);
    } else if (text == "CMD:G") {
       currentState = APP_PINGPONG;
       ballX = 120; ballY = 160; ballVX = 3; ballVY = -4; score = 0; gameOver = false;
    } else if (text == "CMD:D") {
       nodUntilMs = millis() + 1200;
       currentExpressionId = 1; // Senang
    } else if (text == "CMD:E") {
       curiousUntilMs = millis() + 1000;
       currentExpressionId = 24; // Delight
    } else if (text == "CMD:F") {
       static int rot = 1;
       rot = (rot == 1) ? 3 : 1;
       display.setRotation(rot);
    }
  } else if (type == WStype_BIN) {
      if (length == 9606 && memcmp(payload, "FRAME:", 6) == 0) {
          if (isDrawMode) {
              spr.drawBitmap(0, 0, payload + 6, 240, 320, TFT_WHITE, TFT_BLACK);
              spr.pushSprite(0, 0);
          }
      } else if (length > 0) {
          Serial.printf("[WS] Received BIN: %u bytes\n", length);
          // Raw PCM chunk from the server (Test Tone / TTS)
          xRingbufferSend(audioRingBuf, payload, length, pdMS_TO_TICKS(100));
      }
  }
}

void initI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // Back to stereo
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false, // APLL conflicts with TFT SPI clock!
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_set_clk(I2S_NUM_0, 16000, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);

  xTaskCreatePinnedToCore(audioTask, "AudioTask", 4096, NULL, 2, NULL, 1); // Higher priority, more stack
}

void micTask(void *pvParameters) {
  size_t bytes_read;
  int32_t samples[64];
  while (true) {
    esp_err_t res = i2s_read(I2S_NUM_1, &samples, sizeof(samples), &bytes_read, portMAX_DELAY);
    if (res != ESP_OK) {
      Serial.print("I2S read returned Error ");
      Serial.println(res);
      vTaskDelay(pdMS_TO_TICKS(100)); // Prevent watchdog reset if failing rapidly
      continue;
    }
    int samples_read = bytes_read / sizeof(int32_t);
    if (samples_read > 0) {
      float mean = 0;
      for (int i = 0; i < samples_read; i++) {
        // INMP441 is 24-bit aligned in a 32-bit slot, shift to get 16-bit
        int16_t sample = samples[i] >> 14; 
        mean += abs(sample);
      }
      mean /= samples_read;
      
      // If voice recording, stream to WebSocket
      if (voiceRecording) {
         for (int i = 0; i < samples_read; i++) {
           int16_t sample = samples[i] >> 14;
           voicePacket[voicePacketBytes++] = (uint8_t)(sample & 0xFF);
           voicePacket[voicePacketBytes++] = (uint8_t)((sample >> 8) & 0xFF);
           if (voicePacketBytes >= sizeof(voicePacket)) {
              if (webSocket.isConnected()) webSocket.sendBIN(voicePacket, voicePacketBytes);
              voicePacketBytes = 4;
              memcpy(voicePacket, "MIC:", 4);
           }
         }
      }
      
      // If a loud sound is detected (clap, shout), trigger surprised!
      if (mean > 2500 && !voiceRecording) {
        extern unsigned long surprisedUntilMs;
        if (surprisedUntilMs < millis()) {
            surprisedUntilMs = millis() + 900;
        }
      }
    }
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

void initMicI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // INMP441 requires 32-bit slots
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 128,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_MIC_BCLK,
    .ws_io_num = I2S_MIC_LRC,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_MIC_DIN
  };
  i2s_driver_install(I2S_NUM_1, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_1, &pin_config);

  xTaskCreatePinnedToCore(micTask, "MicTask", 2048, NULL, 1, NULL, 0); // Run on core 0 to distribute load
}

void playBeep(float freq, int duration_ms) {
  currentToneFreq = freq;
  toneEndMs = millis() + duration_ms;
}

// (TFT and sprite instantiated at top)

bool rawMpuMode = false;
uint8_t mpuAddr = 0x68;

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

int16_t be16(uint8_t h, uint8_t l) {
  return (int16_t)((h << 8) | l);
}

bool readRawMotion(uint8_t addr, float& ax, float& ay, float& az, float& gx, float& gy, float& gz) {
  Wire.beginTransmission(addr);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)addr, 14) != 14) return false;
  uint8_t b[14];
  for (uint8_t i = 0; i < 14; i++) b[i] = Wire.read();
  
  int16_t rax = be16(b[0], b[1]);
  int16_t ray = be16(b[2], b[3]);
  int16_t raz = be16(b[4], b[5]);
  int16_t rgx = be16(b[8], b[9]);
  int16_t rgy = be16(b[10], b[11]);
  int16_t rgz = be16(b[12], b[13]);

  ax = ((float)rax / 8192.0f) * 9.80665f;
  ay = ((float)ray / 8192.0f) * 9.80665f;
  az = ((float)raz / 8192.0f) * 9.80665f;
  gx = ((float)rgx / 65.5f) * 0.0174533f; // Convert deg/s to rad/s
  gy = ((float)rgy / 65.5f) * 0.0174533f;
  gz = ((float)rgz / 65.5f) * 0.0174533f;
  return true;
}

bool setupRawMPU() {
  for (uint8_t addr : { (uint8_t)0x68, (uint8_t)0x69 }) {
    bool ok = false;
    uint8_t who = readReg8(addr, 0x75, ok);
    if (ok && (who == 0x68 || who == 0x70 || who == 0x71)) {
      mpuAddr = addr;
      rawMpuMode = true;
      writeReg8(addr, 0x6B, 0x00);
      delay(50);
      writeReg8(addr, 0x1A, 0x03);
      return true;
    }
  }
  return false;
}

bool mpuReady = false;

// MPU Logic Variables
float currentJerk = 0.0f;
float currentRoll = 0.0f;
float currentPitch = 0.0f;
float currentYaw = 0.0f;

// Gesture States
bool nodDetected = false;
unsigned long nodUntilMs = 0;
bool headShakeDetected = false;
unsigned long headShakeUntilMs = 0;
bool surprisedMode = false;
unsigned long surprisedUntilMs = 0;
unsigned long curiousUntilMs = 0;

void handleTouchAction(bool isHold);
bool faceUpMode = false;
bool faceDownMode = false;

// History for gestures
float pitchHistory[4] = {0};
float yawHistory[4] = {0};
uint8_t histIdx = 0;
unsigned long lastHistMs = 0;
float sustainedTiltX = 0.0f;
unsigned long sustainedTiltStartMs = 0;

// Face Drawing Variables
float targetLookX = 0;
float targetLookY = 0;
float targetBob = 0;

float lookX = 0;
float lookY = 0;
float bobY = 0;

unsigned long lastMpuMs = 0;


void updateMPU() {
  if (!mpuReady) return;
  unsigned long now = millis();
  if (now - lastMpuMs < 20) return;
  lastMpuMs = now;

  float ax, ay, az, gx, gy, gz;
  if (!readRawMotion(mpuAddr, ax, ay, az, gx, gy, gz)) {
    setupRawMPU();
    return;
  }

  float jerk = sqrt(ax*ax + ay*ay + az*az);
  currentJerk = jerk;

  // Track History
  if (now - lastHistMs > 100) {
    lastHistMs = now;
    pitchHistory[histIdx] = ay;
    yawHistory[histIdx] = gz;
    histIdx = (histIdx + 1) % 4;
  }

  // Tilt/Look calculations

  // Surprise tap (Gravity is ~9.8, so >18 means a stronger tap)
  if (jerk > 18.0f) {
    surprisedUntilMs = now + 900;
  }

  // Curious tilt
  float tiltX = -ax;
  float tiltY = ay;
  if (fabs(tiltX) > 0.45f) {
    if (sustainedTiltStartMs == 0) sustainedTiltStartMs = now;
    else if (now - sustainedTiltStartMs > 700) { 
        curiousUntilMs = now + 500; 
    }
  } else {
    sustainedTiltStartMs = 0;
  }

  // Face Up/Down
  faceUpMode = (az < 5.0f);
  faceDownMode = (az > 14.0f);

  // Nod (pitch change)
  float pMax = pitchHistory[0], pMin = pitchHistory[0];
  for(int i=1;i<4;i++){ if(pitchHistory[i]>pMax) pMax=pitchHistory[i]; if(pitchHistory[i]<pMin) pMin=pitchHistory[i]; }
  if (pMax - pMin > 6.0f) { 
      nodUntilMs = now + 1200; 
  }

  // Shake (yaw change)
  float yMax = yawHistory[0], yMin = yawHistory[0];
  for(int i=1;i<4;i++){ if(yawHistory[i]>yMax) yMax=yawHistory[i]; if(yawHistory[i]<yMin) yMin=yawHistory[i]; }
  if (yMax - yMin > 4.0f) { 
      headShakeUntilMs = now + 1000; 
  }

  // Touch logic handled in loop now

  // Smooth Targets
  targetLookX = tiltX * 2.0f;
  targetLookY = tiltY * 2.0f;
}

int menuCursor = 0;
const int menuCount = 6;
const char* menuItems[] = {"Pingpong", "Suhu", "Pengingat", "Draw", "Musik", "Kembali"};

int musicCursor = 0;
const int musicCount = 4;
const char* musicItems[] = {"MBG", "Love Story", "Test Max", "Kembali"};

void handleTouchAction(bool isHold) {
  if (currentState == APP_FACE) {
    currentState = APP_MENU;
    menuCursor = 0;
    return;
  }
  if (currentState == APP_MENU) {
    if (!isHold) {
      menuCursor = (menuCursor + 1) % menuCount;
    } else {
      if (menuCursor == 0) {
        currentState = APP_PINGPONG;
        ballX = 120; ballY = 160; ballVX = 3; ballVY = -4; score = 0; gameOver = false;
      } else if (menuCursor == 1) currentState = APP_SUHU;
      else if (menuCursor == 2) currentState = APP_PENGINGAT;
      else if (menuCursor == 3) { isDrawMode = true; currentState = APP_DRAW; }
      else if (menuCursor == 4) currentState = APP_MUSIK;
      else if (menuCursor == 5) currentState = APP_FACE;
    }
    return;
  }
  if (currentState == APP_MUSIK) {
    if (!isHold) {
      musicCursor = (musicCursor + 1) % musicCount;
    } else {
      if (musicCursor == musicCount - 1) {
        currentState = APP_MENU;
      } else {
        String cmd = (musicCursor == 2) ? "CMD:TEST_MAX" : (String("CMD:PLAY:") + String(musicCursor + 1));
        webSocket.sendTXT(cmd);
        currentState = APP_FACE;
      }
    }
    return;
  }
  if (currentState == APP_PINGPONG && isHold && gameOver) {
    ballX = 120; ballY = 160; ballVX = 3; ballVY = -4; score = 0; gameOver = false;
    return;
  }
  if (isHold) {
    currentState = APP_FACE;
    isDrawMode = false;
  }
}

void handleTouch() {
  int touchVal = touchRead(TOUCH_PIN);
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 1000) {
    Serial.printf("Touch Value: %d\n", touchVal);
    lastPrint = millis();
  }
  bool currentTouch = (touchVal < 50);
  unsigned long now = millis();
  static unsigned long lastTouchEnd = 0;
  if (currentTouch && (now - lastTouchEnd < 300)) return;
  if (currentTouch) {
    if (touchStartTime == 0) {
      touchStartTime = now;
      touchHandled = false;
    } else if (!touchHandled && (now - touchStartTime > 180)) {
      touchHandled = true;
      handleTouchAction(false);
    } else if (!touchHandled && (now - touchStartTime > 700)) {
      touchHandled = true;
      handleTouchAction(true);
    }
  } else {
    if (touchStartTime > 0) {
      touchStartTime = 0;
      lastTouchEnd = now;
    }
  }
}
static const unsigned char PROGMEM image_Layer_7_bits[] = {0xc0,0x03,0xe0,0x07,0xf8,0x1f,0x7f,0xfe,0x3f,0xfc,0x0f,0xf0};
static const unsigned char PROGMEM image_Layer_8_bits[] = {0x3f,0xff,0xf8,0x00,0x7f,0xff,0xfe,0x00,0xff,0xff,0xff,0x00,0xff,0xff,0xff,0x80,0xff,0xff,0xff,0x80,0xff,0xff,0xff,0x80,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0x80,0xff,0xff,0xff,0x80,0xff,0xff,0xff,0x80,0xff,0xff,0xff,0x80,0xff,0xff,0xff,0x00,0x7f,0xff,0xff,0x00,0x7f,0xff,0xff,0x00,0x7f,0xff,0xfe,0x00,0x7f,0xff,0xfe,0x00,0x3f,0xff,0xfc,0x00,0x1f,0xff,0xf8,0x00,0x0f,0xff,0xe0,0x00,0x07,0xff,0x00,0x00};`r`nstatic const unsigned char PROGMEM image_Layer_9_bits[] = {0x07,0xff,0xff,0x00,0x1f,0xff,0xff,0x80,0x3f,0xff,0xff,0xc0,0x7f,0xff,0xff,0xc0,0x7f,0xff,0xff,0xc0,0x7f,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0xff,0xff,0xff,0xc0,0x7f,0xff,0xff,0xc0,0x7f,0xff,0xff,0xc0,0x7f,0xff,0xff,0xc0,0x7f,0xff,0xff,0xc0,0x3f,0xff,0xff,0xc0,0x3f,0xff,0xff,0x80,0x3f,0xff,0xff,0x80,0x1f,0xff,0xff,0x80,0x1f,0xff,0xff,0x80,0x0f,0xff,0xff,0x00,0x07,0xff,0xfe,0x00,0x01,0xff,0xfc,0x00,0x00,0x3f,0xf8,0x00};
void drawBitmapScaled(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color, float scaleX, float scaleY) {
  int16_t byteWidth = (w + 7) / 8;
  uint8_t byte = 0;
  for (int16_t j = 0; j < h; j++) {
    int16_t py = y + (int)(j * scaleY);
    int16_t py2 = y + (int)((j + 1) * scaleY);
    int16_t ph = py2 - py;
    if (ph <= 0) continue;
    for (int16_t i = 0; i < w; i++) {
      if (i & 7) byte <<= 1;
      else byte = pgm_read_byte(&bitmap[j * byteWidth + i / 8]);
      if (byte & 0x80) {
        int16_t px = x + (int)(i * scaleX);
        int16_t px2 = x + (int)((i + 1) * scaleX);
        int16_t pw = px2 - px;
        if (pw > 0) spr.fillRect(px, py, pw, ph, color);
      }
    }
  }
}

unsigned long lastBlinkMs = 0;
bool isBlinking = false;
float currentEyeScaleY = 2.5f;
float currentEyeScaleX = 2.5f;
float currentCuriousOffsetL = 0;
float currentCuriousOffsetR = 0;
float currentMouthOpen = 0;
float currentMouthTalk = 0;
unsigned long lastIdleMs = 0;
bool isSleeping = false;
float currentSleepAmount = 0;
float currentListenPulse = 0;

void drawFace() {
  // Existing drawMochi logic
  spr.fillSprite(0);
  unsigned long now = millis();

  bool nodding = now < nodUntilMs;
  bool headShaking = now < headShakeUntilMs;
  bool surprised = now < surprisedUntilMs;
  bool curious = now < curiousUntilMs;
  
  // Check if audio is playing (reduced from 300ms to 150ms to avoid stuck O mouth)
  bool isTalking = (now - lastAudioPlayedMs < 150);
  bool isListening = voiceRecording;

  // Sleep detection (idle for 30+ seconds with no movement)
  if (nodding || headShaking || surprised || curious || isTalking || isListening || currentExpressionId >= 0) {
    lastIdleMs = now;
    isSleeping = false;
  } else if (now - lastIdleMs > 30000) {
    isSleeping = true;
  }

  float actualTargetLookX = targetLookX;
  float actualTargetLookY = targetLookY;
  float actualTargetBob = 0;

  if (nodding) {
    actualTargetBob = -abs(sin(now * 0.015f)) * 8.0f;
  }
  if (headShaking) {
    actualTargetLookX += sin(now * 0.025f) * 10.0f;
  }
  if (faceUpMode) {
    actualTargetLookY -= 5.0f;
  }
  if (faceDownMode) {
    actualTargetLookY += 5.0f;
    actualTargetBob += 3.0f;
  }

  // Breathing animation (smooth idle)
  if (isSleeping) {
    actualTargetBob += sin(now * 0.0015f) * 5.0f; // Slower breathing
  } else {
    actualTargetBob += sin(now * 0.0025f) * 3.5f;
    actualTargetLookX += sin(now * 0.001f) * 1.5f;
  }

  // Blinking Logic
  if (!isSleeping) {
    if (now - lastBlinkMs > 3500) {
      if (!isBlinking && random(10) > 2) {
         isBlinking = true;
      }
      lastBlinkMs = now;
    }
  }

  float targetEyeScaleY = 2.5f;
  float targetEyeScaleX = 2.5f;
  float targetCuriousOffsetL = 0;
  float targetCuriousOffsetR = 0;
  float targetMouthOpen = 0;
  float targetMouthTalk = 0;
  float targetSleep = 0;
  float targetListenPulse = 0;
  int mouthStyle = 0; // 0=W, 1=U, 2=^, 3=_, 4=V, 5=~, 6=O

  if (currentExpressionId >= 0) {
      if (currentExpressionId == 0) { // Senyum
          targetEyeScaleY = 3.0f; targetEyeScaleX = 3.0f; mouthStyle = 0;
      } else if (currentExpressionId == 1) { // Senang
          targetEyeScaleY = 1.5f; targetEyeScaleX = 3.2f; mouthStyle = 1;
      } else if (currentExpressionId == 6) { // Sedih
          targetEyeScaleY = 2.0f; targetEyeScaleX = 3.0f; mouthStyle = 2;
      } else if (currentExpressionId == 3) { // Marah
          targetEyeScaleY = 1.8f; targetEyeScaleX = 3.5f; mouthStyle = 3; targetCuriousOffsetL = -5.0f; targetCuriousOffsetR = 5.0f;
      } else if (currentExpressionId == 4) { // Kaget
          targetEyeScaleY = 4.2f; targetEyeScaleX = 3.8f; mouthStyle = 6; targetMouthOpen = 1.0f;
      } else if (currentExpressionId == 5) { // Ngantuk
          targetEyeScaleY = 0.5f; targetEyeScaleX = 3.5f; mouthStyle = 3;
      } else if (currentExpressionId == 24) { // Delight
          targetEyeScaleY = 2.0f; targetEyeScaleX = 3.0f; mouthStyle = 4;
      } else if (currentExpressionId == 25) { // Guilty
          targetEyeScaleY = 1.5f; targetEyeScaleX = 3.5f; mouthStyle = 5;
      } else if (currentExpressionId == 26) { // Daydream
          targetEyeScaleY = 2.0f; targetEyeScaleX = 3.0f; mouthStyle = 0; targetCuriousOffsetL = -15.0f; targetCuriousOffsetR = -15.0f;
      } else if (currentExpressionId == 27) { // Grumpy
          targetEyeScaleY = 1.5f; targetEyeScaleX = 3.0f; mouthStyle = 2; targetCuriousOffsetL = 5.0f; targetCuriousOffsetR = 5.0f;
      } else if (currentExpressionId == 28) { // Amazed
          targetEyeScaleY = 4.0f; targetEyeScaleX = 3.5f; mouthStyle = 4;
      } else if (currentExpressionId == 29) { // Nangis
          targetEyeScaleY = 2.0f; targetEyeScaleX = 3.0f; mouthStyle = 2; targetCuriousOffsetL = 5.0f; targetCuriousOffsetR = 5.0f;
      } else if (currentExpressionId == 30) { // Pusing
          targetEyeScaleY = 3.5f; targetEyeScaleX = 3.5f; mouthStyle = 5; actualTargetBob += sin(now * 0.01f) * 10.0f;
      } else if (currentExpressionId == 31) { // Nakal
          targetEyeScaleY = 3.0f; targetEyeScaleX = 3.0f; mouthStyle = 3;
      }
  } else {
      if (curious && actualTargetLookX <= 0) targetCuriousOffsetL = -18.0f;
      if (curious && actualTargetLookX > 0) targetCuriousOffsetR = -18.0f;
      
      if (surprised) {
          targetEyeScaleY = 4.2f; targetEyeScaleX = 3.8f; targetMouthOpen = 1.0f; mouthStyle = 6;
      } else if (nodding) {
          targetEyeScaleY = 1.5f; targetEyeScaleX = 3.2f; mouthStyle = 1;
      } else if (isSleeping) {
          targetSleep = 1.0f; targetEyeScaleY = 0.3f; targetEyeScaleX = 3.5f; mouthStyle = 3;
      }
  }

  // Blinking overrides
  if (isBlinking && !isSleeping && currentExpressionId != 5) {
    unsigned long blinkElapsed = now - lastBlinkMs;
    if (blinkElapsed < 80) {
       targetEyeScaleY = 0.1f;
    } else if (blinkElapsed < 160) {
       // returns to normal slowly via lerp below
    } else {
       isBlinking = false;
       lastBlinkMs = now;
    }
  }
  
  if (isListening) {
    targetEyeScaleY = 3.8f;
    targetEyeScaleX = 3.5f;
  }

  // Idle micro-expressions (smooth transitions when nothing is happening)
  if (currentExpressionId < 0 && !surprised && !nodding && !curious && !headShaking && !isTalking && !isListening && !isSleeping) {
      unsigned long idleCycle = (now / 8000) % 5; // Change every 8 seconds
      if (idleCycle == 0) {
          // Default smile (W mouth)
          mouthStyle = 0;
      } else if (idleCycle == 1) {
          // Happy squint
          targetEyeScaleY = 1.8f;
          mouthStyle = 1; // U mouth
      } else if (idleCycle == 2) {
          // Curious look sideways
          actualTargetLookX += sin(now * 0.0008f) * 6.0f;
          mouthStyle = 0;
      } else if (idleCycle == 3) {
          // Content/relaxed
          targetEyeScaleY = 2.5f;
          mouthStyle = 3; // Flat mouth
      } else {
          // Slight smirk
          mouthStyle = 4; // V mouth
          targetEyeScaleY = 2.2f;
      }
  }

  // Talking expression (mouth flaps) - only for recent audio
  if (isTalking) {
    float talkAmount = sin(now * 0.015f) * 0.4f + 0.4f;
    targetMouthTalk = talkAmount;
    targetEyeScaleY = 2.8f;
    mouthStyle = 1; // Use U mouth for talking instead of O
  }

  // Smooth lerp all animations (lower = smoother)
  float eyeLerp = 0.08f;
  float posLerp = (nodding || headShaking || surprised || curious) ? 0.12f : 0.04f;
  
  currentEyeScaleY += (targetEyeScaleY - currentEyeScaleY) * eyeLerp;
  currentEyeScaleX += (targetEyeScaleX - currentEyeScaleX) * eyeLerp;
  currentCuriousOffsetL += (targetCuriousOffsetL - currentCuriousOffsetL) * 0.08f;
  currentCuriousOffsetR += (targetCuriousOffsetR - currentCuriousOffsetR) * 0.08f;
  currentMouthOpen += (targetMouthOpen - currentMouthOpen) * 0.10f;
  currentMouthTalk += (targetMouthTalk - currentMouthTalk) * 0.15f;
  currentSleepAmount += (targetSleep - currentSleepAmount) * 0.04f;
  currentListenPulse += (targetListenPulse - currentListenPulse) * 0.10f;

  lookX = lookX + (actualTargetLookX - lookX) * posLerp;
  lookY = lookY + (actualTargetLookY - lookY) * posLerp;
  bobY = bobY + (actualTargetBob - bobY) * posLerp;

  int cx = 120 + (int)lookX;
  int cy = 160 + (int)lookY + (int)bobY;

  int leX = cx - (int)(36 * currentEyeScaleX);
  int reX = cx + (int)(12 * currentEyeScaleX);
  int eyeY = cy - 57;
  int mouthX = cx - 21;
  int mouthY = cy + 48;

  int eyeOffsetYL = (111 - (int)(37 * currentEyeScaleY)) / 2;
  int eyeOffsetYR = eyeOffsetYL;
  
  // Asymmetric eyes for Nakal
  if (currentExpressionId == 31) {
     eyeOffsetYR = (111 - (int)(37 * 0.2f)) / 2;
  }

  // Draw Eyes
  drawBitmapScaled(leX, eyeY + eyeOffsetYL + (int)currentCuriousOffsetL, image_Layer_9_bits, 26, 37, TFT_WHITE, currentEyeScaleX, currentExpressionId == 31 ? currentEyeScaleY : currentEyeScaleY);
  drawBitmapScaled(reX, eyeY + eyeOffsetYR + (int)currentCuriousOffsetR, image_Layer_8_bits, 26, 37, TFT_WHITE, currentEyeScaleX, currentExpressionId == 31 ? 0.2f : currentEyeScaleY);

  // Draw Mouth
  float totalMouth = max(currentMouthOpen, currentMouthTalk);
  if (mouthStyle == 6 || totalMouth > 0.05f) {
    int maxR = 18;
    int r1 = max(4, (int)(maxR * max(0.2f, totalMouth)));
    int r2 = max(1, (int)((maxR - 6) * max(0.2f, totalMouth)));
    spr.fillCircle(mouthX + 24, mouthY + 9, r1, TFT_WHITE);
    spr.fillCircle(mouthX + 24, mouthY + 9, r2, TFT_BLACK);
  } else if (mouthStyle == 0) {
    // W shape
    float smileScaleY = 3.0f;
    drawBitmapScaled(mouthX, mouthY, image_Layer_7_bits, 16, 6, TFT_WHITE, 3.0f, smileScaleY);
  } else if (mouthStyle == 1) {
    // U shape (Senang)
    spr.fillCircle(mouthX + 24, mouthY + 5, 15, TFT_WHITE);
    spr.fillCircle(mouthX + 24, mouthY - 2, 18, TFT_BLACK); // cut top
  } else if (mouthStyle == 2) {
    // ^ shape (Sedih/Marah/Grumpy)
    spr.fillCircle(mouthX + 24, mouthY + 12, 15, TFT_WHITE);
    spr.fillCircle(mouthX + 24, mouthY + 19, 18, TFT_BLACK); // cut bottom
  } else if (mouthStyle == 3) {
    // _ shape (Datar/Ngantuk)
    spr.fillRect(mouthX + 10, mouthY + 5, 28, 4, TFT_WHITE);
  } else if (mouthStyle == 4) {
    // V shape (Delight/Amazed)
    spr.fillTriangle(mouthX + 10, mouthY, mouthX + 38, mouthY, mouthX + 24, mouthY + 15, TFT_WHITE);
    spr.fillTriangle(mouthX + 10, mouthY - 4, mouthX + 38, mouthY - 4, mouthX + 24, mouthY + 10, TFT_BLACK);
  } else if (mouthStyle == 5) {
    // ~ shape (Guilty/Pusing)
    spr.fillCircle(mouthX + 15, mouthY + 5, 8, TFT_WHITE);
    spr.fillCircle(mouthX + 15, mouthY + 9, 8, TFT_BLACK);
    spr.fillCircle(mouthX + 30, mouthY + 5, 8, TFT_WHITE);
    spr.fillCircle(mouthX + 30, mouthY + 1, 8, TFT_BLACK);
  }

  // Draw Sleep ZZZ
  if (currentSleepAmount > 0.3f) {
    uint8_t alpha = (uint8_t)(200 * currentSleepAmount);
    uint16_t zzColor = spr.color565(alpha/2, alpha/2, alpha);
    float zOff = sin(now * 0.002f) * 5.0f;
    spr.setTextColor(zzColor);
    spr.setTextSize(2);
    spr.setCursor(cx + 50, cy - 70 + (int)zOff);
    spr.print("z");
    spr.setTextSize(3);
    spr.setCursor(cx + 62, cy - 90 + (int)(zOff * 0.7f));
    spr.print("z");
    spr.setTextSize(4);
    spr.setCursor(cx + 72, cy - 115 + (int)(zOff * 0.4f));
    spr.print("Z");
  }
}

void drawMenu() {
  spr.fillSprite(TFT_BLACK);
  spr.setTextColor(TFT_WHITE);
  spr.setTextSize(3);
  spr.setCursor(10, 20);
  spr.print("MENU GEMBOT");
  spr.drawLine(10, 50, 230, 50, TFT_WHITE);
  
  for (int i = 0; i < menuCount; i++) {
    int y = 70 + (i * 40);
    if (i == menuCursor) {
      spr.fillRect(5, y - 5, 230, 35, spr.color565(50, 100, 250));
    }
    spr.setCursor(15, y);
    spr.print(menuItems[i]);
  }
}

void drawPingPong() {
  static float lastBallX = 120, lastBallY = 160;
  static float lastPaddleX = 120;
  static bool firstFrame = true;
  static unsigned long lastFrameMs = 0;
  static float smoothPaddleX = 120;

  unsigned long now = millis();
  if (now - lastFrameMs < 16) return; // ~60 FPS cap
  float dt = (now - lastFrameMs) / 16.0f; // delta-time factor
  if (dt > 3.0f) dt = 3.0f;
  lastFrameMs = now;

  if (firstFrame) {
    display.fillScreen(TFT_BLACK);
    // Draw score header
    display.setTextColor(TFT_WHITE);
    display.setTextSize(2);
    display.setCursor(5, 5);
    display.print("0");
    firstFrame = false;
  }

  if (gameOver) {
    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_RED);
    display.setTextSize(3);
    display.setCursor(30, 120);
    display.print("GAME OVER!");
    display.setTextColor(TFT_WHITE);
    display.setTextSize(2);
    display.setCursor(60, 160);
    display.print("Score: ");
    display.print(score);
    display.setCursor(20, 200);
    display.print("Hold to Restart");
    firstFrame = true;
    return;
  }

  // Update paddle - smooth MPU mapping with lerp
  float rawPaddleTarget = 120.0f + targetLookX * 12.0f;
  if (rawPaddleTarget < 35) rawPaddleTarget = 35;
  if (rawPaddleTarget > 205) rawPaddleTarget = 205;
  smoothPaddleX += (rawPaddleTarget - smoothPaddleX) * 0.35f; // Smooth but responsive
  paddleX = smoothPaddleX;

  // Update ball with delta-time
  ballX += ballVX * dt;
  ballY += ballVY * dt;

  // Bounce walls
  if (ballX <= 8) { ballX = 8; ballVX = fabs(ballVX); playBeep(1200.0f, 30); }
  if (ballX >= 232) { ballX = 232; ballVX = -fabs(ballVX); playBeep(1200.0f, 30); }
  if (ballY <= 25) { ballY = 25; ballVY = fabs(ballVY); playBeep(1000.0f, 30); }

  // Paddle collision (wider zone for easier play)
  if (ballY >= 285 && ballY <= 305 && ballVY > 0) {
    float diff = ballX - paddleX;
    if (fabs(diff) < 40) {
      ballVY = -fabs(ballVY);
      // Angle based on where ball hits paddle
      ballVX = diff * 0.15f;
      // Gentle speed increase
      if (fabs(ballVY) < 6.0f) ballVY *= 1.05f;
      score++;
      playBeep(2000.0f, 50);
      // Update score display
      display.fillRect(0, 0, 60, 22, TFT_BLACK);
      display.setTextColor(TFT_WHITE);
      display.setTextSize(2);
      display.setCursor(5, 5);
      display.print(score);
    }
  }

  // Bottom out
  if (ballY > 320) {
    gameOver = true;
    playBeep(300.0f, 500);
    return;
  }

  // Erase old positions
  if ((int)lastBallX != (int)ballX || (int)lastBallY != (int)ballY) {
    display.fillCircle((int)lastBallX, (int)lastBallY, 7, TFT_BLACK);
  }
  if ((int)lastPaddleX != (int)paddleX) {
    display.fillRoundRect((int)lastPaddleX - 35, 298, 70, 12, 4, TFT_BLACK);
  }

  // Draw new ball (slightly larger for visibility)
  display.fillCircle((int)ballX, (int)ballY, 7, TFT_GREEN);
  // Draw paddle (wider, rounded, colored)
  display.fillRoundRect((int)paddleX - 35, 298, 70, 12, 4, TFT_CYAN);

  lastBallX = ballX;
  lastBallY = ballY;
  lastPaddleX = paddleX;
}

void drawSuhu() {
  spr.fillSprite(TFT_BLACK);
  
  // Draw modern UI for DHT22
  spr.fillRoundRect(20, 40, 200, 100, 10, spr.color565(40, 40, 40));
  spr.fillRoundRect(20, 160, 200, 100, 10, spr.color565(40, 40, 40));

  spr.setTextColor(TFT_ORANGE);
  spr.setTextSize(2);
  spr.setCursor(30, 50);
  spr.print("SUHU RUANGAN");
  spr.setTextColor(TFT_WHITE);
  spr.setTextSize(4);
  spr.setCursor(30, 90);
  if (currentSuhu > 0) {
    spr.print(currentSuhu, 1);
    spr.print(" C");
  } else {
    spr.print("--.- C");
  }

  spr.setTextColor(TFT_CYAN);
  spr.setTextSize(2);
  spr.setCursor(30, 170);
  spr.print("KELEMBAPAN");
  spr.setTextColor(TFT_WHITE);
  spr.setTextSize(4);
  spr.setCursor(30, 210);
  if (currentLembap > 0) {
    spr.print(currentLembap, 1);
    spr.print(" %");
  } else {
    spr.print("--.- %");
  }
}


void drawPengingat() {
  spr.fillSprite(TFT_BLACK);
  
  // Card background
  spr.fillRoundRect(10, 20, 220, 280, 15, spr.color565(50, 50, 80));
  
  // Header
  spr.setTextColor(TFT_YELLOW);
  spr.setTextSize(3);
  spr.setCursor(35, 40);
  spr.print("PENGINGAT");
  
  spr.drawLine(20, 80, 220, 80, TFT_WHITE);
  
  // Content
  spr.setTextColor(TFT_WHITE);
  spr.setTextSize(2);
  spr.setCursor(20, 120);
  spr.print("Belum ada");
  spr.setCursor(20, 150);
  spr.print("jadwal baru.");
  
  // Footer
  spr.setTextColor(spr.color565(150, 150, 150));
  spr.setTextSize(1);
  spr.setCursor(45, 270);
  spr.print("Tahan untuk kembali");
}

void drawMusik() {
  spr.fillSprite(TFT_BLACK);
  spr.setTextColor(TFT_WHITE);
  spr.setTextSize(3);
  spr.setCursor(10, 20);
  spr.print("PILIH LAGU");
  spr.drawLine(10, 50, 230, 50, TFT_WHITE);
  
  for (int i = 0; i < musicCount; i++) {
    int y = 70 + (i * 40);
    if (i == musicCursor) {
      spr.fillRect(5, y - 5, 230, 35, spr.color565(250, 100, 50));
    }
    spr.setCursor(15, y);
    spr.setTextSize(2);
    spr.print(musicItems[i]);
  }
}

void drawScreen() {
  if (currentState == APP_DRAW) return; // Drawn externally via FRAME packets
  
  if (currentState == APP_PINGPONG) {
     drawPingPong();
     return; // Bypass spr.pushSprite for 60 FPS direct drawing
  }

  if (currentState == APP_FACE) drawFace();
  else if (currentState == APP_MENU) drawMenu();
  else if (currentState == APP_SUHU) drawSuhu();
  else if (currentState == APP_PENGINGAT) drawPengingat();
  else if (currentState == APP_MUSIK) drawMusik();

  spr.pushSprite(0, 0);
}


void setup() {
  Serial.begin(115200);
  delay(100);

  dht.begin();

  display.begin();
  display.setRotation(0); // Portrait
  display.fillScreen(TFT_BLACK);
  
  spr.setColorDepth(8);
  spr.createSprite(240, 320);
  if (spr.getPointer() == nullptr) {
    Serial.println("Failed to allocate sprite memory!");
  }

  // Setup I2S Audio
  audioRingBuf = xRingbufferCreate(RING_BUF_SIZE, RINGBUF_TYPE_BYTEBUF);
  initI2S();
  initMicI2S();
  playBeep(1000.0f, 200);

  // Setup Touch Sensor
  // pinMode(TOUCH_PIN, INPUT); // touchRead automatically configures the pin

  // Setup MPU6050
  Wire.begin(21, 22);
  if (!setupRawMPU()) {
    Serial.println("Failed to find MPU6050 chip");
  } else {
    mpuReady = true;
    Serial.println("MPU6050 Found manually!");
  }

  // Setup WiFi
  display.fillScreen(TFT_BLACK);
  display.setTextColor(TFT_WHITE);
  display.setCursor(10, 10);
  display.setTextSize(2);
  display.print("Connecting WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi connected!");
      webSocket.begin(WS_HOST, WS_PORT, "/");
      webSocket.onEvent(webSocketEvent);
      webSocket.setReconnectInterval(5000);
      playBeep(1500.0f, 150);
  } else {
      Serial.println("\nWiFi failed!");
  }

}

void loop() {
  unsigned long now = millis();
  if (now - lastDhtRead > 2000) {
     lastDhtRead = now;
     float t = dht.readTemperature();
     float h = dht.readHumidity();
     if (!isnan(t) && !isnan(h)) {
         currentSuhu = t;
         currentLembap = h;
     }
  }

  handleTouch();
  updateMPU();
  drawScreen();

  
  webSocket.loop();
  readAudioStream();
  
  delay(5); // Run at ~200fps for smoother WebSocket data flow
}






