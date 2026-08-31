/// @file test_supervisor_policy.cpp — B1 监管策略内核单测（纯 domain，无 ROS2）
///
/// 覆盖：拓扑序（链/菱形/环/未知依赖/重名）、退避曲线、状态机全转移、
/// 预算→FATAL、稳定窗清零、oneshot、迟到 EXITED 免疫、级联让位。
#include "ros2_robot_middleware/domain/monitoring/supervisor_policy.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

using amr::domain::monitoring::backoff_ns;
using amr::domain::monitoring::ChildSpec;
using amr::domain::monitoring::ChildState;
using amr::domain::monitoring::Event;
using amr::domain::monitoring::Phase;
using amr::domain::monitoring::RestartPolicy;
using amr::domain::monitoring::Action;
using amr::domain::monitoring::topo_order;
using amr::domain::monitoring::transition;

constexpr int64_t S = 1'000'000'000LL;         // 1s
constexpr int64_t NOW = 10'000 * S;            // 任意起点，测试用绝对时刻推进

ChildSpec spec(const std::string &name, RestartPolicy p = {}) {
  return ChildSpec{name, {}, false, p};
}

Action step(const ChildSpec &s, ChildState &st, Event ev, int64_t now) {
  return transition(s, st, ev, now);
}

// 把子进程从 STOPPED 推到 RUNNING 的惯用序列
Action to_running(const ChildSpec &s, ChildState &st, int64_t now) {
  step(s, st, Event::SPAWNED, now);
  return step(s, st, Event::RUNNING, now);  // v1: 首个 tick 存活确认
}

}  // namespace

// ── 拓扑序 ────────────────────────────────────────────────────────────

TEST(SupervisorTopoTest, Given_Empty_Then_EmptyOrder) {
  EXPECT_TRUE(topo_order({}).empty());
}

TEST(SupervisorTopoTest, Given_Chain_Then_DependencyFirst) {
  std::vector<ChildSpec> specs = {
      {"c", {"b"}, false, {}}, {"b", {"a"}, false, {}}, {"a", {}, false, {}}};
  auto order = topo_order(specs);
  ASSERT_EQ(order.size(), 3u);
  EXPECT_EQ(order[0], "a");
  EXPECT_EQ(order[1], "b");
  EXPECT_EQ(order[2], "c");
}

TEST(SupervisorTopoTest, Given_Diamond_Then_DepsBeforeJoin) {
  std::vector<ChildSpec> specs = {
      {"top", {"l", "r"}, false, {}},
      {"l", {"base"}, false, {}},
      {"r", {"base"}, false, {}},
      {"base", {}, false, {}}};
  auto order = topo_order(specs);
  ASSERT_EQ(order.size(), 4u);
  EXPECT_EQ(order.front(), "base");
  EXPECT_EQ(order.back(), "top");
}

TEST(SupervisorTopoTest, Given_Cycle_Then_Empty) {
  std::vector<ChildSpec> specs = {
      {"a", {"b"}, false, {}}, {"b", {"a"}, false, {}}};
  EXPECT_TRUE(topo_order(specs).empty());
}

TEST(SupervisorTopoTest, Given_UnknownDep_Then_Empty) {
  std::vector<ChildSpec> specs = {{"a", {"ghost"}, false, {}}};
  EXPECT_TRUE(topo_order(specs).empty());
}

TEST(SupervisorTopoTest, Given_DuplicateName_Then_Empty) {
  std::vector<ChildSpec> specs = {spec("a"), spec("a")};
  EXPECT_TRUE(topo_order(specs).empty());
}

// ── 退避曲线 ──────────────────────────────────────────────────────────

TEST(SupervisorBackoffTest, Given_Steps_Then_ExponentialCapped) {
  RestartPolicy p;
  p.backoff_base_ns = S;
  p.backoff_max_ns = 8 * S;
  EXPECT_EQ(backoff_ns(p, 0), S);
  EXPECT_EQ(backoff_ns(p, 1), 2 * S);
  EXPECT_EQ(backoff_ns(p, 2), 4 * S);
  EXPECT_EQ(backoff_ns(p, 3), 8 * S);   // 封顶
  EXPECT_EQ(backoff_ns(p, 10), 8 * S);  // 永不越顶
}

// ── 状态机：正常生命周期 ──────────────────────────────────────────────

TEST(SupervisorTransitionTest, Given_Spawn_Then_Starting) {
  auto s = spec("n");
  ChildState st;
  auto a = step(s, st, Event::SPAWNED, NOW);
  EXPECT_EQ(st.phase, Phase::STARTING);
  EXPECT_EQ(a.kind, Action::Kind::NONE);
}

TEST(SupervisorTransitionTest, Given_AliveConfirm_Then_Running) {
  auto s = spec("n");
  ChildState st;
  step(s, st, Event::SPAWNED, NOW);
  auto a = step(s, st, Event::RUNNING, NOW + S);
  EXPECT_EQ(st.phase, Phase::RUNNING);
  EXPECT_EQ(st.phase_since_ns, NOW + S);
  EXPECT_EQ(a.kind, Action::Kind::NONE);
}

TEST(SupervisorTransitionTest, Given_Crash_Then_BackoffWithDelay) {
  auto s = spec("n");
  ChildState st;
  step(s, st, Event::SPAWNED, NOW);
  auto a = step(s, st, Event::EXITED_CRASH, NOW + S);
  EXPECT_EQ(st.phase, Phase::BACKOFF);
  EXPECT_EQ(a.kind, Action::Kind::SPAWN);
  EXPECT_GT(a.at_ns, NOW + S);  // 未来时刻 = 退避后再拉
  EXPECT_EQ(st.restarts_in_window, 1);
}

TEST(SupervisorTransitionTest, Given_BackoffTickBeforeResume_Then_None) {
  auto s = spec("n");
  ChildState st;
  step(s, st, Event::SPAWNED, NOW);
  auto a = step(s, st, Event::EXITED_CRASH, NOW + S);
  // 退避未到期的 TICK 不动作
  auto early = step(s, st, Event::TICK, a.at_ns - 1);
  EXPECT_EQ(early.kind, Action::Kind::NONE);
  EXPECT_EQ(st.phase, Phase::BACKOFF);
  // 到期 TICK → 立即 SPAWN
  auto due = step(s, st, Event::TICK, a.at_ns);
  EXPECT_EQ(due.kind, Action::Kind::SPAWN);
  EXPECT_EQ(st.phase, Phase::STARTING);
}

TEST(SupervisorTransitionTest, Given_LongRunExitZero_Then_RestartAnyway) {
  auto s = spec("n");  // oneshot=false：exit 0 也是意外死亡
  ChildState st;
  step(s, st, Event::SPAWNED, NOW);
  step(s, st, Event::TICK, NOW);
  auto a = step(s, st, Event::EXITED_OK, NOW + 10 * S);
  EXPECT_EQ(st.phase, Phase::BACKOFF);
  EXPECT_EQ(a.kind, Action::Kind::SPAWN);
}

TEST(SupervisorTransitionTest, Given_OneshotExitZero_Then_StoppedDone) {
  auto s = spec("spawn_amr");
  s.oneshot = true;
  ChildState st;
  step(s, st, Event::SPAWNED, NOW);
  auto a = step(s, st, Event::EXITED_OK, NOW + S);
  EXPECT_EQ(st.phase, Phase::STOPPED);
  EXPECT_EQ(a.kind, Action::Kind::NONE);
  EXPECT_EQ(st.restarts_in_window, 0);
}

TEST(SupervisorTransitionTest, Given_OneshotExitCrash_Then_Restart) {
  auto s = spec("spawn_amr");
  s.oneshot = true;
  ChildState st;
  step(s, st, Event::SPAWNED, NOW);
  auto a = step(s, st, Event::EXITED_CRASH, NOW + S);
  EXPECT_EQ(st.phase, Phase::BACKOFF);  // 失败的一次性任务要重试
  EXPECT_EQ(a.kind, Action::Kind::SPAWN);
}

TEST(SupervisorTransitionTest, Given_StartTimeout_Then_CrashPath) {
  auto s = spec("n");
  s.policy.startup_timeout_ns = 5 * S;
  ChildState st;
  step(s, st, Event::SPAWNED, NOW);
  step(s, st, Event::START_TIMEOUT, NOW + 6 * S);
  EXPECT_EQ(st.phase, Phase::BACKOFF);
  EXPECT_EQ(st.restarts_in_window, 1);
}

// ── 预算 → FATAL ─────────────────────────────────────────────────────

TEST(SupervisorBudgetTest, Given_ExceedBudget_Then_Fatal) {
  RestartPolicy p;
  p.max_restarts = 2;
  p.backoff_base_ns = S;
  auto s = spec("n", p);
  ChildState st;
  Action a{};
  for (int i = 0; i < 3; ++i) {  // 第 3 次崩溃超出预算
    if (st.phase == Phase::BACKOFF) {
      step(s, st, Event::TICK, st.resume_at_ns);  // 退避到期 → STARTING
    }
    step(s, st, Event::SPAWNED, st.phase_since_ns);
    a = step(s, st, Event::EXITED_CRASH, st.phase_since_ns + S);
  }
  EXPECT_EQ(st.phase, Phase::FATAL);
  EXPECT_EQ(a.kind, Action::Kind::MARK_FATAL);
}

TEST(SupervisorBudgetTest, Given_StableWindow_Then_BudgetResets) {
  RestartPolicy p;
  p.max_restarts = 2;
  p.window_ns = 60 * S;
  auto s = spec("n", p);
  ChildState st;
  // 崩 2 次（=预算用满但未超）
  for (int i = 0; i < 2; ++i) {
    if (st.phase == Phase::BACKOFF) {
      step(s, st, Event::TICK, st.resume_at_ns);
    }
    step(s, st, Event::SPAWNED, st.phase_since_ns);
    step(s, st, Event::EXITED_CRASH, st.phase_since_ns + S);
  }
  EXPECT_EQ(st.restarts_in_window, 2);
  // 退避到期重生 → 存活确认 → 稳定窗达标 → 清零
  step(s, st, Event::TICK, st.resume_at_ns);            // BACKOFF → STARTING
  step(s, st, Event::RUNNING, st.phase_since_ns + S);   // → RUNNING
  step(s, st, Event::RUNNING_STABLE, st.phase_since_ns + 61 * S);
  EXPECT_EQ(st.restarts_in_window, 0);
  EXPECT_EQ(st.backoff_step, 0);
  // 再崩 1 次不应 FATAL（预算已清零重算）
  auto a = step(s, st, Event::EXITED_CRASH, st.phase_since_ns + 62 * S);
  EXPECT_EQ(st.phase, Phase::BACKOFF);
  EXPECT_EQ(a.kind, Action::Kind::SPAWN);
}

TEST(SupervisorBudgetTest, Given_Fatal_Then_Terminal) {
  RestartPolicy p;
  p.max_restarts = 0;  // 一次都不许崩
  auto s = spec("n", p);
  ChildState st;
  step(s, st, Event::SPAWNED, NOW);
  auto a = step(s, st, Event::EXITED_CRASH, NOW + S);
  EXPECT_EQ(a.kind, Action::Kind::MARK_FATAL);
  // FATAL 吸收一切事件
  for (Event ev : {Event::SPAWNED, Event::TICK, Event::EXITED_CRASH,
                   Event::DEP_RESTARTING, Event::RUNNING_STABLE}) {
    EXPECT_EQ(step(s, st, ev, NOW + 2 * S).kind, Action::Kind::NONE);
  }
  EXPECT_EQ(st.phase, Phase::FATAL);
}

// ── 迟到事件免疫与级联让位 ────────────────────────────────────────────

TEST(SupervisorHygieneTest, Given_LateExitedInStopped_Then_Ignored) {
  auto s = spec("n");
  ChildState st;  // STOPPED
  auto a = step(s, st, Event::EXITED_CRASH, NOW);
  EXPECT_EQ(a.kind, Action::Kind::NONE);
  EXPECT_EQ(st.phase, Phase::STOPPED);
  EXPECT_EQ(st.restarts_in_window, 0);  // 不污染新周期
}

TEST(SupervisorHygieneTest, Given_LateExitedInBackoff_Then_Ignored) {
  auto s = spec("n");
  ChildState st;
  step(s, st, Event::SPAWNED, NOW);
  step(s, st, Event::EXITED_CRASH, NOW + S);  // → BACKOFF
  auto a = step(s, st, Event::EXITED_CRASH, NOW + 2 * S);  // 旧僵尸晚 reap
  EXPECT_EQ(a.kind, Action::Kind::NONE);
  EXPECT_EQ(st.restarts_in_window, 1);  // 不重复计数
}

TEST(SupervisorHygieneTest, Given_DepRestarting_Then_YieldWithKill) {
  auto s = spec("downstream");
  ChildState st;
  to_running(s, st, NOW);
  auto a = step(s, st, Event::DEP_RESTARTING, NOW + 5 * S);
  EXPECT_EQ(st.phase, Phase::STOPPED);
  EXPECT_EQ(a.kind, Action::Kind::KILL);
  // 让位后迟到 EXITED（被我们 kill 导致）不得计数
  auto a2 = step(s, st, Event::EXITED_CRASH, NOW + 6 * S);
  EXPECT_EQ(a2.kind, Action::Kind::NONE);
  EXPECT_EQ(st.restarts_in_window, 0);
}

TEST(SupervisorHygieneTest, Given_DepRestartingInBackoff_Then_StopWaiting) {
  auto s = spec("downstream");
  ChildState st;
  step(s, st, Event::SPAWNED, NOW);
  step(s, st, Event::EXITED_CRASH, NOW + S);  // → BACKOFF
  auto a = step(s, st, Event::DEP_RESTARTING, NOW + 2 * S);
  EXPECT_EQ(st.phase, Phase::STOPPED);  // 退避作废，让位等待
  EXPECT_EQ(a.kind, Action::Kind::NONE);  // 已死，无需 KILL
}
