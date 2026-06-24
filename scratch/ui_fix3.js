const fs = require('fs');
const file = 'src/main_esp32_tft.cpp';
let content = fs.readFileSync(file, 'utf8');

// 1. Disable drawStatus
content = content.replace(/void drawStatus\([^\{]+\{[\s\S]*?spr\.pushSprite\(0,\s*0\);\s*;\s*\n\s*\}/m, 
  'void drawStatus(const char* title, const char* line1, const char* line2 = "") {\n  // Disabled to prevent text over face\n}');

// 2. Add pushScaledSprite
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
content = content.replace('void drawStatus(const char* title, const char* line1, const char* line2);', 
  'void drawStatus(const char* title, const char* line1, const char* line2);\n' + pushScaled);

// 3. Change spr.createSprite(320, 240) to spr.createSprite(128, 64)
content = content.replace(/if \(!spr\.createSprite\(320,\s*240\)\)/g, 'if (!spr.createSprite(128, 64))');

// 4. Replace spr.pushSprite(0, 0);; with pushScaledSprite();
content = content.replace(/spr\.pushSprite\(0,\s*0\);\s*;/g, 'pushScaledSprite();');

// 5. Disable blinking
content = content.replace(/bool isBlinkingNow = \([^;]+;/g, 'bool isBlinkingNow = false; // User requested no blink');

// 6. Disable winking
content = content.replace(/if \(random\(0, 3\) == 0 && !isWinking\) \{/g, 'if (false && random(0, 3) == 0 && !isWinking) { // User requested no blink');

fs.writeFileSync(file, content);
console.log('Fixed UI issues.');
