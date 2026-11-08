#pragma once

#include <atomic>

namespace Star {

// StarPad: process-wide flag telling the gamepad adapter whether a menu or
// window currently has the player's attention. Set by the client every frame,
// read by the mobile gamepad adapter to switch A/B/X/Y/LB/RB to menu actions.
namespace GamepadUiContext {
  inline std::atomic<bool>& menuActiveFlag() {
    static std::atomic<bool> flag{false};
    return flag;
  }

  inline void setMenuActive(bool active) {
    menuActiveFlag().store(active, std::memory_order_relaxed);
  }

  inline bool menuActive() {
    return menuActiveFlag().load(std::memory_order_relaxed);
  }
}

}
