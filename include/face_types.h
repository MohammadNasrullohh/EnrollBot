#ifndef FACE_TYPES_H
#define FACE_TYPES_H

#include <Arduino.h>

#define DEF_EYEL_X 18
#define DEF_EYER_X 86
#define DEF_EYE_Y  11
#define DEF_MOUTH_X 56
#define DEF_MOUTH_Y 39

enum EyeType : uint8_t {
  EYE_NORMAL = 0,
  EYE_HAPPY_ARC,
  EYE_X_CROSS,
  EYE_HEART,
  EYE_STAR,
  EYE_SPIRAL,
  EYE_DOT
};

enum MouthType : uint8_t {
  MOUTH_CURVE = 0,
  MOUTH_OPEN,
  MOUTH_ZIGZAG,
  MOUTH_O,
  MOUTH_TONGUE,
  MOUTH_CAT_W
};

enum ExtraFlags : uint8_t {
  EXT_SWEAT    = 0x01,
  EXT_BLUSH    = 0x02,
  EXT_TEARS    = 0x04,
  EXT_HEARTS   = 0x08,
  EXT_SPARKLE  = 0x10,
  EXT_ZZZ      = 0x20,
  EXT_ANGER    = 0x40,
  EXT_QUESTION = 0x80
};

struct FaceState {
  int8_t eyeL_x;
  int8_t eyeL_y;
  int8_t eyeR_x;
  int8_t eyeR_y;
  uint8_t eyeL_w;
  uint8_t eyeL_h;
  uint8_t eyeR_w;
  uint8_t eyeR_h;
  uint8_t eyeL_type;
  uint8_t eyeR_type;
  int8_t pupilL_x;
  int8_t pupilL_y;
  int8_t pupilR_x;
  int8_t pupilR_y;
  uint8_t pupilL_r;
  uint8_t pupilR_r;
  int8_t browL_y;
  int8_t browR_y;
  int8_t browL_angle;
  int8_t browR_angle;
  uint8_t browVisible;
  int8_t mouth_x;
  int8_t mouth_y;
  uint8_t mouth_w;
  uint8_t mouth_h;
  int8_t mouth_curve;
  uint8_t mouth_type;
  uint8_t extras;
};

struct FaceStateF {
  float eyeL_x;
  float eyeL_y;
  float eyeR_x;
  float eyeR_y;
  float eyeL_w;
  float eyeL_h;
  float eyeR_w;
  float eyeR_h;
  float eyeL_type;
  float eyeR_type;
  float pupilL_x;
  float pupilL_y;
  float pupilR_x;
  float pupilR_y;
  float pupilL_r;
  float pupilR_r;
  float browL_y;
  float browR_y;
  float browL_angle;
  float browR_angle;
  float browVisible;
  float mouth_x;
  float mouth_y;
  float mouth_w;
  float mouth_h;
  float mouth_curve;
  float mouth_type;
  float extras;
};

#endif
