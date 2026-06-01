#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_MPU6050 mpu;

bool oledReady = false;
bool mpuReady = false;
bool rawMpuReady = false;
uint8_t mpuAddr = 0x68;
uint8_t whoAmI = 0xFF;
uint8_t foundAddrs[8];
uint8_t foundCount = 0;

void scanI2C() {
  foundCount = 0;
  for (uint8_t addr = 1; addr < 127 && foundCount < 8; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      foundAddrs[foundCount++] = addr;
    }
  }
}

void drawHeader(const char* title) {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(title);
  display.drawFastHLine(0, 10, 128, SH110X_WHITE);
}

bool i2cAck(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
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

bool tryBeginRawMPU() {
  for (uint8_t addr : { (uint8_t)0x68, (uint8_t)0x69 }) {
    bool ok = false;
    uint8_t who = readReg8(addr, 0x75, ok);
    if (ok && (who == 0x68 || who == 0x70 || who == 0x71)) {
      mpuAddr = addr;
      whoAmI = who;
      writeReg8(addr, 0x6B, 0x00); // wake
      delay(50);
      writeReg8(addr, 0x1A, 0x03); // low-pass filter
      writeReg8(addr, 0x1B, 0x08); // gyro +-500 dps
      writeReg8(addr, 0x1C, 0x08); // accel +-4g
      return true;
    }
  }
  return false;
}

bool tryBeginMPU() {
  if (mpu.begin(0x68, &Wire)) {
    mpuAddr = 0x68;
    return true;
  }
  if (mpu.begin(0x69, &Wire)) {
    mpuAddr = 0x69;
    return true;
  }
  return false;
}

void drawScanOnly() {
  bool ok68 = false;
  bool ok69 = false;
  uint8_t who68 = readReg8(0x68, 0x75, ok68);
  uint8_t who69 = readReg8(0x69, 0x75, ok69);

  drawHeader("MPU / I2C TEST");
  display.setCursor(0, 13);
  display.print("MPU: ERR");
  display.setCursor(0, 23);
  display.print("I2C:");
  for (uint8_t i = 0; i < foundCount; i++) {
    display.print(" 0x");
    if (foundAddrs[i] < 16) display.print("0");
    display.print(foundAddrs[i], HEX);
  }
  display.setCursor(0, 35);
  display.print("68 ");
  display.print(i2cAck(0x68) ? "ACK" : "NO ");
  display.print(" WHO ");
  if (ok68) display.print(who68, HEX); else display.print("--");
  display.setCursor(0, 47);
  display.print("69 ");
  display.print(i2cAck(0x69) ? "ACK" : "NO ");
  display.print(" WHO ");
  if (ok69) display.print(who69, HEX); else display.print("--");
  display.setCursor(0, 57);
  display.print("Need WHO=68");
  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Wire.begin(D4, D5);
  Wire.setClock(100000);

  oledReady = display.begin(OLED_ADDR, true);
  if (oledReady) {
    display.clearDisplay();
    display.display();
  }

  scanI2C();
  mpuReady = tryBeginMPU();
  rawMpuReady = mpuReady;
  if (!rawMpuReady) rawMpuReady = tryBeginRawMPU();
  if (mpuReady) {
    mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  }

  Serial.println("MPU OLED TEST");
  Serial.print("MPU: ");
  Serial.println(rawMpuReady ? "OK" : "ERR");
  Serial.print("ADDR: 0x");
  Serial.println(mpuAddr, HEX);
  Serial.print("WHO: 0x");
  Serial.println(whoAmI, HEX);
}

void loop() {
  static unsigned long lastScan = 0;
  if (millis() - lastScan > 2500UL) {
    lastScan = millis();
    scanI2C();
    if (!mpuReady) mpuReady = tryBeginMPU();
    rawMpuReady = mpuReady || tryBeginRawMPU();
  }

  if (!oledReady) {
    delay(500);
    return;
  }

  if (!rawMpuReady) {
    drawScanOnly();
    delay(250);
    return;
  }

  float axMs2 = 0;
  float ayMs2 = 0;
  float gxRad = 0;
  float gyRad = 0;
  float gzRad = 0;

  if (mpuReady) {
    sensors_event_t a;
    sensors_event_t g;
    sensors_event_t temp;
    mpu.getEvent(&a, &g, &temp);
    axMs2 = a.acceleration.x;
    ayMs2 = a.acceleration.y;
    gxRad = g.gyro.x;
    gyRad = g.gyro.y;
    gzRad = g.gyro.z;
  } else {
    int16_t ax, ay, az, gx, gy, gz;
    if (!readRawMotion(mpuAddr, ax, ay, az, gx, gy, gz)) {
      rawMpuReady = false;
      delay(120);
      return;
    }
    axMs2 = ((float)ax / 8192.0f) * 9.80665f;
    ayMs2 = ((float)ay / 8192.0f) * 9.80665f;
    gxRad = ((float)gx / 65.5f) * 0.0174533f;
    gyRad = ((float)gy / 65.5f) * 0.0174533f;
    gzRad = ((float)gz / 65.5f) * 0.0174533f;
  }

  drawHeader("MPU GYRO OK");
  display.setCursor(86, 0);
  display.print("0x");
  display.print(mpuAddr, HEX);
  display.print("/");
  display.print(whoAmI, HEX);
  display.setCursor(0, 14);
  display.print("GX ");
  display.print(gxRad, 2);
  display.print(" rad/s");
  display.setCursor(0, 26);
  display.print("GY ");
  display.print(gyRad, 2);
  display.print(" rad/s");
  display.setCursor(0, 38);
  display.print("GZ ");
  display.print(gzRad, 2);
  display.print(" rad/s");
  display.setCursor(0, 52);
  display.print("AX ");
  display.print(axMs2, 1);
  display.print(" AY ");
  display.print(ayMs2, 1);
  display.display();

  Serial.print("gyro ");
  Serial.print(gxRad, 3);
  Serial.print(", ");
  Serial.print(gyRad, 3);
  Serial.print(", ");
  Serial.println(gzRad, 3);
  delay(120);
}
