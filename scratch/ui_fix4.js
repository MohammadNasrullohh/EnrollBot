const fs = require('fs');
const file = 'src/main_esp32_tft.cpp';
let content = fs.readFileSync(file, 'utf8');

const pushScaled = `
void pushScaledSprite() {
  uint16_t lineBuf[256];
  for (int y = 0; y < 64; y++) {
    for (int x = 0; x < 128; x++) {
      uint16_t color = spr.readPixel(x, y) ? TFT_CYAN : TFT_BLACK;
      lineBuf[x * 2] = color;
      lineBuf[x * 2 + 1] = color;
    }
    display.pushImage(32, y * 2 + 56, 256, 1, lineBuf);
    display.pushImage(32, y * 2 + 57, 256, 1, lineBuf);
  }
}

`;
// Insert pushScaled before drawBitmapFaceStable
content = content.replace('void drawBitmapFaceStable', pushScaled + 'void drawBitmapFaceStable');

// Remove drawStatus calls in setup()
content = content.replace(/if \(oledReady\) drawStatus\("DFPLAYER ERR", "TX>D6 RX>D1", "cek SD 0001\.mp3"\);/g, '');
content = content.replace(/drawStatus\("DFPLAYER OK", "\/mp3\/0001\.mp3", "siap diputar"\);/g, '');
content = content.replace(/if \(oledReady\) drawStatus\("DFPLAYER ERR", "module belum kebaca", "cek wiring\/SD"\);/g, '');
content = content.replace(/drawStatus\("DFPLAYER", line, "speaker DFPlayer"\);/g, '');

fs.writeFileSync(file, content);
console.log('Fixed UI issues part 4.');
