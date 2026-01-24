#include "Adafruit_TinyUSB.h"

// ================================
// USB HID Mouse
// ================================

Adafruit_USBD_HID usb_hid;

uint8_t const hid_report_descriptor[] = {
  TUD_HID_REPORT_DESC_MOUSE()
};

// ================================
// 触摸 → 逻辑层结构
// ================================

struct TouchFrame {
  uint8_t fingers; // 0 / 1 / 2
  int16_t dx;
  int16_t dy;      // 单指：位移；双指：已融合后的滚动位移
  bool tap;        // 单指 tap 事件
};

struct PointerState {
  int8_t dx;
  int8_t dy;
  int8_t wheel;
  bool left;
};

// ================================
// 触摸逻辑映射（核心）
// ================================

PointerState touch_process(const TouchFrame& t)
{
  PointerState p{};
  // 默认全 0

  // 双指：滚轮
  if (t.fingers == 2) {
    p.wheel = -t.dy;  // 触摸向上 → 页面向上
    return p;
  }

  // 单指：移动
  if (t.fingers == 1) {
    p.dx = t.dx;
    p.dy = t.dy;

    if (t.tap) {
      p.left = true;
    }
    return p;
  }

  return p;
}

// ================================
// USB 发送
// ================================

void send_mouse(const PointerState& p)
{
  uint8_t buttons = p.left ? MOUSE_BUTTON_LEFT : 0;

  usb_hid.mouseReport(
    0,            // Report ID
    buttons,
    p.dx,
    p.dy,
    p.wheel,
    0             // horizontal wheel
  );
}

// ================================
// mock 触摸输入（现在用假的）
// 后面直接换成真实触摸 IC
// ================================

TouchFrame read_touch_mock()
{
  static uint32_t tick = 0;
  tick++;

  TouchFrame t{};
  
  // 示例：每隔一段时间切换模式
  if ((tick / 200) % 3 == 0) {
    // 单指移动
    t.fingers = 1;
    t.dx = 2;
    t.dy = 1;
    t.tap = false;
  }
  else if ((tick / 200) % 3 == 1) {
    // 单指 tap
    t.fingers = 1;
    t.dx = 0;
    t.dy = 0;
    t.tap = (tick % 50 == 0);
  }
  else {
    // 双指滚动
    t.fingers = 2;
    t.dy = 2;
  }

  return t;
}

// ================================
// Arduino 生命周期
// ================================

void setup()
{
  usb_hid.setReportDescriptor(hid_report_descriptor,
                             sizeof(hid_report_descriptor));
  usb_hid.begin();

  // 等待 USB 枚举完成
  while (!TinyUSBDevice.mounted()) {
    delay(10);
  }
}

void loop()
{
  TouchFrame t = read_touch_mock();
  PointerState p = touch_process(t);
  send_mouse(p);

  delay(5);
}
