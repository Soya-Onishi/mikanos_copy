#include <deque>

#include "keyboard.hpp"
#include "message.hpp"
#include "task.hpp"

#include "usb/memory.hpp"
#include "usb/device.hpp"
#include "usb/classdriver/keyboard.hpp"
#include "usb/xhci/xhci.hpp"
#include "usb/xhci/trb.hpp"

namespace {
  const int kLControlBitMask = 0b00000001u;
  const int kLShiftBitMask   = 0b00000010u;
  const int kLAltBitMask     = 0b00000100u;
  const int kLGUIBitMask     = 0b00001000u;
  const int kRControlBitMask = 0b00010000u;
  const int kRShiftBitMask   = 0b00100000u;
  const int kRAltBitMask     = 0b01000000u;
  const int kRGUIBitMask     = 0b10000000u;
}

void InitializeKeyboard() {
  usb::HIDKeyboardDriver::default_observer = 
    [](uint8_t modifier, uint8_t keycode) {      
      const bool shift = (modifier & (kLShiftBitMask | kRShiftBitMask)) != 0;
      char ascii = keycode_map[keycode];
      if(shift) {
        ascii = keycode_map_shifted[keycode];
      }

      Message msg{Message::kKeyPush};
      msg.arg.keyboard.keycode = keycode;
      msg.arg.keyboard.ascii = ascii;
      msg.arg.keyboard.modifier = modifier;
      
      task_manager->SendMessage(1, msg);      
    };
}