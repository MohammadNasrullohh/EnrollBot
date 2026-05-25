#ifndef FACE_DRAW_H
#define FACE_DRAW_H

#include <Adafruit_SSD1306.h>
#include "face_types.h"

// Forward declaration - display object defined in .ino
extern Adafruit_SSD1306 display;

// ===== EYE DRAWING =====

inline void drawEyeNormal(int x, int y, int w, int h, int px, int py, int pr) {
  if (w < 2 || h < 2) return;
  x += px;
  y += py;
  int r = ((w < h) ? w : h) / 4;
  if (r < 1) r = 1;
  if (r > 7) r = 7;
  display.fillRoundRect(x, y, w, h, r, SSD1306_WHITE);
}

inline void drawEyeHappyArc(int x, int y, int w, int h) {
  if (w < 4) return;
  int cx = x + w / 2;
  int cy = y + h / 2 + 2;
  int rx = w / 2;
  int ry = (h / 3 > 4) ? h / 3 : 4;
  for (int t = -2; t <= 1; t++) {
    for (int i = -rx; i <= rx; i++) {
      float norm = (float)i / rx;
      int yi = cy - (int)(ry * (1.0f - norm * norm)) + t;
      display.drawPixel(cx + i, yi, SSD1306_WHITE);
    }
  }
}

inline void drawEyeX(int x, int y, int w, int h) {
  int cx = x + w / 2;
  int cy = y + h / 2;
  int s = ((w < h) ? w : h) / 3;
  for (int t = -1; t <= 1; t++) {
    display.drawLine(cx - s, cy - s + t, cx + s, cy + s + t, SSD1306_WHITE);
    display.drawLine(cx + s, cy - s + t, cx - s, cy + s + t, SSD1306_WHITE);
  }
}

inline void drawEyeHeart(int x, int y, int w, int h) {
  int cx = x + w / 2;
  int cy = y + h / 2;
  int s = ((w < h) ? w : h) / 4;
  if (s < 3) s = 3;
  display.fillCircle(cx - s / 2 - 1, cy - s / 3, s / 2 + 1, SSD1306_WHITE);
  display.fillCircle(cx + s / 2 + 1, cy - s / 3, s / 2 + 1, SSD1306_WHITE);
  display.fillTriangle(cx - s - 1, cy - s / 4, cx + s + 1, cy - s / 4,
                       cx, cy + s + 2, SSD1306_WHITE);
}

inline void drawEyeStar(int x, int y, int w, int h) {
  int cx = x + w / 2;
  int cy = y + h / 2;
  int s = ((w < h) ? w : h) / 3;
  display.drawLine(cx, cy - s, cx, cy + s, SSD1306_WHITE);
  display.drawLine(cx - s, cy, cx + s, cy, SSD1306_WHITE);
  display.drawLine(cx - s + 2, cy - s + 2, cx + s - 2, cy + s - 2, SSD1306_WHITE);
  display.drawLine(cx + s - 2, cy - s + 2, cx - s + 2, cy + s - 2, SSD1306_WHITE);
  display.fillCircle(cx, cy, 2, SSD1306_WHITE);
}

inline void drawEyeSpiral(int x, int y, int w, int h) {
  int cx = x + w / 2;
  int cy = y + h / 2;
  int maxR = ((w < h) ? w : h) / 3;
  for (int r = 2; r <= maxR; r += 3) {
    display.drawCircle(cx, cy, r, SSD1306_WHITE);
  }
}

inline void drawEyeDot(int x, int y, int w, int h) {
  int cx = x + w / 2;
  int cy = y + h / 2;
  display.fillCircle(cx, cy, 3, SSD1306_WHITE);
}

inline void drawEye(int x, int y, int w, int h, uint8_t type,
             int px, int py, int pr) {
  switch (type) {
    case EYE_NORMAL:    drawEyeNormal(x, y, w, h, px, py, pr); break;
    case EYE_HAPPY_ARC: drawEyeHappyArc(x, y, w, h); break;
    case EYE_X_CROSS:   drawEyeX(x, y, w, h); break;
    case EYE_HEART:     drawEyeHeart(x, y, w, h); break;
    case EYE_STAR:      drawEyeStar(x, y, w, h); break;
    case EYE_SPIRAL:    drawEyeSpiral(x, y, w, h); break;
    case EYE_DOT:       drawEyeDot(x, y, w, h); break;
    default:            drawEyeNormal(x, y, w, h, px, py, pr); break;
  }
}

// ===== EYEBROW DRAWING =====

inline void drawBrow(int eyeX, int eyeY, int eyeW, int browY, int browAngle) {
  int x1 = eyeX + 2;
  int x2 = eyeX + eyeW - 2;
  int yBase = eyeY + browY - 4;
  int y1 = yBase + browAngle;
  int y2 = yBase - browAngle;
  for (int t = -1; t <= 0; t++) {
    display.drawLine(x1, y1 + t, x2, y2 + t, SSD1306_WHITE);
  }
}

// ===== MOUTH DRAWING =====

inline void drawMouthCurve(int x, int y, int w, int curve) {
  if (w < 4) return;
  int half = w / 2;
  if (half < 2) return;

  int depth = abs(curve);
  if (depth < 4) depth = 4;
  if (depth > 8) depth = 8;
  int dir = (curve >= 0) ? 1 : -1;
  int denom = half * half;

  for (int i = 0; i <= w; i++) {
    int dx = i - half;
    int arch = denom - dx * dx;
    if (arch < 0) arch = 0;
    int cy = y + dir * ((depth * arch) / denom);
    display.fillCircle(x + i, cy, 2, SSD1306_WHITE);
  }
}

inline void drawMouthOpen(int x, int y, int w, int h, int curve) {
  if (w < 4) w = 4;
  if (h < 4) h = 4;
  int r = (w < h) ? w / 2 : h / 2;
  int yOff = -(curve * 2 / 10);
  display.fillRoundRect(x, y + yOff, w, h, r, SSD1306_WHITE);
  if (w > 6 && h > 6) {
    int ir = (r - 2 > 1) ? r - 2 : 1;
    display.fillRoundRect(x + 2, y + yOff + 2, w - 4, h - 4, ir, SSD1306_BLACK);
  }
}

inline void drawMouthZigzag(int x, int y, int w) {
  int segments = 4;
  int segW = w / segments;
  if (segW < 2) segW = 2;
  for (int t = 0; t < 2; t++) {
    for (int i = 0; i < segments; i++) {
      int xx1 = x + i * segW;
      int xx2 = x + i * segW + segW / 2;
      int xx3 = x + (i + 1) * segW;
      int yUp = y - 3 + t;
      int yDn = y + 3 + t;
      display.drawLine(xx1, (i % 2 == 0) ? yDn : yUp,
                       xx2, (i % 2 == 0) ? yUp : yDn, SSD1306_WHITE);
      display.drawLine(xx2, (i % 2 == 0) ? yUp : yDn,
                       xx3, (i % 2 == 0) ? yDn : yUp, SSD1306_WHITE);
    }
  }
}

inline void drawMouthO(int x, int y, int w, int h) {
  int cx = x + w / 2;
  int cy = y;
  int rx = (w / 3 > 3) ? w / 3 : 3;
  display.fillCircle(cx, cy, rx, SSD1306_WHITE);
  int ir = (rx - 2 > 1) ? rx - 2 : 1;
  display.fillCircle(cx, cy, ir, SSD1306_BLACK);
}

inline void drawMouthTongue(int x, int y, int w, int h) {
  int r = ((w < h) ? w : h) / 2;
  if (r < 2) r = 2;
  display.fillRoundRect(x, y - 2, w, h, r, SSD1306_WHITE);
  int ir = (r - 2 > 1) ? r - 2 : 1;
  display.fillRoundRect(x + 2, y, w - 4, h - 3, ir, SSD1306_BLACK);
  display.fillRoundRect(x + w / 3, y + h / 2 - 2, w / 3, h / 2 + 1,
                        3, SSD1306_WHITE);
}

inline void drawMouthCatW(int x, int y, int w) {
  int cx = x + w / 2;
  int s = w / 4;
  if (s < 2) s = 2;
  for (int t = 0; t < 2; t++) {
    display.drawLine(cx - s * 2, y + t, cx - s, y - 3 + t, SSD1306_WHITE);
    display.drawLine(cx - s, y - 3 + t, cx, y + t, SSD1306_WHITE);
    display.drawLine(cx, y + t, cx + s, y - 3 + t, SSD1306_WHITE);
    display.drawLine(cx + s, y - 3 + t, cx + s * 2, y + t, SSD1306_WHITE);
  }
}

inline void drawMouth(int x, int y, int w, int h, int curve, uint8_t type) {
  switch (type) {
    case MOUTH_CURVE:  drawMouthCurve(x, y, w, curve); break;
    case MOUTH_OPEN:   drawMouthOpen(x, y, w, h, curve); break;
    case MOUTH_ZIGZAG: drawMouthZigzag(x, y, w); break;
    case MOUTH_O:      drawMouthO(x, y, w, h); break;
    case MOUTH_TONGUE: drawMouthTongue(x, y, w, h); break;
    case MOUTH_CAT_W:  drawMouthCatW(x, y, w); break;
    default:           drawMouthCurve(x, y, w, curve); break;
  }
}

// ===== EXTRAS DRAWING =====

inline void drawSweat(int x, int y) {
  display.fillCircle(x, y + 8, 3, SSD1306_WHITE);
  display.fillTriangle(x - 2, y + 6, x + 2, y + 6, x, y, SSD1306_WHITE);
}

inline void drawBlush(int lyX, int lyY, int ryX, int ryY) {
  for (int i = 0; i < 3; i++) {
    display.drawLine(lyX - 4 + i * 3, lyY, lyX - 2 + i * 3, lyY, SSD1306_WHITE);
  }
  for (int i = 0; i < 3; i++) {
    display.drawLine(ryX - 4 + i * 3, ryY, ryX - 2 + i * 3, ryY, SSD1306_WHITE);
  }
}

inline void drawTears(int lx, int ly, int rx, int ry, int h) {
  for (int t = 0; t < 2; t++) {
    display.drawLine(lx + 2 + t, ly, lx + t, ly + h, SSD1306_WHITE);
    display.drawLine(rx + 2 + t, ry, rx + t, ry + h, SSD1306_WHITE);
  }
}

inline void drawFloatingHearts(int x, int y, unsigned long ms) {
  int off = (ms / 200) % 8;
  int hx = x + 4;
  int hy = y - 6 - off;
  display.fillCircle(hx - 1, hy, 1, SSD1306_WHITE);
  display.fillCircle(hx + 1, hy, 1, SSD1306_WHITE);
  display.drawPixel(hx, hy + 2, SSD1306_WHITE);
}

inline void drawSparkle(int x, int y, unsigned long ms) {
  int off = (ms / 300) % 4;
  int sx = x - 6 + off;
  int sy = y - 4 - off;
  display.drawPixel(sx, sy - 2, SSD1306_WHITE);
  display.drawPixel(sx, sy + 2, SSD1306_WHITE);
  display.drawPixel(sx - 2, sy, SSD1306_WHITE);
  display.drawPixel(sx + 2, sy, SSD1306_WHITE);
  display.drawPixel(sx, sy, SSD1306_WHITE);
  sx = x + 20 - off;
  sy = y - 2 + off;
  display.drawPixel(sx, sy - 1, SSD1306_WHITE);
  display.drawPixel(sx, sy + 1, SSD1306_WHITE);
  display.drawPixel(sx - 1, sy, SSD1306_WHITE);
  display.drawPixel(sx + 1, sy, SSD1306_WHITE);
}

inline void drawZzz(int x, int y, unsigned long ms) {
  int off = (ms / 400) % 6;
  int zx = x + 20 + off;
  int zy = y - 4 - off;
  display.drawLine(zx, zy, zx + 4, zy, SSD1306_WHITE);
  display.drawLine(zx + 4, zy, zx, zy + 4, SSD1306_WHITE);
  display.drawLine(zx, zy + 4, zx + 4, zy + 4, SSD1306_WHITE);
  zx += 5;
  zy -= 4;
  display.drawLine(zx, zy, zx + 3, zy, SSD1306_WHITE);
  display.drawLine(zx + 3, zy, zx, zy + 3, SSD1306_WHITE);
  display.drawLine(zx, zy + 3, zx + 3, zy + 3, SSD1306_WHITE);
}

inline void drawAngerMark(int x, int y) {
  display.drawLine(x, y, x + 4, y + 4, SSD1306_WHITE);
  display.drawLine(x + 4, y, x, y + 4, SSD1306_WHITE);
  display.drawLine(x + 1, y, x + 5, y + 4, SSD1306_WHITE);
  display.drawLine(x + 5, y, x + 1, y + 4, SSD1306_WHITE);
}

inline void drawQuestionMark(int x, int y) {
  display.drawCircle(x + 3, y + 2, 3, SSD1306_WHITE);
  display.fillRect(x + 4, y - 1, 3, 3, SSD1306_BLACK);
  display.drawLine(x + 3, y + 4, x + 3, y + 6, SSD1306_WHITE);
  display.drawPixel(x + 3, y + 8, SSD1306_WHITE);
}

// ===== MASTER RENDER =====

inline void renderFace(FaceStateF& s, unsigned long ms) {
  display.clearDisplay();

  int elw = (int)s.eyeL_w;
  int erw = (int)s.eyeR_w;
  int elh = (int)s.eyeL_h;
  int erh = (int)s.eyeR_h;
  if (elw < 4) elw = 4;
  if (erw < 4) erw = 4;
  if (elh < 2) elh = 2;
  if (erh < 2) erh = 2;

  int elx = DEF_EYEL_X + (int)s.eyeL_x;
  int ely = DEF_EYE_Y + (int)s.eyeL_y;
  int erx = DEF_EYER_X + (int)s.eyeR_x;
  int ery = DEF_EYE_Y + (int)s.eyeR_y;

  // Force pr (pupil radius) to 0 for all shapes just in case
  drawEye(elx, ely, elw, elh, (uint8_t)s.eyeL_type,
          (int)s.pupilL_x, (int)s.pupilL_y, 0);
  drawEye(erx, ery, erw, erh, (uint8_t)s.eyeR_type,
          (int)s.pupilR_x, (int)s.pupilR_y, 0);

  if (s.browVisible > 0.5f) {
    drawBrow(elx, ely, elw, (int)s.browL_y, (int)s.browL_angle);
    drawBrow(erx, ery, erw, (int)s.browR_y, (int)s.browR_angle);
  }

  int mx = DEF_MOUTH_X + (int)s.mouth_x;
  int my = DEF_MOUTH_Y + (int)s.mouth_y;
  int mw = (int)s.mouth_w;
  if (mw < 2) mw = 2;
  
  drawMouth(mx, my, mw, (int)s.mouth_h,
            (int)s.mouth_curve, (uint8_t)s.mouth_type);

  uint8_t ex = (uint8_t)s.extras;
  if (ex & EXT_SWEAT)    drawSweat(elx - 4, ely + 4);
  if (ex & EXT_BLUSH)    drawBlush(elx + elw / 2, ely + elh + 2,
                                   erx + erw / 2, ery + erh + 2);
  if (ex & EXT_TEARS)    drawTears(elx + 4, ely + elh, erx + erw - 6, ery + erh, 10);
  if (ex & EXT_HEARTS)   drawFloatingHearts(erx + erw, ery, ms);
  if (ex & EXT_SPARKLE) { drawSparkle(elx, ely, ms); drawSparkle(erx + erw, ery, ms); }
  if (ex & EXT_ZZZ)      drawZzz(erx + erw, ery, ms);
  if (ex & EXT_ANGER)    drawAngerMark(erx + erw + 2, ery - 4);
  if (ex & EXT_QUESTION) drawQuestionMark(erx + erw + 4, ery - 6);

  display.display();
}

#endif
