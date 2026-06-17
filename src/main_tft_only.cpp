#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <driver/i2s.h>
#include <math.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include "hai_sample.h"
#include "secrets.h"

TFT_eSPI display = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&display);

int eyeOffsetX = 0;
int eyeOffsetY = 0;

// === AUDIO OUTPUT (SPEAKER MAX98357A) ===
#define I2S_BCLK 26
#define I2S_LRC  25
#define I2S_DOUT 27

// === AUDIO INPUT (MIC INMP441) ===
#define I2S_MIC_WS 15
#define I2S_MIC_SCK 14
#define I2S_MIC_SD 32

#define BOOT_BUTTON 0

WebSocketsClient webSocket;
float currentRmsVolume = 0.0f;
unsigned long lastTelemetryMs = 0;

void initI2S_Speaker() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 2, 0)
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
#else
    .communication_format = I2S_COMM_FORMAT_I2S,
#endif
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
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
}

void initI2S_Mic() {
  i2s_config_t mic_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 2, 0)
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
#else
    .communication_format = I2S_COMM_FORMAT_I2S,
#endif
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  
  i2s_pin_config_t mic_pin_config = {
    .bck_io_num = I2S_MIC_SCK,
    .ws_io_num = I2S_MIC_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_MIC_SD
  };
  
  i2s_driver_install(I2S_NUM_1, &mic_config, 0, NULL);
  i2s_set_pin(I2S_NUM_1, &mic_pin_config);
}

// === FACE PARAMS ===
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
FaceParam faceListening = { 30, 30, 18, 16,  30, 30, 82, 16,  16, 16, 56, 42, 1 }; // Bulat

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

void pushScaledSpriteCustom() {
  uint16_t lineBuf[240]; 
  int outWidth = 180, outHeight = 104;
  int inWidth = 90, inHeight = 52;
  int startX = 20, startY = 10;
  int drawOffsetX = (240 - outWidth) / 2;
  int drawOffsetY = 80;

  for (int outY = 0; outY < outHeight; outY++) {
    int inY = startY + (outY * inHeight) / outHeight;
    for (int outX = 0; outX < outWidth; outX++) {
      int inX = startX + (outX * inWidth) / outWidth;
      uint16_t color = spr.readPixel(inX, inY) ? TFT_WHITE : TFT_BLACK;
      lineBuf[outX] = color;
    }
    display.pushImage(drawOffsetX, drawOffsetY + outY, outWidth, 1, lineBuf);
  }
}

void drawMochi() {
  spr.fillSprite(0);
  
  int mouthOffset = 0;
  if (currentRmsVolume > 0.05f) {
    mouthOffset = (int)(currentRmsVolume * 25.0f);
  }
  
  spr.fillRoundRect(currentFace.leftEyeX + eyeOffsetX, currentFace.leftEyeY + eyeOffsetY, currentFace.leftEyeW, currentFace.leftEyeH, 8, 1);
  spr.fillRoundRect(currentFace.rightEyeX + eyeOffsetX, currentFace.rightEyeY + eyeOffsetY, currentFace.rightEyeW, currentFace.rightEyeH, 8, 1);

  int mx = currentFace.mouthX, my = currentFace.mouthY + mouthOffset;
  int mw = currentFace.mouthW, mh = currentFace.mouthH;

  if (currentFace.mouthCurve > 0.5f) {
    spr.fillCircle(mx + mw/2, my, mw/2, 1);
    spr.fillRect(mx, my - mw/2, mw, mw/2, 0);
  } else if (currentFace.mouthCurve < -0.5f) {
    spr.fillCircle(mx + mw/2, my + mw/2, mw/2, 1);
    spr.fillRect(mx, my + mw/2, mw, mw/2, 0);
  } else {
    spr.fillRoundRect(mx, my, mw, mh, 4, 1);
  }

  unsigned long now = millis();
  static unsigned long nextBlink = 2000;
  static bool blinking = false;
  static unsigned long blinkStart = 0;
  
  if (!blinking && now > nextBlink) {
    blinking = true;
    blinkStart = now;
    nextBlink = now + random(2000, 5000);
  }
  
  if (blinking) {
    long elapsed = now - blinkStart;
    int blinkHeight = 0;
    if (elapsed < 80) blinkHeight = (elapsed * 37) / 80;
    else if (elapsed < 160) blinkHeight = 37 - ((elapsed - 80) * 37) / 80;
    else blinking = false;
    
    if (blinkHeight > 0) {
      spr.fillRect(10, 0, 108, blinkHeight, 0);
      spr.fillRect(10, 50 - blinkHeight, 108, blinkHeight + 14, 0);
    }
  }

  static unsigned long nextGlance = 1000;
  if (now > nextGlance && !blinking) {
    if (random(0,3) == 0) {
      eyeOffsetX = 0; eyeOffsetY = 0;
    } else {
      eyeOffsetX = random(-3, 4); eyeOffsetY = random(-2, 3);
    }
    nextGlance = now + random(500, 2000);
  }

  pushScaledSpriteCustom();
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("WS Disconnected");
      break;
    case WStype_CONNECTED:
      Serial.println("WS Connected");
      webSocket.sendTXT("{\"type\":\"auth\",\"role\":\"owibot\"}");
      break;
    case WStype_TEXT: {
      String cmdStr = String((char*)payload);
      if (cmdStr.startsWith("CMD:M")) {
        String m = cmdStr.substring(4);
        if (m == "M0") targetFace = faceNormal;
        else if (m == "M1") targetFace = faceHappy;
        else if (m == "M6") targetFace = faceSad;
        else if (m == "M3") targetFace = faceAngry;
        else if (m == "M30") targetFace = faceDizzy;
        else if (m == "M31") targetFace = faceCheeky;
      }
      break;
    }
    case WStype_BIN: {
      // Audio chunk dari VPS (TTS)
      int16_t* samples = (int16_t*)payload;
      int numSamples = length / 2;
      double sum = 0;
      
      int16_t stereoBuf[1024]; 
      if (numSamples > 512) numSamples = 512;
      for (int i=0; i<numSamples; i++) {
         int16_t s = samples[i];
         stereoBuf[i*2] = s;
         stereoBuf[i*2+1] = s;
         sum += ((double)s * (double)s);
      }
      float rms = sqrt(sum / numSamples);
      currentRmsVolume = currentRmsVolume * 0.7f + (rms / 32768.0f) * 0.3f;
      
      size_t bytes_written;
      i2s_write(I2S_NUM_0, stereoBuf, numSamples * 4, &bytes_written, portMAX_DELAY);
      break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  display.init();
  display.setRotation(0); 
  display.fillScreen(TFT_BLACK);
  display.setTextColor(TFT_WHITE);
  display.setTextSize(2);
  display.setCursor(10, 150);
  display.print("Connecting WiFi...");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  
  display.fillScreen(TFT_BLACK);
  display.setCursor(10, 150);
  display.print(WiFi.localIP().toString());
  delay(1500);

  initI2S_Speaker();
  initI2S_Mic();

  webSocket.begin(WS_HOST, WS_PORT, "/");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);

  display.fillScreen(TFT_BLACK);
  spr.setColorDepth(1); 
  if (!spr.createSprite(128, 64)) {
    Serial.println("Sprite fail");
    while(1) delay(100);
  }
}

void loop() {
  unsigned long now = millis();
  webSocket.loop();
  
  // INMP441 di-disable sementara sesuai request user
  /*
  static bool isRecording = false;
  if (digitalRead(BOOT_BUTTON) == LOW) { 
    if (!isRecording) {
      isRecording = true;
      webSocket.sendTXT("{\"event\":\"start_record\"}");
      targetFace = faceListening; 
      Serial.println("Start Recording...");
    }
    // Baca mic 16khz
    uint8_t micBuf[512];
    size_t bytesRead;
    i2s_read(I2S_NUM_1, micBuf, sizeof(micBuf), &bytesRead, portMAX_DELAY);
    if (bytesRead > 0) {
      webSocket.sendBIN(micBuf, bytesRead);
    }
  } else {
    if (isRecording) {
      isRecording = false;
      webSocket.sendTXT("{\"event\":\"stop_record\"}");
      targetFace = faceNormal;
      Serial.println("Stop Recording.");
    }
  }
  */

  // Decay volume mulut (simulasi karena nggak ada input mic lokal)
  currentRmsVolume *= 0.8f;

  // Kirim Telemetry
  if (now - lastTelemetryMs > 1000) {
    lastTelemetryMs = now;
    if (webSocket.isConnected()) {
      char json[128];
      snprintf(json, sizeof(json), "{\"ip\":\"%s\",\"type\":\"telemetry\"}", WiFi.localIP().toString().c_str());
      webSocket.sendTXT(json);
    }
  }

  updateMorph();
  drawMochi();
  delay(30);
}
