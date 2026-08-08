/// @file test_goal_dispatch_gate.cpp — GoalDispatchGate 纯领域测试
/// 覆盖：fusion 就绪门控 + 同任务去重（map 坐标身份）。
#include "ros2_robot_middleware/domain/planning/goal_dispatch_gate.hpp"

#include <gtest/gtest.h>

namespace planning = amr::domain::planning;

class GoalDispatchGateTest : public ::testing::Test {};

// 冷启动门控：fusion 未就绪（无心跳）→ 任何 goal 都不得派发。
// 回归：A* 在感知就绪前用空网格规划，直线穿墙。
TEST_F(GoalDispatchGateTest, Given_FusionNotReady_When_ShouldDispatch_ReturnsFalse) {
  planning::GoalDispatchGate gate;
  EXPECT_FALSE(gate.should_dispatch(3.0F, 0.0F));
  EXPECT_FALSE(gate.fusion_ready());
}

// fusion 就绪且从未派发 → 允许派发。
TEST_F(GoalDispatchGateTest, Given_FusionReadyNoDispatch_When_ShouldDispatch_ReturnsTrue) {
  planning::GoalDispatchGate gate;
  gate.set_fusion_ready(true);
  EXPECT_TRUE(gate.should_dispatch(3.0F, 0.0F));
}

// 同任务 goal 已派发 → 再次感知相同 goal 不得重复派发。
// 回归：原代码比较 map 参数值 vs odom 派发值，真实 TF 偏移下永不相等 → 无限重发。
TEST_F(GoalDispatchGateTest, Given_GoalDispatched_When_SameGoalAgain_ReturnsFalse) {
  planning::GoalDispatchGate gate;
  gate.set_fusion_ready(true);
  gate.note_dispatched(3.0F, 0.0F);
  EXPECT_FALSE(gate.should_dispatch(3.0F, 0.0F));
  EXPECT_FALSE(gate.should_dispatch(3.0F, 0.0F));
}

// 不同任务 goal → 允许派发。
TEST_F(GoalDispatchGateTest, Given_GoalDispatched_When_DifferentGoal_ReturnsTrue) {
  planning::GoalDispatchGate gate;
  gate.set_fusion_ready(true);
  gate.note_dispatched(3.0F, 0.0F);
  EXPECT_TRUE(gate.should_dispatch(5.0F, 0.0F));
}

// fusion 再次失效（心跳 critical/inactive）→ 恢复不派发（即使已派发过）。
TEST_F(GoalDispatchGateTest, Given_FusionRevoked_When_ShouldDispatch_ReturnsFalse) {
  planning::GoalDispatchGate gate;
  gate.set_fusion_ready(true);
  gate.set_fusion_ready(false);
  EXPECT_FALSE(gate.should_dispatch(3.0F, 0.0F));
}
