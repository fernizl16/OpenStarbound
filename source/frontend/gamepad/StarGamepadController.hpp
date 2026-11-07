#pragma once

// StarPad: native gamepad layer for OpenStarbound.
// Phase 1: movement, stick swap, sensitivity, direction-based aiming with
// Auto (smart target) and Precision (free cursor) modes, core gameplay buttons.

#include "StarInputEvent.hpp"
#include "StarJson.hpp"
#include "StarGamepadLogic.hpp"

namespace Star {

STAR_CLASS(Player);
STAR_CLASS(WorldClient);
STAR_CLASS(MainInterface);
STAR_CLASS(GamepadController);

class GamepadController {
public:
  enum class AimMode { Auto, Precision };
  enum class AimContext { Combat, Mine, Place };
  enum class AimAssist { Off, Soft, Strong };

  struct Settings {
    bool enabled = true;
    bool swapSticks = false;
    Gamepad::StickSettings moveStick;
    Gamepad::StickSettings aimStick;
    AimAssist aimAssist = AimAssist::Soft;
    float autoAimRange = 10.0f;        // tiles, combat aim distance
    float precisionSpeed = 22.0f;      // tiles per second at full deflection
    float precisionMaxOffset = 40.0f;  // tiles from the player
    float triggerThreshold = 0.35f;
    float tapTime = 0.25f;
    float holdTime = 0.4f;
    float facingMemory = 0.6f;         // seconds the last aim direction is kept after releasing the stick

    static Settings fromJson(Json const& json);
    Json toJson() const;
  };

  GamepadController();

  // Reads "starpad" from the client configuration (creating defaults).
  void loadSettings();
  void saveSettings();
  Settings const& settings() const;
  void setSettings(Settings const& settings);

  // Feed every input event. Never consumes events; it only observes them and
  // decides whether the player is currently using the gamepad or the mouse.
  void handleInputEvent(InputEvent const& event);

  // True while the gamepad is the active device (last used).
  bool active() const;

  // Movement vector for this frame (x used for walking, y for up/down).
  Vec2F moveVector() const;

  // Drives the player for one frame. Call after keyboard handling so both
  // devices can be used together. When no window is open this runs gameplay;
  // when a window is open only menu-level buttons are handled (Phase 3 adds
  // full widget navigation).
  void update(float dt, PlayerPtr const& player, WorldClientPtr const& world, MainInterface* mainInterface);

  AimMode aimMode() const;
  AimContext aimContext() const;
  Maybe<Vec2F> aimWorldPosition() const;

private:
  enum Button {
    A, B, X, Y, LB, RB, LT, RT, LS, RS, Back, Start, DUp, DDown, DLeft, DRight, ButtonCount
  };

  static Maybe<Button> mapButton(ControllerButton button);

  AimContext contextFor(PlayerPtr const& player) const;
  Vec2F computeAim(float dt, PlayerPtr const& player, WorldClientPtr const& world, Vec2F aimStick, Vec2F moveStick);
  void cycleActionBar(PlayerPtr const& player, int direction);
  void releaseHeldActions(PlayerPtr const& player);

  Settings m_settings;
  bool m_active = false;

  Vec2F m_rawLeft;
  Vec2F m_rawRight;
  float m_rawLT = 0.0f;
  float m_rawRT = 0.0f;
  Gamepad::ButtonTimer m_buttons[ButtonCount];

  AimMode m_aimMode = AimMode::Auto;
  AimContext m_aimContext = AimContext::Combat;
  Vec2F m_lastAimDir = Vec2F(1, 0);
  float m_aimDirAge = 1000.0f;
  Vec2F m_precisionOffset;
  Maybe<Vec2F> m_aimWorld;
  bool m_walkToggle = false;

  bool m_firingPrimary = false;
  bool m_firingAlt = false;
  bool m_interacting = false;

  Vec2I m_lastMousePos;
  bool m_haveMousePos = false;
};

}
