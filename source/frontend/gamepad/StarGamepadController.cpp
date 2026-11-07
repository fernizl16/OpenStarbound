#include "StarGamepadController.hpp"
#include "StarRoot.hpp"
#include "StarConfiguration.hpp"
#include "StarJsonExtra.hpp"
#include "StarPlayer.hpp"
#include "StarPlayerInventory.hpp"
#include "StarWorldClient.hpp"
#include "StarMainInterface.hpp"
#include "StarMainInterfaceTypes.hpp"
#include "StarMaterialItem.hpp"
#include "StarObjectItem.hpp"
#include "StarTools.hpp"

namespace Star {

namespace {
  String const ConfigKey = "starpad";

  Gamepad::StickSettings stickFromJson(Json const& j, Gamepad::StickSettings def) {
    if (!j.isType(Json::Type::Object))
      return def;
    def.deadzone = clamp(j.getFloat("deadzone", def.deadzone), 0.0f, 0.3f);
    def.sensitivity = clamp(j.getFloat("sensitivity", def.sensitivity), 0.5f, 2.0f);
    def.curve = j.getString("curve", def.curve == Gamepad::ResponseCurve::Linear ? "linear" : "progressive") == "linear"
        ? Gamepad::ResponseCurve::Linear : Gamepad::ResponseCurve::Progressive;
    return def;
  }

  Json stickToJson(Gamepad::StickSettings const& s) {
    return JsonObject{
      {"deadzone", s.deadzone},
      {"sensitivity", s.sensitivity},
      {"curve", s.curve == Gamepad::ResponseCurve::Linear ? "linear" : "progressive"}
    };
  }

  String aimAssistName(GamepadController::AimAssist a) {
    switch (a) {
      case GamepadController::AimAssist::Off: return "off";
      case GamepadController::AimAssist::Strong: return "strong";
      default: return "soft";
    }
  }

  float aimAssistStrength(GamepadController::AimAssist a) {
    switch (a) {
      case GamepadController::AimAssist::Off: return 0.0f;
      case GamepadController::AimAssist::Strong: return 0.85f;
      default: return 0.45f;
    }
  }
}

GamepadController::Settings GamepadController::Settings::fromJson(Json const& json) {
  Settings s;
  if (!json.isType(Json::Type::Object))
    return s;
  s.enabled = json.getBool("enabled", s.enabled);
  s.swapSticks = json.getBool("swapSticks", s.swapSticks);
  s.moveStick = stickFromJson(json.get("moveStick", {}), s.moveStick);
  s.aimStick = stickFromJson(json.get("aimStick", {}), s.aimStick);
  String assist = json.getString("aimAssist", "soft");
  s.aimAssist = assist == "off" ? AimAssist::Off : assist == "strong" ? AimAssist::Strong : AimAssist::Soft;
  s.autoAimRange = clamp(json.getFloat("autoAimRange", s.autoAimRange), 3.0f, 30.0f);
  s.precisionSpeed = clamp(json.getFloat("precisionSpeed", s.precisionSpeed), 4.0f, 80.0f);
  s.triggerThreshold = clamp(json.getFloat("triggerThreshold", s.triggerThreshold), 0.05f, 0.95f);
  return s;
}

Json GamepadController::Settings::toJson() const {
  return JsonObject{
    {"enabled", enabled},
    {"swapSticks", swapSticks},
    {"moveStick", stickToJson(moveStick)},
    {"aimStick", stickToJson(aimStick)},
    {"aimAssist", aimAssistName(aimAssist)},
    {"autoAimRange", autoAimRange},
    {"precisionSpeed", precisionSpeed},
    {"triggerThreshold", triggerThreshold}
  };
}

GamepadController::GamepadController() {}

void GamepadController::loadSettings() {
  auto config = Root::singleton().configuration();
  m_settings = Settings::fromJson(config->get(ConfigKey, {}));
  // Write back so every option is visible and editable in starbound.config.
  config->set(ConfigKey, m_settings.toJson());
}

void GamepadController::saveSettings() {
  Root::singleton().configuration()->set(ConfigKey, m_settings.toJson());
}

GamepadController::Settings const& GamepadController::settings() const {
  return m_settings;
}

void GamepadController::setSettings(Settings const& settings) {
  m_settings = settings;
  saveSettings();
}

Maybe<GamepadController::Button> GamepadController::mapButton(ControllerButton button) {
  switch (button) {
    case ControllerButton::A: return A;
    case ControllerButton::B: return B;
    case ControllerButton::X: return X;
    case ControllerButton::Y: return Y;
    case ControllerButton::LeftShoulder: return LB;
    case ControllerButton::RightShoulder: return RB;
    case ControllerButton::LeftStick: return LS;
    case ControllerButton::RightStick: return RS;
    case ControllerButton::Back: return Back;
    case ControllerButton::Start: return Start;
    case ControllerButton::DPadUp: return DUp;
    case ControllerButton::DPadDown: return DDown;
    case ControllerButton::DPadLeft: return DLeft;
    case ControllerButton::DPadRight: return DRight;
    default: return {};
  }
}

void GamepadController::handleInputEvent(InputEvent const& event) {
  if (!m_settings.enabled)
    return;

  if (auto axis = event.ptr<ControllerAxisEvent>()) {
    float v = axis->controllerAxisValue;
    switch (axis->controllerAxis) {
      case ControllerAxis::LeftX: m_rawLeft[0] = v; break;
      case ControllerAxis::LeftY: m_rawLeft[1] = v; break;
      case ControllerAxis::RightX: m_rawRight[0] = v; break;
      case ControllerAxis::RightY: m_rawRight[1] = v; break;
      case ControllerAxis::TriggerLeft: m_rawLT = v; break;
      case ControllerAxis::TriggerRight: m_rawRT = v; break;
      default: break;
    }
    float dz = max(m_settings.moveStick.deadzone, m_settings.aimStick.deadzone);
    if (m_rawLeft.magnitude() > dz || m_rawRight.magnitude() > dz
        || m_rawLT > m_settings.triggerThreshold || m_rawRT > m_settings.triggerThreshold)
      m_active = true;
  } else if (auto down = event.ptr<ControllerButtonDownEvent>()) {
    if (auto b = mapButton(down->controllerButton)) {
      m_buttons[*b].set(true);
      m_active = true;
    }
  } else if (auto up = event.ptr<ControllerButtonUpEvent>()) {
    if (auto b = mapButton(up->controllerButton))
      m_buttons[*b].set(false);
  } else if (auto mouseMove = event.ptr<MouseMoveEvent>()) {
    // Real mouse movement hands control back to the mouse. Small jitter is ignored.
    Vec2I pos = Vec2I::round(mouseMove->mousePosition);
    if (m_haveMousePos && (pos - m_lastMousePos).magnitude() > 6.0f)
      m_active = false;
    m_lastMousePos = pos;
    m_haveMousePos = true;
  } else if (event.is<MouseButtonDownEvent>()) {
    m_active = false;
  }
}

bool GamepadController::active() const {
  return m_settings.enabled && m_active;
}

Vec2F GamepadController::moveVector() const {
  auto roles = Gamepad::assignRoles(
      Gamepad::shapeStick(m_rawLeft, m_settings.moveStick),
      Gamepad::shapeStick(m_rawRight, m_settings.moveStick),
      m_settings.swapSticks);
  return Gamepad::clampMagnitude(roles.move, 1.0f);
}

GamepadController::AimMode GamepadController::aimMode() const {
  return m_aimMode;
}

GamepadController::AimContext GamepadController::aimContext() const {
  return m_aimContext;
}

Maybe<Vec2F> GamepadController::aimWorldPosition() const {
  return m_aimWorld;
}

GamepadController::AimContext GamepadController::contextFor(PlayerPtr const& player) const {
  auto item = player->primaryHandItem();
  if (!item)
    return AimContext::Combat;
  if (as<MaterialItem>(item) || as<ObjectItem>(item))
    return AimContext::Place;
  if (as<BeamMiningTool>(item) || as<MiningTool>(item) || as<PaintingBeamTool>(item) || as<TillingTool>(item))
    return AimContext::Mine;
  return AimContext::Combat;
}

Vec2F GamepadController::computeAim(float dt, PlayerPtr const& player, WorldClientPtr const& world, Vec2F aimStick, Vec2F moveStick) {
  Vec2F origin = player->position();

  if (m_aimMode == AimMode::Precision) {
    m_precisionOffset += aimStick * m_settings.precisionSpeed * dt;
    m_precisionOffset = Gamepad::clampMagnitude(m_precisionOffset, m_settings.precisionMaxOffset);
    return origin + m_precisionOffset;
  }

  // Direction: aim stick if deflected, else the last aim direction for a short
  // while, else the movement direction (mining while walking), else facing.
  Vec2F dir;
  if (aimStick.magnitudeSquared() > 0.0f) {
    dir = aimStick.normalized();
    m_lastAimDir = dir;
    m_aimDirAge = 0.0f;
  } else {
    m_aimDirAge += dt;
    if (m_aimDirAge <= m_settings.facingMemory)
      dir = m_lastAimDir;
    else if (m_aimContext != AimContext::Combat && moveStick.magnitudeSquared() > 0.0f)
      dir = moveStick.normalized();
    else
      dir = Vec2F((float)numericalDirection(player->facingDirection()), 0.0f);
  }

  float toolRange = max(1.0f, player->toolRadius() - 0.5f);
  auto isSolid = [world](Vec2I const& t) {
    return world->tileIsOccupied(t, TileLayer::Foreground);
  };

  if (m_aimContext == AimContext::Mine) {
    if (auto tile = Gamepad::findMineTarget(origin, dir, toolRange, isSolid))
      return centerOfTile(*tile);
    return origin + dir * toolRange;
  }

  if (m_aimContext == AimContext::Place) {
    if (auto tile = Gamepad::findPlaceTarget(origin, dir, toolRange, isSolid))
      return centerOfTile(*tile);
    return origin + dir * min(2.0f, toolRange);
  }

  // Combat: aim assist toward hostile actors near the aim direction.
  float strength = aimAssistStrength(m_settings.aimAssist);
  if (strength > 0.0f) {
    float range = m_settings.autoAimRange * 2.5f;
    auto team = player->getTeam();
    auto targets = world->query<Entity>(RectF::withCenter(origin, Vec2F(range * 2, range * 2)),
        [&](EntityPtr const& e) {
          auto type = e->entityType();
          return (type == EntityType::Monster || type == EntityType::Npc || type == EntityType::Player)
              && e->entityId() != player->entityId()
              && team.canDamage(e->getTeam(), false);
        });
    List<Vec2F> rel;
    for (auto const& t : targets)
      rel.append(world->geometry().diff(t->position(), origin));
    dir = Gamepad::applyAimAssist(dir, rel, 15.0f * Constants::pi / 180.0f, range, strength);
  }
  return origin + dir * m_settings.autoAimRange;
}

void GamepadController::cycleActionBar(PlayerPtr const& player, int direction) {
  // Same ordering as the mouse wheel in ActionBar: left custom slots,
  // essentials, right custom slots.
  auto inventory = player->inventory();
  int customBarIndexes = inventory->customBarIndexes();
  int total = customBarIndexes + (int)EssentialItemCount;
  auto abl = inventory->selectedActionBarLocation();

  int index = 0;
  if (!abl) {
    index = direction > 0 ? 0 : total - 1;
  } else {
    if (auto cbi = abl.ptr<CustomBarIndex>())
      index = *cbi < customBarIndexes / 2 ? (int)*cbi : (int)*cbi + (int)EssentialItemCount;
    else
      index = customBarIndexes / 2 + (int)abl.get<EssentialItem>();
    index = pmod(index + direction, total);
  }

  if (index < customBarIndexes / 2)
    abl = (CustomBarIndex)index;
  else if (index < customBarIndexes / 2 + (int)EssentialItemCount)
    abl = (EssentialItem)(index - customBarIndexes / 2);
  else
    abl = (CustomBarIndex)(index - (int)EssentialItemCount);

  inventory->selectActionBarLocation(abl);
}

void GamepadController::releaseHeldActions(PlayerPtr const& player) {
  if (m_firingPrimary) {
    player->endPrimaryFire();
    m_firingPrimary = false;
  }
  if (m_firingAlt) {
    player->endAltFire();
    m_firingAlt = false;
  }
  if (m_interacting) {
    player->endTrigger();
    m_interacting = false;
  }
}

void GamepadController::update(float dt, PlayerPtr const& player, WorldClientPtr const& world, MainInterface* mainInterface) {
  // Triggers are analog; expose them as buttons.
  m_buttons[LT].set(m_rawLT > m_settings.triggerThreshold);
  m_buttons[RT].set(m_rawRT > m_settings.triggerThreshold);

  auto finish = [&]() {
    for (auto& b : m_buttons)
      b.endFrame(dt);
  };

  if (!active() || !player || !world || !mainInterface) {
    if (player)
      releaseHeldActions(player);
    m_aimWorld = {};
    if (mainInterface)
      mainInterface->setGamepadAim({});
    finish();
    return;
  }

  auto paneManager = mainInterface->paneManager();
  bool windowOpen = mainInterface->windowsOpen();
  bool textFocus = mainInterface->inputFocus();

  // Menu-level buttons work with or without windows open.
  if (m_buttons[Start].pressedEdge)
    paneManager->toggleRegisteredPane(MainInterfacePanes::EscapeDialog);

  if (windowOpen || textFocus) {
    releaseHeldActions(player);
    // Phase 1: B or Y closes the top window. Widget focus navigation is Phase 3.
    if (!textFocus && (m_buttons[B].pressedEdge || m_buttons[Y].pressedEdge)) {
      if (auto pane = paneManager->topPane({PaneLayer::ModalWindow, PaneLayer::Window}))
        paneManager->dismissPane(pane);
    }
    m_aimWorld = {};
    mainInterface->setGamepadAim({});
    finish();
    return;
  }

  auto shapedLeft = Gamepad::shapeStick(m_rawLeft, m_settings.swapSticks ? m_settings.aimStick : m_settings.moveStick);
  auto shapedRight = Gamepad::shapeStick(m_rawRight, m_settings.swapSticks ? m_settings.moveStick : m_settings.aimStick);
  auto roles = Gamepad::assignRoles(shapedLeft, shapedRight, m_settings.swapSticks);
  Vec2F move = Gamepad::clampMagnitude(roles.move, 1.0f);

  // Movement: only the movement stick moves the character.
  if (move[0] != 0.0f)
    player->setMoveVector(Vec2F(move[0], 0.0f));
  if (move[1] > 0.5f)
    player->moveUp();
  else if (move[1] < -0.5f)
    player->moveDown();

  if (m_buttons[LS].pressedEdge)
    m_walkToggle = !m_walkToggle;
  if (m_walkToggle)
    player->setShifting(true);

  if (m_buttons[A].held)
    player->jump();
  if (m_buttons[B].held)
    player->special(1);

  if (m_buttons[X].pressedEdge) {
    player->beginTrigger();
    m_interacting = true;
  } else if (m_interacting && !m_buttons[X].held) {
    player->endTrigger();
    m_interacting = false;
  }

  if (m_buttons[LB].pressedEdge)
    cycleActionBar(player, -1);
  if (m_buttons[RB].pressedEdge)
    cycleActionBar(player, 1);

  if (m_buttons[Y].pressedEdge)
    paneManager->toggleRegisteredPane(MainInterfacePanes::Inventory);
  if (m_buttons[Back].pressedEdge)
    mainInterface->togglePlainCraftingWindow();

  // R3 tap toggles Auto / Precision. Entering Precision starts the free cursor
  // where the auto aim currently is, so the switch is seamless.
  if (m_buttons[RS].tapped(m_settings.tapTime)) {
    if (m_aimMode == AimMode::Auto) {
      m_aimMode = AimMode::Precision;
      if (m_aimWorld)
        m_precisionOffset = world->geometry().diff(*m_aimWorld, player->position());
    } else {
      m_aimMode = AimMode::Auto;
    }
  }

  m_aimContext = contextFor(player);
  m_aimWorld = computeAim(dt, player, world, roles.aim, move);
  mainInterface->setGamepadAim(m_aimWorld);

  // Fire after aiming so the first shot already goes where intended.
  if (m_buttons[RT].pressedEdge) {
    player->beginPrimaryFire();
    m_firingPrimary = true;
  } else if (m_firingPrimary && !m_buttons[RT].held) {
    player->endPrimaryFire();
    m_firingPrimary = false;
  }
  if (m_buttons[LT].pressedEdge) {
    player->beginAltFire();
    m_firingAlt = true;
  } else if (m_firingAlt && !m_buttons[LT].held) {
    player->endAltFire();
    m_firingAlt = false;
  }

  finish();
}

}
