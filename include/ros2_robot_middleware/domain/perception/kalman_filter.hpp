#ifndef ROS2_ROBOT_MIDDLEWARE_KALMAN_FILTER_HPP_
#define ROS2_ROBOT_MIDDLEWARE_KALMAN_FILTER_HPP_

#include <cmath>
#include <cstddef>
#include <utility>

// ── Measurement models (pluggable via template policy) ───────────────
//
// Each model provides:
//   static constexpr int kMeasDim — measurement vector size
//   h(state) → measurement prediction ẑ
//   jacobian(state) → measurement Jacobian H (kMeasDim × kDim)

struct LinearMeasurement {
  static constexpr int kMeasDim = 2;

  static void h(const double state[4], double z_pred[2]) noexcept {
    z_pred[0] = state[0];  // x
    z_pred[1] = state[1];  // y
  }

  static void jacobian(const double /*state*/[4], double H[2 * 4]) noexcept {
    // H = [[1, 0, 0, 0],
    //      [0, 1, 0, 0]]
    for (int i = 0; i < 8; ++i) H[i] = 0.0;
    H[0] = 1.0;  // ∂h0/∂x  = 1
    H[5] = 1.0;  // ∂h1/∂y  = 1
  }
};

struct RangeBearingMeasurement {
  static constexpr int kMeasDim = 2;

  static void h(const double state[4], double z_pred[2]) noexcept {
    double x = state[0];
    double y = state[1];
    double r = std::sqrt(x * x + y * y);
    z_pred[0] = r;
    z_pred[1] = (r > 1e-9) ? std::atan2(y, x) : 0.0;
  }

  static void jacobian(const double state[4], double H[2 * 4]) noexcept {
    double x = state[0];
    double y = state[1];
    double r2 = x * x + y * y;
    double r  = std::sqrt(r2);
    double r3 = r2 * r;

    for (int i = 0; i < 8; ++i) H[i] = 0.0;

    if (r > 1e-9) {
      H[0] = x / r;          // ∂r/∂x
      H[1] = y / r;          // ∂r/∂y
      H[4] = -y / r2;        // ∂θ/∂x
      H[5] =  x / r2;        // ∂θ/∂y
    }
  }
};

// ── Extended Kalman Filter — 2D, constant-acceleration model ────────
//
// Template parameter MeasureModel: measurement model policy.
//   Default: LinearMeasurement — (x, y) from LiDAR cluster centroids.
//   Optional: RangeBearingMeasurement — (r, θ) from raw LiDAR returns.
//
// State (4D): [x, y, vx, vy]
// Prediction: linear constant-acceleration (F is always linear)
// Update:     nonlinear via MeasureModel::h() + Jacobian
//
// ADR-6 (EKF upgrade): replaces linear-only KF with pluggable measurement.
//   Benefits:
//     1. Range-bearing measurements natively supported (no Cartesian conversion)
//     2. Measurement model is a compile-time policy — zero runtime overhead
//     3. Linear model preserved as default — existing callers unchanged
//   Trade-offs:
//     Linearization error in range-bearing when r ≈ 0 (handled by 1e-9 guard)
//
// Numerical stability (v1.1.0, preserved):
//   - Joseph form covariance update — P always symmetric positive-definite
//   - Mahalanobis 3σ outlier rejection — rejects anomalous measurements

template <typename MeasureModel = LinearMeasurement>
class KalmanFilter2D {
public:
  static constexpr int kDim  = 4;                     // state dimension
  static constexpr int kMeas = MeasureModel::kMeasDim; // measurement dimension

  KalmanFilter2D() { reset(); }

  void reset() {
    for (double &v : state_) v = 0.0;
    for (double &v : cov_) v = 0.0;
    // Position uncertainty: 10m (large — accepts first measurement far from origin)
    // Velocity uncertainty: 1 m/s
    cov_[0] = 100.0;   // Pxx
    cov_[5] = 100.0;   // Pyy
    cov_[10] = 1.0;    // Pvxvx
    cov_[15] = 1.0;    // Pvyvy
    initialized_ = false;
  }

  // ── Prediction (linear, unchanged) ────────────────────────────────

  void predict(double dt, double ax, double ay) {
    // F = [[1, 0, dt, 0],
    //      [0, 1, 0, dt],
    //      [0, 0, 1,  0],
    //      [0, 0, 0,  1]]
    const double f[16] = {
      1.0, 0.0, dt,  0.0,
      0.0, 1.0, 0.0, dt,
      0.0, 0.0, 1.0, 0.0,
      0.0, 0.0, 0.0, 1.0,
    };

    // x = F·x + B·u
    double x_new[4] = {};
    for (int i = 0; i < kDim; ++i)
      for (int j = 0; j < kDim; ++j)
        x_new[i] += f[i * kDim + j] * state_[j];
    x_new[0] += 0.5 * dt * dt * ax;
    x_new[1] += 0.5 * dt * dt * ay;
    x_new[2] += dt * ax;
    x_new[3] += dt * ay;

    // P = F·P·F^T + Q
    double fp[16] = {};
    for (int i = 0; i < kDim; ++i)
      for (int j = 0; j < kDim; ++j)
        for (int k = 0; k < kDim; ++k)
          fp[i * kDim + j] += f[i * kDim + k] * cov_[k * kDim + j];

    for (int i = 0; i < kDim * kDim; ++i) cov_[i] = 0.0;
    for (int i = 0; i < kDim; ++i)
      for (int j = 0; j < kDim; ++j)
        for (int k = 0; k < kDim; ++k)
          cov_[i * kDim + j] += fp[i * kDim + k] * f[j * kDim + k];

    for (int i = 0; i < kDim; ++i) cov_[i * kDim + i] += kQdiag[i];
    for (int i = 0; i < kDim; ++i) state_[i] = x_new[i];
  }

  // ── EKF Update (nonlinear measurement via policy) ─────────────────
  //
  // z[0..kMeas-1]: measurement vector
  // R_diag[0..kMeas-1]: measurement noise variance per dimension
  //
  // Returns true if measurement accepted (passes Mahalanobis gate).

  bool update(const double z[kMeas], const double R_diag[kMeas]) {
    // 1. Predicted measurement: ẑ = h(x)
    double z_pred[kMeas];
    MeasureModel::h(state_, z_pred);

    // 2. Innovation: y = z - ẑ
    double y[kMeas];
    for (int i = 0; i < kMeas; ++i) y[i] = z[i] - z_pred[i];

    // 3. Measurement Jacobian: H = ∂h/∂x
    double H[kMeas * kDim];
    MeasureModel::jacobian(state_, H);

    // 4. Innovation covariance: S = H·P·H^T + R
    //    HP = H × P
    double hp[kMeas * kDim] = {};
    for (int i = 0; i < kMeas; ++i)
      for (int j = 0; j < kDim; ++j)
        for (int k = 0; k < kDim; ++k)
          hp[i * kDim + j] += H[i * kDim + k] * cov_[k * kDim + j];

    //    S = HP × H^T + R
    double S[kMeas * kMeas] = {};
    for (int i = 0; i < kMeas; ++i) {
      for (int j = 0; j < kMeas; ++j) {
        for (int k = 0; k < kDim; ++k)
          S[i * kMeas + j] += hp[i * kDim + k] * H[j * kDim + k];
        S[i * kMeas + j] += (i == j) ? R_diag[i] : 0.0;
      }
    }

    // 5. First update — initialize state from measurement directly.
    //    EKF Jacobian is singular at origin for nonlinear models (e.g. range-bearing).
    //    Direct init provides a valid linearization point for subsequent updates.
    if (!initialized_) {
      if (!init_from_measurement(z)) return false;
      initialized_ = true;
      // Recompute innovation + Jacobian with initialized state
      MeasureModel::h(state_, z_pred);
      for (int i = 0; i < kMeas; ++i) y[i] = z[i] - z_pred[i];
      MeasureModel::jacobian(state_, H);
      for (int i = 0; i < kMeas; ++i)
        for (int j = 0; j < kDim; ++j)
          for (int k = 0; k < kDim; ++k) {
            hp[i * kDim + j] = 0.0;
            hp[i * kDim + j] += H[i * kDim + k] * cov_[k * kDim + j];
          }
      for (int i = 0; i < kMeas; ++i)
        for (int j = 0; j < kMeas; ++j) {
          S[i * kMeas + j] = (i == j) ? R_diag[i] : 0.0;
          for (int k = 0; k < kDim; ++k)
            S[i * kMeas + j] += hp[i * kDim + k] * H[j * kDim + k];
        }
    }

    // 6. Mahalanobis gate
    if (!maha_check(y, S)) return false;

    // 6. Kalman gain: K = P·H^T·S⁻¹
    //    PH^T = P × H^T
    double pht[kDim * kMeas] = {};
    for (int i = 0; i < kDim; ++i)
      for (int j = 0; j < kMeas; ++j)
        for (int k = 0; k < kDim; ++k)
          pht[i * kMeas + j] += cov_[i * kDim + k] * H[j * kDim + k];

    //    K = PH^T × S⁻¹
    double K[kDim * kMeas] = {};
    if constexpr (kMeas == 2) {
      // 2×2 S inverse (closed form)
      double det = S[0] * S[3] - S[1] * S[2];
      if (std::abs(det) < 1e-12) return false;
      double inv_det = 1.0 / det;
      double Si[4] = {S[3] * inv_det, -S[1] * inv_det, -S[2] * inv_det, S[0] * inv_det};
      for (int i = 0; i < kDim; ++i)
        for (int j = 0; j < kMeas; ++j)
          for (int k = 0; k < kMeas; ++k)
            K[i * kMeas + j] += pht[i * kMeas + k] * Si[k * kMeas + j];
    } else {
      // Scalar S: K = PH^T / S
      double invS = 1.0 / S[0];
      for (int i = 0; i < kDim; ++i) K[i] = pht[i] * invS;
    }

    // 7. State update: x = x + K·y
    for (int i = 0; i < kDim; ++i)
      for (int j = 0; j < kMeas; ++j)
        state_[i] += K[i * kMeas + j] * y[j];

    // 8. Joseph form covariance: P = (I-KH)P(I-KH)^T + KRK^T
    joseph_update(K, H, R_diag);

    return true;
  }

  // ── Convenience: 2D linear update (backward-compatible) ────────────

  bool update(double zx, double zy, double rx = 0.1, double ry = 0.1) {
    static_assert(kMeas == 2, "Linear update requires 2D measurement model");
    double z[2] = {zx, zy};
    double R[2] = {rx, ry};
    return update(z, R);
  }

  // ── Accessors ──────────────────────────────────────────────────────

  double x()  const { return state_[0]; }
  double y()  const { return state_[1]; }
  double vx() const { return state_[2]; }
  double vy() const { return state_[3]; }
  const double *state() const { return state_; }
  const double *covariance() const { return cov_; }

private:
  // Initialize state from first measurement (bypasses singular Jacobian at origin)
  bool init_from_measurement(const double z[kMeas]) {
    if constexpr (kMeas == 2 && std::is_same_v<MeasureModel, LinearMeasurement>) {
      state_[0] = z[0];
      state_[1] = z[1];
      return true;
    }
    if constexpr (kMeas == 2 && std::is_same_v<MeasureModel, RangeBearingMeasurement>) {
      double r = z[0], theta = z[1];
      state_[0] = r * std::cos(theta);
      state_[1] = r * std::sin(theta);
      return (r > 0.0);
    }
    return false;  // fallback: can't init safely
  }

  // Mahalanobis gate: per-dimension |y_i| < 3 × sqrt(S_ii)
  bool maha_check(const double y[kMeas], const double S[kMeas * kMeas]) const {
    for (int i = 0; i < kMeas; ++i) {
      double sig = std::sqrt(S[i * kMeas + i]);
      if (sig > 0.0 && std::abs(y[i]) > kMahaThreshold * sig) return false;
    }
    return true;
  }

  // Joseph form: P = (I-KH)P(I-KH)^T + KRK^T
  void joseph_update(const double K[kDim * kMeas], const double H[kMeas * kDim],
                     const double R_diag[kMeas]) {
    // IKH = I - K×H
    double ikh[kDim * kDim] = {};
    for (int i = 0; i < kDim; ++i) {
      ikh[i * kDim + i] = 1.0;
      for (int j = 0; j < kDim; ++j)
        for (int k = 0; k < kMeas; ++k)
          ikh[i * kDim + j] -= K[i * kMeas + k] * H[k * kDim + j];
    }
    // temp = IKH × P
    double temp[kDim * kDim] = {};
    for (int i = 0; i < kDim; ++i)
      for (int j = 0; j < kDim; ++j)
        for (int k = 0; k < kDim; ++k)
          temp[i * kDim + j] += ikh[i * kDim + k] * cov_[k * kDim + j];
    // P_new = temp × IKH^T
    double p_new[kDim * kDim] = {};
    for (int i = 0; i < kDim; ++i)
      for (int j = 0; j < kDim; ++j)
        for (int k = 0; k < kDim; ++k)
          p_new[i * kDim + j] += temp[i * kDim + k] * ikh[j * kDim + k];
    // + KRK^T
    for (int i = 0; i < kDim; ++i)
      for (int j = 0; j < kDim; ++j)
        for (int k = 0; k < kMeas; ++k)
          p_new[i * kDim + j] += K[i * kMeas + k] * R_diag[k] * K[j * kMeas + k];

    for (int i = 0; i < kDim * kDim; ++i) cov_[i] = p_new[i];
  }

  double state_[kDim] = {};
  double cov_[kDim * kDim] = {};
  bool initialized_ = false;

  static constexpr double kMahaThreshold = 3.0;
  static constexpr double kQdiag[4] = {0.01, 0.01, 0.1, 0.1};
};

// ── Type aliases ─────────────────────────────────────────────────────
using KalmanFilter2D_Linear       = KalmanFilter2D<LinearMeasurement>;
using KalmanFilter2D_RangeBearing = KalmanFilter2D<RangeBearingMeasurement>;

#endif
