#include <deque>

#include "keyboard.hpp"
#include "message.hpp"

#include "usb/memory.hpp"
#include "usb/device.hpp"
#include "usb/classdriver/keyboard.hpp"
#include "usb/xhci/xhci.hpp"
#include "usb/xhci/trb.hpp"

void InitializeKeyboard(std::deque<Message>& message_queue) {
  usb::HIDKeyboardDriver::default_observer = 
    [&message_queue](uint8_t keycode) {
      Message msg{Message::kKeyPush};
      msg.arg.keyboard.keycode = keycode;
      msg.arg.keyboard.ascii = keycode_map[keycode];
      message_queue.push_back(msg);
    };
}