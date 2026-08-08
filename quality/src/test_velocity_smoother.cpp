/// @file test_velocity_smoother.cpp — VelocitySmoother time-domain acceleration limits
#include "ros2_robot_middleware/domain/execution/velocity_smoother.hpp"

#include <gtest/gtest.h>

using amr::domain::execution::VelocitySmoother;
using amr::domain::execution::Twist2D;

class VelocitySmootherTest : public ::testing::Test {
protected:
  VelocitySmoother::Params params_{0.5F, 0.8F, 1.0F};  // accel / decel / ang_accel
  VelocitySmoother smoother_{params_};
};

// ── GivenDesiredFasterThanAccel_WhenSmooth_LimitsLinearRate ────────────
// 0 → 1.0 m/s over dt=0.1: max change = 0.5 * 0.1 = 0.05.

TEST_F(VelocitySmootherTest,
       GivenDesiredFasterThanAccel_WhenSmooth_LimitsLinearRate) {
  auto out = smoother_.smooth({1.0F, 0.0F}, {0.0F, 0.0F}, 0.1F);
  EXPECT_NEAR(out.linear, 0.05F, 1e-4);
  EXPECT_FLOAT_EQ(out.angular, 0.0F);
}

// ── GivenDesiredSlowdown_WhenSmooth_AppliesMaxDecel ────────────────────
// 1.0 → 0 over dt=0.1: max change = 0.8 * 0.1 = 0.08 (braking faster than accel).

TEST_F(VelocitySmootherTest, GivenDesiredSlowdown_WhenSmooth_AppliesMaxDecel) {
  auto out = smoother_.smooth({0.0F, 0.0F}, {1.0F, 0.0F}, 0.1F);
  EXPECT_NEAR(out.linear, 0.92F, 1e-4);
}

// ── GivenAngularRequest_WhenSmooth_LimitsAngularRate ───────────────────
// 0 → 2.0 rad/s over dt=0.1: max = 1.0 * 0.1 = 0.1.

TEST_F(VelocitySmootherTest, GivenAngularRequest_WhenSmooth_LimitsAngularRate) {
  auto out = smoother_.smooth({0.0F, 2.0F}, {0.0F, 0.0F}, 0.1F);
  EXPECT_NEAR(out.angular, 0.1F, 1e-4);
}

// ── GivenSameVelocity_WhenSmooth_OutputsUnchanged ──────────────────────
TEST_F(VelocitySmootherTest, GivenSameVelocity_WhenSmooth_OutputsUnchanged) {
  auto out = smoother_.smooth({0.5F, 0.2F}, {0.5F, 0.2F}, 0.1F);
  EXPECT_FLOAT_EQ(out.linear, 0.5F);
  EXPECT_FLOAT_EQ(out.angular, 0.2F);
}

// ── GivenStopRequest_WhenSmooth_AppliesMaxDecel ────────────────────────
// 0.5 → 0 over dt=0.5: max change = 0.8 * 0.5 = 0.4 → out = 0.1.

TEST_F(VelocitySmootherTest, GivenStopRequest_WhenSmooth_AppliesMaxDecel) {
  auto out = smoother_.smooth({0.0F, 0.0F}, {0.5F, 0.0F}, 0.5F);
  EXPECT_NEAR(out.linear, 0.1F, 1e-4);
}

// ── GivenRepeatedSmoothing_WhenSmooth_AcceleratesGraduallyToFull ───────
// 0 → 0.5 m/s with max_accel 0.5 → 1 second (10 steps @ dt=0.1).

TEST_F(VelocitySmootherTest,
       GivenRepeatedSmoothing_WhenSmooth_AcceleratesGraduallyToFull) {
  Twist2D last{0.0F, 0.0F};
  for (int i = 0; i < 10; ++i) {
    last = smoother_.smooth({0.5F, 0.0F}, last, 0.1F);
  }
  EXPECT_NEAR(last.linear, 0.5F, 1e-3);
}
