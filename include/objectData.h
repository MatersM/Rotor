#pragma once
#include <Arduino.h>

enum CalibrationCommand { CC_NONE, CC_OK, CC_LEFT, CC_RIGHT, CC_UP, CC_DOWN};
enum CalibrationDirection { CD_NORTH=0, CD_SOUTH=1} ;

struct CalibrationData {
  CalibrationCommand    command = CC_NONE;
  uint16_t              speed;
  CalibrationDirection  direction;
};
