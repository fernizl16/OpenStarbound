#pragma once

// StarPad: pure gamepad logic (no engine state), unit-testable in isolation.
// Stick shaping (deadzone, curve, sensitivity), stick role swapping, and
// tile/target selection along an aim direction.

#include "StarVector.hpp"
#include "StarMaybe.hpp"
#include "StarList.hpp"

#include <algorithm>
#include <functional>
#include <cmath>

namespace Star {
namespace Gamepad {

enum class ResponseCurve { Linear, Progressive };

struct StickSettings {
  float deadzone = 0.12f;     // 0..0.3 radial deadzone
  float sensitivity = 1.0f;   // 0.5..2.0
  ResponseCurve curve = ResponseCurve::Progressive;
};

// Converts a raw SDL stick (x right, y DOWN, each in [-1, 1]) to a shaped
// vector in world orientation (y UP). The magnitude after the deadzone is
// rescaled to [0, 1], bent by the response curve and multiplied by the
// sensitivity. The result is NOT clamped: callers that need a unit-range
// value (movement) clamp it themselves; aim speed may exceed 1.
inline Vec2F shapeStick(Vec2F raw, StickSettings const& s) {
  Vec2F v(raw[0], -raw[1]);
  float mag = v.magnitude();
  if (mag > 1.0f) {
    v /= mag;
    mag = 1.0f;
  }
  float dz = std::clamp(s.deadzone, 0.0f, 0.95f);
  if (mag <= dz || mag <= 0.0f)
    return Vec2F();
  float t = (mag - dz) / (1.0f - dz);
  if (s.curve == ResponseCurve::Progressive)
    t = std::pow(t, 1.7f);
  float out = t * std::clamp(s.sensitivity, 0.1f, 4.0f);
  return v / mag * out;
}

inline Vec2F clampMagnitude(Vec2F v, float maxMag) {
  float m = v.magnitude();
  if (m > maxMag && m > 0.0f)
    return v / m * maxMag;
  return v;
}

struct StickRoles {
  Vec2F move;
  Vec2F aim;
};

// The "swap sticks" option: move with the right stick, aim/navigate with the left.
inline StickRoles assignRoles(Vec2F left, Vec2F right, bool swapSticks) {
  return swapSticks ? StickRoles{right, left} : StickRoles{left, right};
}

typedef std::function<bool(Vec2I const&)> TilePredicate;

inline Vec2I tileAt(Vec2F p) {
  return Vec2I((int)std::floor(p[0]), (int)std::floor(p[1]));
}

// Walks from origin along dir (normalized) up to range tiles and returns the
// first tile for which isSolid is true: the "auto cursor" mining target.
inline Maybe<Vec2I> findMineTarget(Vec2F origin, Vec2F dir, float range, TilePredicate const& isSolid) {
  if (dir.magnitudeSquared() <= 0.0f)
    return {};
  dir = dir.normalized();
  Vec2I last = tileAt(origin);
  if (isSolid(last))
    return last;
  float const step = 0.2f;
  for (float d = step; d <= range; d += step) {
    Vec2I t = tileAt(origin + dir * d);
    if (t == last)
      continue;
    last = t;
    if (isSolid(t))
      return t;
  }
  return {};
}

inline bool hasSolidNeighbor(Vec2I t, TilePredicate const& isSolid) {
  return isSolid(t + Vec2I(1, 0)) || isSolid(t + Vec2I(-1, 0)) || isSolid(t + Vec2I(0, 1)) || isSolid(t + Vec2I(0, -1));
}

// Placement target: the last empty tile before the ray hits something solid.
// If nothing solid is hit within range, the farthest empty tile along the ray
// that touches a solid tile (so blocks attach to existing terrain).
inline Maybe<Vec2I> findPlaceTarget(Vec2F origin, Vec2F dir, float range, TilePredicate const& isSolid) {
  if (dir.magnitudeSquared() <= 0.0f)
    return {};
  dir = dir.normalized();
  Vec2I last = tileAt(origin);
  Vec2I originTile = last;
  Maybe<Vec2I> lastEmpty;
  Maybe<Vec2I> lastAttachable;
  float const step = 0.2f;
  for (float d = step; d <= range; d += step) {
    Vec2I t = tileAt(origin + dir * d);
    if (t == last)
      continue;
    last = t;
    if (isSolid(t))
      return lastEmpty;
    // Never place inside the tile the player stands in.
    if (t == originTile)
      continue;
    lastEmpty = t;
    if (hasSolidNeighbor(t, isSolid))
      lastAttachable = t;
  }
  return lastAttachable;
}

// Soft aim assist: bends dir toward the target whose direction is closest to
// dir, if within coneRadians and maxRange. strength 0 = off, 1 = snap.
// targets are positions relative to the aim origin.
inline Vec2F applyAimAssist(Vec2F dir, List<Vec2F> const& relTargets, float coneRadians, float maxRange, float strength) {
  if (strength <= 0.0f || dir.magnitudeSquared() <= 0.0f)
    return dir;
  dir = dir.normalized();
  Maybe<Vec2F> best;
  float bestAngle = coneRadians;
  for (auto const& t : relTargets) {
    float dist = t.magnitude();
    if (dist <= 0.01f || dist > maxRange)
      continue;
    float a = dir.angleBetween(t);
    if (a <= bestAngle) {
      bestAngle = a;
      best = t.normalized();
    }
  }
  if (!best)
    return dir;
  Vec2F blended = dir * (1.0f - strength) + *best * strength;
  return blended.magnitudeSquared() > 0.0f ? blended.normalized() : dir;
}

// Tracks a digital button for tap / hold discrimination.
struct ButtonTimer {
  bool held = false;
  bool pressedEdge = false;
  bool releasedEdge = false;
  float heldTime = 0.0f;
  bool holdFired = false;

  void set(bool down) {
    if (down && !held) {
      pressedEdge = true;
      heldTime = 0.0f;
      holdFired = false;
    } else if (!down && held) {
      releasedEdge = true;
    }
    held = down;
  }

  // Call once per frame after reading edges.
  void endFrame(float dt) {
    pressedEdge = false;
    releasedEdge = false;
    if (held)
      heldTime += dt;
  }

  // True on the frame a short press is released.
  bool tapped(float maxTap) const {
    return releasedEdge && !holdFired && heldTime <= maxTap;
  }

  // True once, when the button has been held for at least minHold.
  bool holdTriggered(float minHold) {
    if (held && !holdFired && heldTime >= minHold) {
      holdFired = true;
      return true;
    }
    return false;
  }
};

}
}
