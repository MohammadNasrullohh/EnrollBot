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

const uint32_t AUDIO_RATE = 16000;
const uint16_t FRAMES = 256;

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool oledReady = false;
float phase = 0.0f;
uint16_t toneHz = 440;
unsigned long lastScreenMs = 0;
int16_t samples[FRAMES * 2];

void oledMessage(const char* title, const char* line1, const char* line2) {
  if (!oledReady) return;
  display.clearDisplay();
  display.setTextWrap(false);
  display.fillRect(0, 0, SCREEN_WIDTH, 12, OLED_WHITE);
  display.setTextColor(OLED_BLACK);
  display.setTextSize(1);
  display.setCursor(3, 2);
  display.print(title);
  display.setTextColor(OLED_WHITE);
  display.setCursor(3, 23);
  display.print(line1);
  display.setCursor(3, 39);
  display.print(line2);
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
  config.dma_buf_len = FRAMES;
  config.use_apll = true;
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

void writeSoftTone(uint16_t hz) {
  float step = 2.0f * PI * (float)hz / (float)AUDIO_RATE;
  for (uint16_t i = 0; i < FRAMES; i++) {
    int16_t sample = (int16_t)(sinf(phase) * 3600.0f);
    phase += step;
    if (phase > 2.0f * PI) phase -= 2.0f * PI;
    samples[i * 2] = sample;
    samples[i * 2 + 1] = sample;
  }
  size_t written = 0;
  i2s_write(I2S_NUM_0, samples, sizeof(samples), &written, portMAX_DELAY);
}

void setup() {
  Serial.begin(115200);
  delay(700);
  Serial.println("MAX98357 clean sine test");
  Serial.println("MAX: VIN 5V, GND, BCLK D0, LRC D8, DIN D7, SD 3V3");

  Wire.begin(D4, D5);
  oledReady = display.begin(OLED_ADDR, true);
  if (oledReady) {
    display.clearDisplay();
    display.display();
  }

  if (!setupI2S()) {
    oledMessage("MAX I2S ERR", "cek pin D0 D8 D7", "SD wajib 3V3");
    while (true) delay(1000);
  }

  oledMessage("MAX CLEAN TEST", "Nada murni pelan", "Kalau kresek=hardware");
}

void loop() {
  writeSoftTone(toneHz);
  unsigned long now = millis();
  if (now - lastScreenMs > 1400UL) {
    lastScreenMs = now;
    toneHz = (toneHz == 440) ? 660 : 440;
    oledMessage("MAX CLEAN TEST", toneHz == 440 ? "Tone 440 Hz" : "Tone 660 Hz", "harus halus");
    Serial.print("tone ");
    Serial.println(toneHz);
  }
}
