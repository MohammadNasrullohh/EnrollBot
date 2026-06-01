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

// Shared I2S clock pins
#define I2S_BCLK_PIN D0
#define I2S_WS_PIN D8

// INMP441 input
#define INMP_SD_PIN D10

// MAX98357 output
#define MAX_DIN_PIN D7

const uint32_t AUDIO_RATE = 16000;
const uint16_t FRAMES = 128;
const float MIC_GAIN = 2.2f;

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool oledReady = false;
bool i2sReady = false;
unsigned long lastDrawMs = 0;
float smoothLevel = 0.0f;
int32_t rxSamples[FRAMES * 2];
int32_t txSamples[FRAMES * 2];

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
  display.setCursor(4, 23);
  display.print(line1);
  display.setCursor(4, 39);
  display.print(line2);
  display.display();
}

bool setupI2S() {
  i2s_config_t config = {};
  config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX);
  config.sample_rate = AUDIO_RATE;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 8;
  config.dma_buf_len = FRAMES;
  config.use_apll = false;
  config.tx_desc_auto_clear = true;
  config.fixed_mclk = 0;

  i2s_pin_config_t pins = {};
  pins.bck_io_num = I2S_BCLK_PIN;
  pins.ws_io_num = I2S_WS_PIN;
  pins.data_out_num = MAX_DIN_PIN;
  pins.data_in_num = INMP_SD_PIN;

  esp_err_t err = i2s_driver_install(I2S_NUM_0, &config, 0, nullptr);
  if (err != ESP_OK) {
    Serial.print("i2s_driver_install ERR ");
    Serial.println((int)err);
    return false;
  }

  err = i2s_set_pin(I2S_NUM_0, &pins);
  if (err != ESP_OK) {
    Serial.print("i2s_set_pin ERR ");
    Serial.println((int)err);
    i2s_driver_uninstall(I2S_NUM_0);
    return false;
  }

  i2s_zero_dma_buffer(I2S_NUM_0);
  return true;
}

int32_t clamp24(float value) {
  if (value > 8388607.0f) return 8388607;
  if (value < -8388608.0f) return -8388608;
  return (int32_t)value;
}

float readMicAndWriteSpeaker(bool& gotData) {
  size_t bytesRead = 0;
  esp_err_t err = i2s_read(I2S_NUM_0, rxSamples, sizeof(rxSamples), &bytesRead, 70 / portTICK_PERIOD_MS);
  if (err != ESP_OK || bytesRead == 0) {
    gotData = false;
    memset(txSamples, 0, sizeof(txSamples));
    size_t written = 0;
    i2s_write(I2S_NUM_0, txSamples, sizeof(txSamples), &written, 10 / portTICK_PERIOD_MS);
    return 0.0f;
  }

  gotData = true;
  uint16_t samples = bytesRead / sizeof(int32_t);
  double sum = 0.0;
  int32_t peak = 0;

  for (uint16_t i = 0; i < samples; i += 2) {
    // INMP441 L/R tied to GND, so the left slot is the useful one.
    int32_t mic = rxSamples[i] >> 8;
    int32_t absMic = abs(mic);
    if (absMic > peak) peak = absMic;
    sum += (double)mic * (double)mic;

    int32_t out = clamp24((float)mic * MIC_GAIN);
    txSamples[i] = out << 8;
    txSamples[i + 1] = out << 8;
  }

  size_t written = 0;
  i2s_write(I2S_NUM_0, txSamples, samples * sizeof(int32_t), &written, 20 / portTICK_PERIOD_MS);

  float rms = sqrt(sum / max((uint16_t)1, (uint16_t)(samples / 2)));
  float level = rms / 240000.0f;
  if (peak > 1000000) level = max(level, 0.85f);
  if (level > 1.0f) level = 1.0f;
  return level;
}

void drawLevel(float level, bool gotData) {
  if (!oledReady) return;

  int barWidth = (int)(level * 116.0f);
  display.clearDisplay();
  display.fillRect(0, 0, SCREEN_WIDTH, 12, OLED_WHITE);
  display.setTextColor(OLED_BLACK);
  display.setTextSize(1);
  display.setCursor(3, 2);
  display.print(gotData ? "INMP + MAX OK" : "I2S WAIT");

  display.setTextColor(OLED_WHITE);
  display.setCursor(4, 17);
  display.print("BCLK D0  WS D8");
  display.setCursor(4, 28);
  display.print("MIC D10  MAX D7");

  display.drawRect(5, 43, 118, 12, OLED_WHITE);
  display.fillRect(6, 44, barWidth, 10, OLED_WHITE);

  display.setCursor(5, 58);
  if (!gotData) display.print("Tidak ada data");
  else if (level < 0.04f) display.print("Diam");
  else if (level < 0.35f) display.print("Mic terbaca");
  else display.print("Kencang");
  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(900);
  Serial.println("INMP441 + MAX98357 test");
  Serial.println("INMP: VDD 3V3, GND, SCK D0, WS D8, SD D10, L/R GND");
  Serial.println("MAX : VIN 5V, GND, BCLK D0, LRC D8, DIN D7, SD 3V3");

  Wire.begin(D4, D5);
  oledReady = display.begin(OLED_ADDR, true);
  if (oledReady) {
    display.clearDisplay();
    display.display();
    drawStatus("INMP + MAX TEST", "Init shared I2S", "D0/D8 shared");
  } else {
    Serial.println("OLED ERR");
  }

  i2sReady = setupI2S();
  if (!i2sReady) {
    drawStatus("I2S ERR", "gagal init", "cek pin D0 D8");
    while (true) delay(1000);
  }

  drawStatus("INMP + MAX OK", "Tepuk / bicara", "jauhkan speaker dari mic");
}

void loop() {
  bool gotData = false;
  float level = readMicAndWriteSpeaker(gotData);
  smoothLevel = smoothLevel * 0.78f + level * 0.22f;

  if (millis() - lastDrawMs > 90UL) {
    lastDrawMs = millis();
    drawLevel(smoothLevel, gotData);
    Serial.print("mic level=");
    Serial.println(smoothLevel, 3);
  }
}
