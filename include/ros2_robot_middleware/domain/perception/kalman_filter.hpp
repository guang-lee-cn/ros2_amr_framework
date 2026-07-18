#ifndef ROS2_ROBOT_MIDDLEWARE_KALMAN_FILTER_HPP_
#define ROS2_ROBOT_MIDDLEWARE_KALMAN_FILTER_HPP_

#include <cmath>

// 2D 线性卡尔曼滤波器 — 恒加速度模型，商用级实现
//
// 状态向量 (4D): [x, y, vx, vy]
//   位置由 LiDAR 观测，速度由 IMU 加速度驱动
//
// ADR-9 选型结论：
//   评估了 robot_localization (EKF)、FusionCore (23-state UKF)、Fuse (因子图)
//   三种商用方案。对于 2D 仓库 AMR，4 状态线性 KF 是最优选择：
//   - robot_localization 的 15 维 EKF 不估计 sensor bias，且需要 sudo 安装
//   - FusionCore 的 23 维 UKF 针对室外 GPS+IMU bias 估计场景，仓库 AMR 用不上
//   - 保留 4 状态模型，修复数值稳定性/野值剔除后即可满足商用需求
//   - 如需 GPS/多传感器 bias 估计，推荐切换 FusionCore (Apache 2.0, 纯 C++17)
//
// 商用级改进（v1.1.0）：
//   1. Joseph 形式协方差更新 — 保证 P 始终对称正定
//   2. Mahalanobis 距离野值剔除 — 拒绝 >3σ 的异常观测
//   3. 独立更新 x/y 维度，保持计算高效

class KalmanFilter2D {
public:
  static constexpr int kDim  = 4;   // state: [x, y, vx, vy]
  static constexpr int kMeas = 2;   // measurement: [x, y]

  KalmanFilter2D() { reset(); }

  void reset() {
    for (double &v : state_) v = 0.0;
    for (double &v : cov_) v = 0.0;
    for (int i = 0; i < kDim; ++i) cov_[i * kDim + i] = 1.0;
  }

  // 预测步 — dt 秒, ax/ay 为 IMU 加速度 (m/s²)
  void predict(double dt, double ax, double ay) {
    // F: 状态转移矩阵 (4x4, 恒加速度)
    const double f[16] = {
      1.0, 0.0, dt,  0.0,
      0.0, 1.0, 0.0, dt,
      0.0, 0.0, 1.0, 0.0,
      0.0, 0.0, 0.0, 1.0,
    };

    // x = F·x + B·u
    double x_new[4] = {};
    for (int i = 0; i < kDim; ++i) {
      for (int j = 0; j < kDim; ++j) { x_new[i] += f[i * kDim + j] * state_[j]; }
    }
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

  // 更新步 — zx/zy 为 LiDAR 聚类中心 (x,y)，返回是否接受了该测量
  //
  // 商用改进 1: Mahalanobis 距离野值剔除
  //   如果 |z - Hx| > kMahalanobisThreshold × sqrt(P_ii + R)，拒绝更新。
  //   防止镜面反射/噪点产生的异常聚类中心污染状态估计。
  //
  // 商用改进 2: Joseph 形式协方差更新
  //   P = (I - KH)P(I - KH)^T + KRK^T，保证 P 始终对称正定，
  //   避免标准形式 P = (I - KH)P 在浮点累积误差下发散。
  //
  // 返回 true 表示测量被接受，false 表示被野值剔除拒绝。
  bool update(double zx, double zy, double measurement_noise = 0.1) {
    // ── 独立更新 x 维度 ──────────────────────────────────────
    bool x_ok = update_dimension(0, zx, measurement_noise);
    // ── 独立更新 y 维度 ──────────────────────────────────────
    bool y_ok = update_dimension(1, zy, measurement_noise);

    return x_ok && y_ok;
  }

  double x()  const { return state_[0]; }
  double y()  const { return state_[1]; }
  double vx() const { return state_[2]; }
  double vy() const { return state_[3]; }

private:
  static constexpr double kMahalanobisThreshold = 3.0;  // 3σ 门限

  bool update_dimension(int idx, double z, double R) {
    // 残差 y = z - Hx (H 取对应行)
    double y = z - state_[idx];

    // 新息协方差 S = P[idx][idx] + R
    double S = cov_[idx * kDim + idx] + R;

    // Mahalanobis 距离 = y / sqrt(S)
    // 如果 |y| > 3 × sqrt(S)，测量为野值，拒绝更新
    if (std::abs(y) > kMahalanobisThreshold * std::sqrt(S)) {
      return false;
    }

    // 卡尔曼增益 K = P[:, idx] / S
    double K[4] = {};
    for (int i = 0; i < kDim; ++i) { K[i] = cov_[i * kDim + idx] / S; }

    // 状态更新 x = x + K·y
    for (int i = 0; i < kDim; ++i) { state_[i] += K[i] * y; }

    // Joseph 形式协方差更新: P = (I - KH)P(I - KH)^T + KRK^T
    //
    // 等价于两步:
    //   Step A: 中间矩阵 A = I - KH, 再算 A·P·A^T
    //   Step B: + KRK^T (K × R × K^T，只有 idx 维度非零)
    //
    // 先算 A·P（A 的第 i 行 = δ(i,row) - K[row]·H[col]）
    double ap[16] = {};
    for (int row = 0; row < kDim; ++row) {
      for (int col = 0; col < kDim; ++col) {
        ap[row * kDim + col] = cov_[row * kDim + col];
      }
      // A = I - K·H, H 的第 idx 列为 1
      ap[row * kDim + idx] -= K[row] * cov_[idx * kDim + idx];  // ≈ K·H·P
    }
    // 更精确: A = I - K*H (H 只有 (idx,idx) 为 1)
    // A·P = P - K·(H 的第 idx 行)·P
    // 重写：对每个 col，A·P[row][col] = P[row][col] - K[row] * P[idx][col]

    double apa[16] = {};
    for (int row = 0; row < kDim; ++row) {
      for (int col = 0; col < kDim; ++col) {
        double sum = 0.0;
        for (int k = 0; k < kDim; ++k) {
          double a_rk = (row == k ? 1.0 : 0.0) - K[row] * (k == idx ? 1.0 : 0.0);
          sum += a_rk * cov_[k * kDim + col];
        }
        apa[row * kDim + col] = sum;
      }
    }

    // (A·P)·A^T
    double p_new[16] = {};
    for (int row = 0; row < kDim; ++row) {
      for (int col = 0; col < kDim; ++col) {
        double sum = 0.0;
        for (int k = 0; k < kDim; ++k) {
          double a_colk = (col == k ? 1.0 : 0.0) - K[col] * (k == idx ? 1.0 : 0.0);
          sum += apa[row * kDim + k] * a_colk;
        }
        p_new[row * kDim + col] = sum;
      }
    }

    // + KRK^T (K × R × K^T = R × K × K^T，因为 R 是标量)
    for (int row = 0; row < kDim; ++row) {
      for (int col = 0; col < kDim; ++col) {
        p_new[row * kDim + col] += K[row] * R * K[col];
      }
    }

    for (int i = 0; i < kDim * kDim; ++i) cov_[i] = p_new[i];
    return true;
  }

  double state_[kDim] = {};
  double cov_[kDim * kDim] = {};

  // 过程噪声 Q 对角元素 (越小 = 越信任模型预测)
  static constexpr double kQdiag[4] = {0.01, 0.01, 0.1, 0.1};
};

#endif
