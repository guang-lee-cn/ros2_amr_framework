/// @file   supervisor_node.cpp — B1 进程监管的 Infrastructure 实现。
///
/// 事件模型：tick 里 waitpid/超时检查 → domain::transition → 执行 Action。
/// spawn 失败走「SPAWNED + EXITED_CRASH」事件对（语义 = 出生即死，进退避），
/// 状态机单一入口原则不被绕开。

#include "ros2_robot_middleware/infrastructure/supervisor_node.hpp"

#include <rclcpp/rclcpp.hpp>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <spawn.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <unordered_set>

#include <cstdlib>

extern char **environ;

namespace amr {
namespace infrastructure {
namespace {

using domain::monitoring::Action;
using domain::monitoring::Event;
using domain::monitoring::Phase;
using domain::monitoring::RestartPolicy;
using domain::monitoring::transition;

constexpr auto kTickPeriod = std::chrono::milliseconds(250);
constexpr int64_t kNsPerSec = 1'000'000'000LL;

const char *phase_str(Phase p) {
  switch (p) {
    case Phase::STOPPED: return "STOPPED";
    case Phase::STARTING: return "STARTING";
    case Phase::RUNNING: return "RUNNING";
    case Phase::BACKOFF: return "BACKOFF";
    case Phase::FATAL: return "FATAL";
  }
  return "?";
}

/// Phase → HealthStatus.status（消费端零新增词汇）
const char *phase_health(Phase p) {
  switch (p) {
    case Phase::RUNNING: return "OK";
    case Phase::STARTING:
    case Phase::BACKOFF: return "WARN";
    case Phase::STOPPED: return "STALE";
    case Phase::FATAL: return "ERROR";
  }
  return "STALE";
}

}  // namespace

SupervisorNode::SupervisorNode(const rclcpp::NodeOptions & options)
: AmrNode("supervisor", options)
{
}

bool SupervisorNode::load_children_from_params() {
  auto names = declare_parameter<std::vector<std::string>>("supervisor.children",
                                                           std::vector<std::string>{});
  std::vector<domain::monitoring::ChildSpec> specs;
  for (const auto & n : names) {
    ProcChild c;
    c.spec.name = n;
    c.cmd = declare_parameter<std::vector<std::string>>(
        "supervisor." + n + ".cmd", std::vector<std::string>{});
    c.spec.depends_on = declare_parameter<std::vector<std::string>>(
        "supervisor." + n + ".depends_on", std::vector<std::string>{});
    c.spec.oneshot = declare_parameter<bool>("supervisor." + n + ".oneshot", false);
    auto &p = c.spec.policy;
    p.max_restarts = declare_parameter<int>("supervisor." + n + ".max_restarts", 5);
    p.window_ns = static_cast<int64_t>(
        declare_parameter<double>("supervisor." + n + ".window_s", 300.0) * kNsPerSec);
    p.backoff_base_ns = static_cast<int64_t>(
        declare_parameter<int>("supervisor." + n + ".backoff_base_ms", 1000) * 1'000'000LL);
    p.backoff_max_ns = static_cast<int64_t>(
        declare_parameter<int>("supervisor." + n + ".backoff_max_ms", 30000) * 1'000'000LL);
    p.startup_timeout_ns = static_cast<int64_t>(
        declare_parameter<double>("supervisor." + n + ".startup_timeout_s", 20.0) * kNsPerSec);
    if (c.cmd.empty()) {
      RCLCPP_ERROR(get_logger(), "supervisor.%s.cmd 为空 — 配置不完整", n.c_str());
      return false;
    }
    children_[n] = std::move(c);
    specs.push_back(children_[n].spec);
  }

  topo_ = domain::monitoring::topo_order(specs);
  if (topo_.empty() && !specs.empty()) {
    RCLCPP_ERROR(get_logger(),
                 "依赖图非法（环/未知依赖/重名）— 拒绝启动，请检查 supervisor.<name>.depends_on");
    return false;
  }
  return true;
}

bool SupervisorNode::deps_all_running(const ProcChild &c) const {
  return std::all_of(c.spec.depends_on.begin(), c.spec.depends_on.end(),
                     [&](const std::string &d) {
                       auto it = children_.find(d);
                       return it != children_.end() &&
                              it->second.state.phase == Phase::RUNNING;
                     });
}

std::vector<std::string> SupervisorNode::transitive_dependents(const std::string &name) const {
  // 单遍拓扑扫描（拓扑序保证依赖先于依赖者）：受影响集合逐层扩大
  std::unordered_set<std::string> affected{name};
  std::vector<std::string> out;
  for (const auto &n : topo_) {
    const auto &c = children_.at(n);
    const bool dep_hit = std::any_of(
        c.spec.depends_on.begin(), c.spec.depends_on.end(),
        [&](const std::string &d) { return affected.count(d) > 0; });
    if (dep_hit && affected.insert(n).second) out.push_back(n);
  }
  return out;  // 正拓扑序（依赖靠前）；级联杀时倒序遍历 = 下游先死
}

bool SupervisorNode::spawn_child(ProcChild &c) {
  std::vector<char *> argv;
  argv.reserve(c.cmd.size() + 1);
  for (auto &a : c.cmd) argv.push_back(const_cast<char *>(a.c_str()));
  argv.push_back(nullptr);

  posix_spawnattr_t attr;
  posix_spawnattr_init(&attr);
  // 独立进程组：kill(-pid) 组式清场，孙进程（ros2 launch 的子进程们）不泄漏
  posix_spawnattr_setpgroup(&attr, 0);

  pid_t pid = -1;
  const int rc = posix_spawn(&pid, argv[0], nullptr, &attr, argv.data(), environ);
  posix_spawnattr_destroy(&attr);
  if (rc != 0) {
    RCLCPP_ERROR(get_logger(), "spawn %s 失败: %s", c.spec.name.c_str(), strerror(rc));
    return false;
  }
  c.pid = pid;
  RCLCPP_INFO(get_logger(), "▶ %s 拉起 (pid %d, deps [%s])", c.spec.name.c_str(), pid,
              [&] { std::string s; for (auto &d : c.spec.depends_on) s += d + ","; return s; }().c_str());
  return true;
}

void SupervisorNode::kill_child(ProcChild &c) {
  if (c.pid <= 0) return;
  const pid_t pid = c.pid;
  kill(-pid, SIGKILL);  // 组杀：含孙进程
  kill(pid, SIGKILL);
  // F-1 修复（2026-09-02 Docker soak 12 僵尸根因）：SIGKILL 后必须 waitpid
  // 回收——僵尸持有 DDS 端口/SHM/PID 号，不回收则重启的子进程可能因资源
  // 冲突再死（compute 反复死活循环的疑似根因）。WNOHANG 重试 ≤200ms。
  int status = 0;
  for (int retry = 0; retry < 20; ++retry) {
    if (waitpid(pid, &status, WNOHANG) == pid) {
      RCLCPP_WARN(get_logger(), "✂ %s (pgid %d) 已杀并回收", c.spec.name.c_str(), pid);
      c.pid = -1;
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  RCLCPP_WARN(get_logger(), "⚠ %s (pgid %d) 杀后 200ms 未回收（罕见）", c.spec.name.c_str(), pid);
  c.pid = -1;
}

void SupervisorNode::cascade_yield(const std::string &name) {
  const auto deps = transitive_dependents(name);
  for (auto it = deps.rbegin(); it != deps.rend(); ++it) {  // 逆拓扑：最下游先让位
    auto &c = children_.at(*it);
    completed_.erase(*it);  // oneshot 必须重跑——先清标记，STOPPED 也不例外
    if (c.state.phase == Phase::STOPPED || c.state.phase == Phase::FATAL) continue;
    feed(c, Event::DEP_RESTARTING);  // feed 内执行 KILL 动作
  }
}

void SupervisorNode::feed(ProcChild &c, Event ev) {
  const int64_t now = now_ns();
  const Phase before = c.state.phase;
  const Action a = transition(c.spec, c.state, ev, now);

  if (c.state.phase != before) {
    RCLCPP_INFO(get_logger(), "%s: %s → %s", c.spec.name.c_str(),
                phase_str(before), phase_str(c.state.phase));
  }

  // oneshot 成功收工：标记完成，try_bring_up 不再重生（依赖重启时级联清除）
  if (c.spec.oneshot && ev == Event::EXITED_OK && c.state.phase == Phase::STOPPED) {
    completed_[c.spec.name] = true;
    RCLCPP_INFO(get_logger(), "%s (oneshot) 完成 ✓", c.spec.name.c_str());
  }

  // 依赖者让位：本子进程从存活相（RUNNING/STARTING）跌入 BACKOFF/FATAL
  // 即「死源」——传递依赖者立刻逆拓扑让位，防下游吃旧数据（ADR D2）
  if ((before == Phase::RUNNING || before == Phase::STARTING) &&
      (c.state.phase == Phase::BACKOFF || c.state.phase == Phase::FATAL)) {
    RCLCPP_WARN(get_logger(), "%s 成为死源，级联让位依赖者", c.spec.name.c_str());
    cascade_yield(c.spec.name);
  }

  switch (a.kind) {
    case Action::Kind::SPAWN:
      if (a.at_ns <= now) {
        // 退避已到期/初始拉起；失败走「出生即死」事件对，保持状态机单一入口
        feed(c, Event::SPAWNED);
        if (!spawn_child(c)) feed(c, Event::EXITED_CRASH);
      }
      break;
    case Action::Kind::KILL:
      kill_child(c);
      break;
    case Action::Kind::MARK_FATAL:
      RCLCPP_ERROR(get_logger(), "💀 %s 预算耗尽 → FATAL（窗口内重启 > %d），级联停依赖者",
                   c.spec.name.c_str(), c.spec.policy.max_restarts);
      kill_child(c);
      break;
    case Action::Kind::NONE:
      break;
  }
}

void SupervisorNode::try_bring_up() {
  for (const auto &n : topo_) {
    auto &c = children_.at(n);
    if (c.state.phase != Phase::STOPPED || completed_[n]) continue;
    if (!deps_all_running(c)) continue;
    feed(c, Event::SPAWNED);
    if (!spawn_child(c)) feed(c, Event::EXITED_CRASH);
  }
}

void SupervisorNode::tick() {
  const int64_t now = now_ns();
  for (auto &[n, c] : children_) {
    (void)n;
    // 1) 进程事件采集
    if (c.pid > 0) {
      int status = 0;
      const pid_t r = waitpid(c.pid, &status, WNOHANG);
      if (r == c.pid) {
        c.pid = -1;
        feed(c, (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? Event::EXITED_OK
                                                                : Event::EXITED_CRASH);
        continue;
      }
      if (r == 0) {  // 还活着
        if (c.state.phase == Phase::STARTING) {
          feed(c, Event::RUNNING);  // v1 健康门：存活即确认；v2 换心跳确认
        } else if (c.state.phase == Phase::RUNNING &&
                   now - c.state.phase_since_ns >= c.spec.policy.window_ns) {
          feed(c, Event::RUNNING_STABLE);
        }
      } else if (errno == ECHILD) {  // 已被收割（组杀竞态）——按崩溃走账
        c.pid = -1;
        feed(c, Event::EXITED_CRASH);
        continue;
      }
    }
    // 2) 退避到期
    if (c.state.phase == Phase::BACKOFF && now >= c.state.resume_at_ns) {
      feed(c, Event::TICK);
    }
    // 3) STARTING 超时（v2 心跳门的主路径；v1 防御性保留）
    if (c.state.phase == Phase::STARTING && c.pid > 0 &&
        now - c.state.phase_since_ns > c.spec.policy.startup_timeout_ns) {
      kill_child(c);
      feed(c, Event::START_TIMEOUT);
    }
  }
  try_bring_up();
}

void SupervisorNode::publish_status() {
  ros2_robot_middleware::msg::HealthReport msg;
  msg.header.stamp = now();
  const int64_t now = now_ns();
  for (const auto &n : topo_) {
    const auto &c = children_.at(n);
    ros2_robot_middleware::msg::HealthStatus s;
    s.node_name = n;
    s.status = phase_health(c.state.phase);
    s.last_seen_s = static_cast<double>(now - c.state.phase_since_ns) / 1e9;
    s.timeout_s = static_cast<double>(c.spec.policy.window_ns) / 1e9;
    msg.nodes.push_back(s);
  }
  if (status_pub_) status_pub_->publish(msg);
}

SupervisorNode::CallbackReturn SupervisorNode::on_configure(const rclcpp_lifecycle::State &) {
  if (!load_children_from_params()) {
    return CallbackReturn::FAILURE;
  }
  status_pub_ = create_pub<ros2_robot_middleware::msg::HealthReport>(
      "/supervisor/report", amr::qos::latched_state());
  RCLCPP_INFO(get_logger(), "配置就绪: %zu 个子进程, 拓扑序 [%s]", children_.size(),
              [&] { std::string s; for (auto &n : topo_) s += n + "→"; return s; }().c_str());
  return CallbackReturn::SUCCESS;
}

SupervisorNode::CallbackReturn SupervisorNode::on_activate(const rclcpp_lifecycle::State &) {
  status_pub_->on_activate();
  start_heartbeat("/supervisor/heartbeat");
  tick_timer_ = create_wall_timer(kTickPeriod, [this]() { tick(); });
  status_timer_ = create_wall_timer(std::chrono::seconds(1),
                                    [this]() { publish_status(); });
  try_bring_up();  // 立即起步，不等首个 tick
  return CallbackReturn::SUCCESS;
}

SupervisorNode::CallbackReturn SupervisorNode::on_deactivate(const rclcpp_lifecycle::State &) {
  tick_timer_.reset();
  status_timer_.reset();
  stop_heartbeat();
  status_pub_->on_deactivate();
  teardown_children();
  return CallbackReturn::SUCCESS;
}

SupervisorNode::CallbackReturn SupervisorNode::on_cleanup(const rclcpp_lifecycle::State &) {
  teardown_children();
  children_.clear();
  topo_.clear();
  completed_.clear();
  status_pub_.reset();
  return CallbackReturn::SUCCESS;
}

SupervisorNode::CallbackReturn SupervisorNode::on_shutdown(const rclcpp_lifecycle::State &) {
  teardown_children();
  return CallbackReturn::SUCCESS;
}

void SupervisorNode::teardown_children() {
  if (children_.empty()) return;
  RCLCPP_INFO(get_logger(), "teardown: 逆拓扑停 %zu 个子进程", children_.size());
  // SIGTERM 先礼后兵（2s 宽限 → SIGKILL 组杀）
  for (auto it = topo_.rbegin(); it != topo_.rend(); ++it) {
    auto &c = children_.at(*it);
    if (c.pid > 0) kill(-c.pid, SIGTERM);
  }
  for (int i = 0; i < 20; ++i) {  // 2s 宽限窗
    bool alive = false;
    for (auto &[n, c] : children_) {
      (void)n;
      if (c.pid > 0) {
        int status = 0;
        if (waitpid(c.pid, &status, WNOHANG) == c.pid) c.pid = -1;
        else alive = true;
      }
    }
    if (!alive) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  for (auto &[n, c] : children_) {
    (void)n;
    if (c.pid > 0) kill_child(c);
  }
}

}  // namespace infrastructure
}  // namespace amr
