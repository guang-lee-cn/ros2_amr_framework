#ifndef ROS2_ROBOT_MIDDLEWARE_DOMAIN_SUPERVISOR_POLICY_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DOMAIN_SUPERVISOR_POLICY_HPP_

/// @file   supervisor_policy.hpp
/// @brief  进程监管策略内核（B1）——纯 domain，零 ROS2。
///
/// 状态机（事件驱动，见 docs/design/20260825-b1-supervisor-adr.md D2）：
///   STOPPED → STARTING → RUNNING
///                 │ exit≠0 / startup 超时        │ exit（长驻进程 exit 0 同罪）
///                 ▼                             ▼
///              BACKOFF ──(退避到期)──▶ STARTING   （restarts_in_window++）
///                 │ 窗口内超 max_restarts
///                 ▼
///               FATAL（级联：传递依赖者全部停止）
///
/// 语义对齐既有 RecoveryPolicy：budget 超限 → FATAL 升级；稳定后计数清零。
/// Infrastructure 层持有进程/时钟，只喂事件、执行 Action。

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace amr {
namespace domain {
namespace monitoring {

/// 重启预算与退避参数（全部带默认，声明式配置逐项覆盖）。
struct RestartPolicy {
  int max_restarts = 5;                          // 窗口内上限，超过 → FATAL
  int64_t window_ns = 300'000'000'000LL;         // 稳定窗：RUNNING 达标即清零计数
  int64_t backoff_base_ns = 1'000'000'000LL;     // 1s
  int64_t backoff_max_ns = 30'000'000'000LL;     // 30s 封顶
  int64_t startup_timeout_ns = 20'000'000'000LL; // STARTING→RUNNING 窗，超 = crash
};

/// 受管子进程的声明（配置装载结果）。
struct ChildSpec {
  std::string name;
  std::vector<std::string> depends_on;
  bool oneshot = false;  // true: exit 0 = 成功收工（如 spawn_amr）；false: exit 0 = 意外死亡
  RestartPolicy policy;
};

enum class Phase : uint8_t {
  STOPPED,    // 未拉起（初始/依赖未就绪/oneshot 完成/级联被停）
  STARTING,   // 已 spawn，等 RUNNING 确认（v1: 进程活着即转，v2: 心跳确认）
  RUNNING,    // 存活中
  BACKOFF,    // 崩溃后退避，resume_at_ns 到点再 spawn
  FATAL,      // 预算耗尽，不再重启；依赖者应级联停止
};

/// 子进程的监管状态（Infrastructure 层随事件更新）。
struct ChildState {
  Phase phase = Phase::STOPPED;
  int restarts_in_window = 0;   // 窗口内已重启次数（RUNNING 稳定后清零）
  int64_t phase_since_ns = 0;    // 当前相位起始（时钟由 infra 注入）
  int64_t resume_at_ns = 0;      // BACKOFF 到期时刻
  int backoff_step = 0;
};

/// Infrastructure 喂给状态机的事件。
enum class Event : uint8_t {
  SPAWNED,         // posix_spawn 成功（infra 负责）
  RUNNING,         // 存活确认（v1: 首个 tick waitpid 仍活；v2: 心跳到达）
  EXITED_OK,       // waitpid: exit 0
  EXITED_CRASH,    // waitpid: exit≠0 / 信号死（含 kill -9）
  START_TIMEOUT,   // STARTING 超过 startup_timeout_ns
  DEP_RESTARTING,  // 某依赖进入 BACKOFF/FATAL/重启流程 → 本子进程应让位
  RUNNING_STABLE,  // RUNNING 持续 ≥ window（infra 判定后喂）
  TICK,            // 退避到期轮询
};

/// 状态机输出的动作（infra 执行）。
struct Action {
  enum class Kind : uint8_t { NONE, SPAWN, KILL, MARK_FATAL } kind = Kind::NONE;
  int64_t at_ns = 0;  // SPAWN: 执行时刻（= now + backoff）；KILL/MARK_FATAL: 语义即时
};

/// 指数退避：base×2^step，封顶 max。step 无上界防护由封顶保证。
inline int64_t backoff_ns(const RestartPolicy &p, int step) {
  if (step <= 0) return p.backoff_base_ns;
  int64_t v = p.backoff_base_ns;
  for (int i = 0; i < step; ++i) {
    v *= 2;
    if (v >= p.backoff_max_ns) return p.backoff_max_ns;
  }
  return v < p.backoff_max_ns ? v : p.backoff_max_ns;
}

/// 单子进程状态转移。返回 infra 要执行的动作。
/// 迟到的 EXITED 免疫：仅 STARTING/RUNNING 相位接受 EXITED_*（僵尸晚 reap
/// 不得污染新周期）。FATAL 吸收一切事件（终态，无出边）。
inline Action transition(const ChildSpec &spec, ChildState &st, Event ev, int64_t now_ns) {
  const RestartPolicy &p = spec.policy;

  auto enter_backoff = [&](Action::Kind k = Action::Kind::SPAWN) -> Action {
    st.restarts_in_window += 1;
    if (st.restarts_in_window > p.max_restarts) {
      st.phase = Phase::FATAL;
      st.phase_since_ns = now_ns;
      return {Action::Kind::MARK_FATAL, now_ns};
    }
    st.phase = Phase::BACKOFF;
    st.phase_since_ns = now_ns;
    st.resume_at_ns = now_ns + backoff_ns(p, st.backoff_step);
    st.backoff_step += 1;
    return {k, st.resume_at_ns};
  };

  switch (st.phase) {
    case Phase::STOPPED:
      if (ev == Event::SPAWNED) {
        st.phase = Phase::STARTING;
        st.phase_since_ns = now_ns;
      }
      return {};  // 其余事件（含迟到 EXITED）忽略

    case Phase::STARTING:
      switch (ev) {
        case Event::SPAWNED:
          return {};  // 重复 SPAWNED 不可能，防御
        case Event::RUNNING:
          st.phase = Phase::RUNNING;
          st.phase_since_ns = now_ns;
          return {};
        case Event::EXITED_OK:
          if (spec.oneshot) {
            st.phase = Phase::STOPPED;  // 一次性任务完成
            st.phase_since_ns = now_ns;
            st.restarts_in_window = 0;
            st.backoff_step = 0;
            return {};
          }
          return enter_backoff();  // 长驻进程 startup 期退场，exit 0 也算死
        case Event::EXITED_CRASH:
        case Event::START_TIMEOUT:
          return enter_backoff();
        case Event::DEP_RESTARTING:
          st.phase = Phase::STOPPED;  // 让位：等依赖恢复后按拓扑序重生
          st.phase_since_ns = now_ns;
          return {Action::Kind::KILL, now_ns};
        default:
          return {};
      }

    case Phase::RUNNING:
      switch (ev) {
        case Event::EXITED_OK:
          if (spec.oneshot) {
            st.phase = Phase::STOPPED;
            st.phase_since_ns = now_ns;
            st.restarts_in_window = 0;
            st.backoff_step = 0;
            return {};
          }
          return enter_backoff();
        case Event::EXITED_CRASH:
          return enter_backoff();
        case Event::DEP_RESTARTING:
          st.phase = Phase::STOPPED;
          st.phase_since_ns = now_ns;
          return {Action::Kind::KILL, now_ns};
        case Event::RUNNING_STABLE:
          st.restarts_in_window = 0;  // 稳定窗达标：预算与退避双清零
          st.backoff_step = 0;
          return {};
        default:
          return {};
      }

    case Phase::BACKOFF:
      if (ev == Event::TICK && now_ns >= st.resume_at_ns) {
        st.phase = Phase::STARTING;
        st.phase_since_ns = now_ns;
        return {Action::Kind::SPAWN, now_ns};
      }
      if (ev == Event::DEP_RESTARTING) {
        st.phase = Phase::STOPPED;  // 依赖又出事：退出退避、让位
        st.phase_since_ns = now_ns;
        return {};
      }
      return {};  // 到期前的 TICK / 迟到 EXITED 忽略

    case Phase::FATAL:
      return {};  // 终态：预算耗尽，等人工介入
  }
  return {};
}

/// Kahn 拓扑排序：依赖在前。环或引用未知依赖 → 返回空（配置校验拒绝启动）。
inline std::vector<std::string> topo_order(const std::vector<ChildSpec> &specs) {
  std::unordered_map<std::string, const ChildSpec *> by_name;
  std::unordered_set<std::string> names;
  for (const auto &s : specs) {
    if (!names.insert(s.name).second) return {};  // 重名 = 配置错误
    by_name[s.name] = &s;
  }
  for (const auto &s : specs) {
    for (const auto &d : s.depends_on) {
      if (names.find(d) == names.end()) return {};  // 未知依赖
    }
  }
  // 稳定序：按输入顺序打破平局（可复现的拉起序）
  std::vector<std::string> order;
  order.reserve(specs.size());
  std::unordered_set<std::string> done;
  bool progress = true;
  while (order.size() < specs.size() && progress) {
    progress = false;
    for (const auto &s : specs) {
      if (done.count(s.name)) continue;
      bool ready = true;
      for (const auto &d : s.depends_on) {
        if (!done.count(d)) { ready = false; break; }
      }
      if (ready) {
        order.push_back(s.name);
        done.insert(s.name);
        progress = true;
      }
    }
  }
  return order.size() == specs.size() ? order : std::vector<std::string>{};  // 环
}

}  // namespace monitoring
}  // namespace domain
}  // namespace amr

#endif  // ROS2_ROBOT_MIDDLEWARE_DOMAIN_SUPERVISOR_POLICY_HPP_
