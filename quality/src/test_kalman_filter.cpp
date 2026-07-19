/// @file test_kalman_filter.cpp — KF + EKF unit tests (no ROS2 dependency)
#include "ros2_robot_middleware/domain/perception/kalman_filter.hpp"

#include <cmath>
#include <gtest/gtest.h>

// ── Linear KF (backward-compatible default) ─────────────────────────

TEST(KalmanFilterTest, Linear_ConvergesToConstantPosition) {
  KalmanFilter2D<> kf;
  for (int i = 0; i < 20; ++i) {
    kf.predict(0.1, 0.0, 0.0);
    kf.update(5.0, 10.0);
  }
  EXPECT_NEAR(kf.x(), 5.0, 0.2);
  EXPECT_NEAR(kf.y(), 10.0, 0.2);
}

TEST(KalmanFilterTest, Linear_TracksMovingTarget) {
  KalmanFilter2D<> kf;
  // Object moves at vx=1.0, vy=0.5
  double true_x = 0.0, true_y = 0.0;
  for (int i = 0; i < 30; ++i) {
    true_x += 0.1 * 1.0;  // dt * vx
    true_y += 0.1 * 0.5;
    kf.predict(0.1, 0.0, 0.0);
    // Add ±0.05 noise to simulate LiDAR
    kf.update(true_x + (i % 3 - 1) * 0.05, true_y + (i % 5 - 2) * 0.05);
  }
  EXPECT_NEAR(kf.x(), true_x, 0.2);
  EXPECT_NEAR(kf.y(), true_y, 0.2);
  // Velocity estimate should be close to ground truth
  EXPECT_NEAR(kf.vx(), 1.0, 0.3);
  EXPECT_NEAR(kf.vy(), 0.5, 0.3);
}

TEST(KalmanFilterTest, Linear_MahalanobisRejectsOutlier) {
  KalmanFilter2D<> kf;
  // Converge first
  for (int i = 0; i < 10; ++i) {
    kf.predict(0.1, 0.0, 0.0);
    kf.update(3.0, 4.0);
  }
  double x_before = kf.x();

  // Inject massive outlier — should be rejected
  bool accepted = kf.update(100.0, 4.0);
  EXPECT_FALSE(accepted);
  EXPECT_NEAR(kf.x(), x_before, 0.01);  // state unchanged
}

TEST(KalmanFilterTest, Linear_JosephFormSymmetry) {
  KalmanFilter2D<> kf;
  kf.predict(1.0, 0.1, 0.1);
  kf.update(2.0, 3.0);

  const double *P = kf.covariance();
  for (int i = 0; i < 4; ++i)
    for (int j = i + 1; j < 4; ++j)
      EXPECT_NEAR(P[i * 4 + j], P[j * 4 + i], 1e-12)
          << "Covariance not symmetric at (" << i << "," << j << ")";
}

// ── EKF Range-Bearing ───────────────────────────────────────────────

TEST(EkfTest, RangeBearing_ConvergesToStaticTarget) {
  KalmanFilter2D<RangeBearingMeasurement> kf;
  // Target at (3.0, 4.0) → range=5.0, bearing≈0.927 rad
  double true_range = 5.0;
  double true_bearing = std::atan2(4.0, 3.0);

  for (int i = 0; i < 30; ++i) {
    kf.predict(0.1, 0.0, 0.0);
    double z[2] = {true_range + (i % 3 - 1) * 0.02,
                   true_bearing + (i % 5 - 2) * 0.005};
    double R[2] = {0.01, 1e-4};  // range noise=0.1m², bearing noise=0.01rad²
    kf.update(z, R);
  }

  EXPECT_NEAR(kf.x(), 3.0, 0.15);
  EXPECT_NEAR(kf.y(), 4.0, 0.15);
}

TEST(EkfTest, RangeBearing_TracksMovingTarget) {
  KalmanFilter2D<RangeBearingMeasurement> kf;
  double true_x = 1.0, true_y = 0.0;
  const double vx = 0.5, vy = 0.3;

  for (int i = 0; i < 40; ++i) {
    true_x += 0.1 * vx;
    true_y += 0.1 * vy;
    double r = std::sqrt(true_x * true_x + true_y * true_y);
    double b = std::atan2(true_y, true_x);

    kf.predict(0.1, 0.0, 0.0);
    double z[2] = {r + (i % 3 - 1) * 0.01, b + (i % 7 - 3) * 0.003};
    double R[2] = {0.01, 1e-4};
    kf.update(z, R);
  }

  EXPECT_NEAR(kf.x(), true_x, 0.2);
  EXPECT_NEAR(kf.y(), true_y, 0.2);
}

TEST(EkfTest, RangeBearing_OriginGuard) {
  // When target is at origin (r≈0), Jacobian has singularity.
  // Implementation should guard with r > 1e-9 check.
  KalmanFilter2D<RangeBearingMeasurement> kf;
  kf.predict(0.1, 0.0, 0.0);

  // Measurement at origin — range=0, bearing undefined
  double z[2] = {0.0, 0.0};
  double R[2] = {0.01, 1e-4};
  // Should not crash, just accept or reject based on Mahalanobis
  kf.update(z, R);
  // State should be near origin
  EXPECT_NEAR(kf.x(), 0.0, 2.0);
}

TEST(EkfTest, LinearVsRangeBearing_Consistent) {
  // Same target, same dt, same noise — linear and range-bearing should converge similarly
  KalmanFilter2D<> kf_lin;
  KalmanFilter2D<RangeBearingMeasurement> kf_rb;

  double true_x = 5.0, true_y = -3.0;

  for (int i = 0; i < 25; ++i) {
    // Linear: direct (x, y) measurement with noise
    kf_lin.predict(0.1, 0.0, 0.0);
    kf_lin.update(true_x + (i % 3 - 1) * 0.03, true_y + (i % 5 - 2) * 0.03);

    // Range-bearing: convert to (r, θ) then back through EKF
    double r = std::sqrt(true_x * true_x + true_y * true_y);
    double b = std::atan2(true_y, true_x);
    kf_rb.predict(0.1, 0.0, 0.0);
    double z[2] = {r + (i % 3 - 1) * 0.03, b + (i % 7 - 3) * 0.005};
    double R[2] = {0.001, 2.5e-5};  // tighter noise for fair comparison
    kf_rb.update(z, R);
  }

  // Both should converge near true position
  EXPECT_NEAR(kf_lin.x(), true_x, 0.2);
  EXPECT_NEAR(kf_lin.y(), true_y, 0.2);
  EXPECT_NEAR(kf_rb.x(), true_x, 0.3);
  EXPECT_NEAR(kf_rb.y(), true_y, 0.3);
}
