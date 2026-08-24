#ifndef ROS2_ROBOT_MIDDLEWARE_DOMAIN_OTA_OTA_COORDINATOR_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DOMAIN_OTA_OTA_COORDINATOR_HPP_

/// @file   ota_coordinator.hpp
/// @brief  OTA 升级决策状态机（纯 Domain，零 ROS 依赖）。
///
/// 状态机：IDLE→DOWNLOADING→DOWNLOADED→INSTALLING→SWITCHING→HEALTH_GATE
///         →COMMITTED / ROLLED_BACK
///
/// 三个安全不变量（单测逐条锁定）：
///   I1 活动槽永不被写——刷写只指向非活动槽（A/B 分区的根基）
///   I2 签名/版本不通过 → 旧系统毫发无损（拒绝发生在触碰分区之前）
///   I3 健康门不通过 → 回滚到旧槽（新版本未提交前随时可退）
///
/// 防降级：candidate_version >= security_counter 才接受；COMMITTED 时计数器前移。
/// 物理动作（刷写/切标记/回滚）由 infra 注入回调实现——Domain 只做决策。

#include <cstdint>
#include <functional>
#include <string>

namespace amr {
namespace domain {
namespace ota {

enum class State : uint8_t {
  IDLE = 0, DOWNLOADING, DOWNLOADED, INSTALLING, SWITCHING, HEALTH_GATE,
  COMMITTED, ROLLED_BACK,
};

enum class Result : uint8_t {
  ACCEPTED = 0,
  REJECTED_SIGNATURE,   // 签名校验失败（I2：分区未动）
  REJECTED_VERSION,     // 版本低于安全计数器（防降级）
  REJECTED_STATE,       // 状态机时序错误
  ABORTED,              // 写入失败等中止（旧系统仍完好）
};

inline const char * to_string(State s)
{
  switch (s) {
    case State::IDLE: return "IDLE";
    case State::DOWNLOADING: return "DOWNLOADING";
    case State::DOWNLOADED: return "DOWNLOADED";
    case State::INSTALLING: return "INSTALLING";
    case State::SWITCHING: return "SWITCHING";
    case State::HEALTH_GATE: return "HEALTH_GATE";
    case State::COMMITTED: return "COMMITTED";
    case State::ROLLED_BACK: return "ROLLED_BACK";
  }
  return "?";
}

class OtaCoordinator
{
public:
  /// 槽位物理操作（infra 注入；Domain 只决策不执行）
  struct SlotOps {
    /// 下载新版本镜像（一键门面用；空 = 镜像已就位，跳过下载阶段）
    std::function<bool(int64_t version)> fetch;
    /// 刷写指定槽（只应收到非活动槽）——返回 false 视为安装失败
    std::function<bool(char slot, int64_t version)> write;
    /// 原子切换引导标记到指定槽
    std::function<bool(char slot)> set_boot_target;
    /// 引导标记回滚到旧活动槽
    std::function<void()> rollback_marker;
  };

  OtaCoordinator(char active_slot, int64_t active_version, int64_t security_counter,
                 SlotOps ops)
  : active_slot_(active_slot), active_version_(active_version),
    security_counter_(security_counter), ops_(std::move(ops)) {}

  static char other_slot(char s) { return s == 'A' ? 'B' : 'A'; }

  /// 一键门面（porcelain）：驱动到 HEALTH_GATE——「参数注入一键升级」的 API 形态。
  /// plumbing 五方法保留给高级用法/可观测；门面内部仍走全状态机+全部不变量。
  /// 返回后 state()==HEALTH_GATE 表示已切标记，重启生效；重启后调 on_health()。
  /// 注意重启是物理边界，任何 API 都不可能跨越——门面把复杂度压缩到「两个调用」：
  /// 升级前 run_update()，重启后 on_health()。
  Result run_update(int64_t candidate_version, bool signature_valid)
  {
    Result r = request_update(candidate_version, signature_valid);
    if (r != Result::ACCEPTED) return r;             // I2：拒绝先于触碰
    if (ops_.fetch && !ops_.fetch(candidate_version)) {
      state_ = State::IDLE;
      return Result::ABORTED;                        // 下载失败，旧系统未动
    }
    if ((r = on_download_complete()) != Result::ACCEPTED) return r;
    if ((r = begin_install()) != Result::ACCEPTED) return r;    // 只写非活动槽
    if ((r = on_install_complete()) != Result::ACCEPTED) return r;
    return switch_boot_target();                     // 原子切标记 → HEALTH_GATE
  }

  /// 发起升级：签名 + 防降级检查通过才进入 DOWNLOADING（I2）
  Result request_update(int64_t candidate_version, bool signature_valid)
  {
    if (state_ != State::IDLE) return Result::REJECTED_STATE;
    if (!signature_valid) return Result::REJECTED_SIGNATURE;
    if (candidate_version < security_counter_) return Result::REJECTED_VERSION;
    candidate_version_ = candidate_version;
    state_ = State::DOWNLOADING;
    return Result::ACCEPTED;
  }

  Result on_download_complete()
  {
    if (state_ != State::DOWNLOADING) return Result::REJECTED_STATE;
    state_ = State::DOWNLOADED;
    return Result::ACCEPTED;
  }

  /// 安装：只写非活动槽（I1）——活动槽被写视为实现 bug，返回 ABORTED
  Result begin_install()
  {
    if (state_ != State::DOWNLOADED) return Result::REJECTED_STATE;
    char inactive = other_slot(active_slot_);
    if (!ops_.write || !ops_.write(inactive, candidate_version_)) {
      state_ = State::IDLE;  // 旧系统未受影响
      return Result::ABORTED;
    }
    state_ = State::INSTALLING;
    return Result::ACCEPTED;
  }

  Result on_install_complete()
  {
    if (state_ != State::INSTALLING) return Result::REJECTED_STATE;
    state_ = State::SWITCHING;
    return Result::ACCEPTED;
  }

  /// 切换引导标记到候选槽（重启前的最后一步，原子操作）
  Result switch_boot_target()
  {
    if (state_ != State::SWITCHING) return Result::REJECTED_STATE;
    char inactive = other_slot(active_slot_);
    if (!ops_.set_boot_target || !ops_.set_boot_target(inactive)) {
      return Result::ABORTED;  // 标记未切，仍在旧槽
    }
    state_ = State::HEALTH_GATE;
    return Result::ACCEPTED;
  }

  /// 重启进入候选分区后上报健康（I3）：通过→提交；失败→回滚
  Result on_health(bool ok)
  {
    if (state_ != State::HEALTH_GATE) return Result::REJECTED_STATE;
    if (ok) {
      active_slot_ = other_slot(active_slot_);
      active_version_ = candidate_version_;
      if (candidate_version_ > security_counter_) security_counter_ = candidate_version_;
      state_ = State::COMMITTED;
      return Result::ACCEPTED;
    }
    if (ops_.rollback_marker) ops_.rollback_marker();  // 标记回旧槽，重启即回滚
    state_ = State::ROLLED_BACK;
    return Result::ACCEPTED;
  }

  /// 健康门超时（watchdog 触发）——等价于健康失败
  Result force_rollback()
  {
    if (state_ != State::HEALTH_GATE) return Result::REJECTED_STATE;
    if (ops_.rollback_marker) ops_.rollback_marker();
    state_ = State::ROLLED_BACK;
    return Result::ACCEPTED;
  }

  State state() const { return state_; }
  char active_slot() const { return active_slot_; }
  int64_t active_version() const { return active_version_; }
  int64_t security_counter() const { return security_counter_; }
  int64_t candidate_version() const { return candidate_version_; }

private:
  State state_ = State::IDLE;
  char active_slot_;
  int64_t active_version_;
  int64_t security_counter_;
  int64_t candidate_version_ = 0;
  SlotOps ops_;
};

}  // namespace ota
}  // namespace domain
}  // namespace amr

#endif  // ROS2_ROBOT_MIDDLEWARE_DOMAIN_OTA_OTA_COORDINATOR_HPP_
