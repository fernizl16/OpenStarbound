#include "../frontend/gamepad/StarGamepadLogic.hpp"

#include "gtest/gtest.h"

#include <set>

using namespace Star;
using namespace Star::Gamepad;

TEST(GamepadTest, DeadzoneRemovesSmallInput) {
  StickSettings s;
  s.deadzone = 0.2f;
  EXPECT_EQ(shapeStick(Vec2F(0.1f, 0.1f), s), Vec2F());
  EXPECT_NE(shapeStick(Vec2F(0.5f, 0.0f), s), Vec2F());
}

TEST(GamepadTest, YAxisIsFlippedToWorldUp) {
  StickSettings s;
  s.curve = ResponseCurve::Linear;
  // SDL reports pushing up as negative Y.
  Vec2F up = shapeStick(Vec2F(0.0f, -1.0f), s);
  EXPECT_NEAR(up[1], 1.0f, 1e-4);
  EXPECT_NEAR(up[0], 0.0f, 1e-4);
}

TEST(GamepadTest, FullDeflectionMatchesSensitivity) {
  StickSettings s;
  s.curve = ResponseCurve::Linear;
  s.sensitivity = 1.5f;
  EXPECT_NEAR(shapeStick(Vec2F(1.0f, 0.0f), s).magnitude(), 1.5f, 1e-4);
  s.sensitivity = 0.5f;
  EXPECT_NEAR(shapeStick(Vec2F(1.0f, 0.0f), s).magnitude(), 0.5f, 1e-4);
}

TEST(GamepadTest, ProgressiveCurveIsFinerNearCenter) {
  StickSettings lin, prog;
  lin.curve = ResponseCurve::Linear;
  prog.curve = ResponseCurve::Progressive;
  float l = shapeStick(Vec2F(0.4f, 0.0f), lin).magnitude();
  float p = shapeStick(Vec2F(0.4f, 0.0f), prog).magnitude();
  EXPECT_LT(p, l);
  // Both reach full output at full deflection.
  EXPECT_NEAR(shapeStick(Vec2F(1.0f, 0.0f), prog).magnitude(), 1.0f, 1e-4);
}

TEST(GamepadTest, DiagonalOverflowIsNormalized) {
  StickSettings s;
  s.curve = ResponseCurve::Linear;
  EXPECT_LE(shapeStick(Vec2F(1.0f, 1.0f), s).magnitude(), 1.0001f);
}

TEST(GamepadTest, SwapSticks) {
  Vec2F l(1, 0), r(0, 1);
  auto normal = assignRoles(l, r, false);
  EXPECT_EQ(normal.move, l);
  EXPECT_EQ(normal.aim, r);
  auto swapped = assignRoles(l, r, true);
  EXPECT_EQ(swapped.move, r);
  EXPECT_EQ(swapped.aim, l);
}

namespace {
  // Flat ground at y <= 0, a wall at x >= 5.
  bool solid(Vec2I const& t) {
    return t[1] <= 0 || t[0] >= 5;
  }
}

TEST(GamepadTest, MineTargetsFirstSolidTileInDirection) {
  Vec2F origin(0.5f, 2.5f);
  auto right = findMineTarget(origin, Vec2F(1, 0), 10.0f, solid);
  ASSERT_TRUE(right.isValid());
  EXPECT_EQ(*right, Vec2I(5, 2));
  auto down = findMineTarget(origin, Vec2F(0, -1), 10.0f, solid);
  ASSERT_TRUE(down.isValid());
  EXPECT_EQ(*down, Vec2I(0, 0));
}

TEST(GamepadTest, MineRespectsRange) {
  Vec2F origin(0.5f, 2.5f);
  EXPECT_FALSE(findMineTarget(origin, Vec2F(1, 0), 3.0f, solid).isValid());
  EXPECT_FALSE(findMineTarget(origin, Vec2F(0, 1), 10.0f, solid).isValid());
}

TEST(GamepadTest, PlaceTargetsEmptyTileBeforeSolid) {
  Vec2F origin(0.5f, 2.5f);
  auto right = findPlaceTarget(origin, Vec2F(1, 0), 10.0f, solid);
  ASSERT_TRUE(right.isValid());
  EXPECT_EQ(*right, Vec2I(4, 2));
  // Straight down: the tile under the player's own tile is empty (y=1), above ground.
  auto down = findPlaceTarget(origin, Vec2F(0, -1), 10.0f, solid);
  ASSERT_TRUE(down.isValid());
  EXPECT_EQ(*down, Vec2I(0, 1));
}

TEST(GamepadTest, PlaceNeedsSomethingToAttachTo) {
  Vec2F origin(0.5f, 2.5f);
  auto up = findPlaceTarget(origin, Vec2F(0, 1), 6.0f, solid);
  EXPECT_FALSE(up.isValid());
}

TEST(GamepadTest, AimAssistBendsTowardTargetInCone) {
  List<Vec2F> targets = {Vec2F(10.0f, 1.0f)};
  Vec2F out = applyAimAssist(Vec2F(1, 0), targets, 0.3f, 20.0f, 1.0f);
  EXPECT_NEAR(out[0], Vec2F(10.0f, 1.0f).normalized()[0], 1e-4);
  EXPECT_NEAR(out[1], Vec2F(10.0f, 1.0f).normalized()[1], 1e-4);
}

TEST(GamepadTest, AimAssistIgnoresTargetsOutsideCone) {
  List<Vec2F> targets = {Vec2F(0.0f, 10.0f)};
  Vec2F out = applyAimAssist(Vec2F(1, 0), targets, 0.3f, 20.0f, 1.0f);
  EXPECT_NEAR(out[0], 1.0f, 1e-4);
  EXPECT_NEAR(out[1], 0.0f, 1e-4);
}

TEST(GamepadTest, AimAssistOffDoesNothing) {
  List<Vec2F> targets = {Vec2F(10.0f, 1.0f)};
  EXPECT_EQ(applyAimAssist(Vec2F(1, 0), targets, 0.3f, 20.0f, 0.0f), Vec2F(1, 0));
}

TEST(GamepadTest, ButtonTapAndHold) {
  ButtonTimer b;
  b.set(true);
  EXPECT_TRUE(b.pressedEdge);
  b.endFrame(0.1f);
  b.set(false);
  EXPECT_TRUE(b.tapped(0.25f));
  b.endFrame(0.016f);

  b.set(true);
  b.endFrame(0.5f);
  EXPECT_TRUE(b.holdTriggered(0.4f));
  EXPECT_FALSE(b.holdTriggered(0.4f));
  b.set(false);
  EXPECT_FALSE(b.tapped(0.25f));
}
