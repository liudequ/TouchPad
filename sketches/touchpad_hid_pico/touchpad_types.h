#pragma once

enum ZoneType {
  ZONE_NONE = 0,
  ZONE_MOUSE = 1,
  ZONE_KEYBOARD = 2
};

struct ZoneBinding {
  ZoneType type;
  uint8_t mouseButtons;
  uint8_t keyModifier;
  uint8_t keyCode;
};
