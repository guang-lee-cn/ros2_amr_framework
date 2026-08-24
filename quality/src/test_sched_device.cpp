// 调度策略内核 + 设备清单 单测
#include <gtest/gtest.h>

#include "ros2_robot_middleware/domain/device/device_registry.hpp"
#include "ros2_robot_middleware/domain/scheduling/task_scheduler.hpp"

namespace dsch = amr::scheduling;
namespace ddev = amr::device;

// ── TaskScheduler ─────────────────────────────────────────────────────

TEST(TaskScheduler, SamePriorityFifo) {
  dsch::TaskScheduler s;
  s.submit({1, "patrol", 5});
  s.submit({2, "charge", 5});
  EXPECT_EQ(s.running()->id, 1u);   // 先提交先跑
  s.complete(1);
  EXPECT_EQ(s.running()->id, 2u);   // 同级 FIFO
}

TEST(TaskScheduler, HigherPriorityPreempts) {
  dsch::TaskScheduler s;
  s.submit({1, "patrol", 5});
  bool preempted = s.submit({2, "e_stop", 10});   // 紧急任务插队
  EXPECT_TRUE(preempted);
  EXPECT_EQ(s.running()->id, 2u);                  // 高优先立即运行
  EXPECT_EQ(s.queued(), 1u);                       // patrol 被挤回队首
  s.complete(2);
  EXPECT_EQ(s.running()->id, 1u);                  // 恢复被抢占者
}

TEST(TaskScheduler, NonPreemptibleTaskHolds) {
  dsch::TaskScheduler s;
  s.submit({1, "calib", 5});
  s.submit({1, "calib", 5});  // 占位
  dsch::Task critical{3, "safe_stop", 1};
  critical.preemptible = false;
  // 先跑不可抢占任务
  dsch::TaskScheduler s2;
  dsch::Task hold{9, "hold", 5};
  hold.preemptible = false;
  s2.submit(hold);
  bool p = s2.submit({10, "other", 99});           // 更高优先也不插队
  EXPECT_FALSE(p);
  EXPECT_EQ(s2.running()->id, 9u);
  EXPECT_EQ(s2.queued(), 1u);                      // 高优先在队列里等
}

TEST(TaskScheduler, CompleteWrongIdIgnored) {
  dsch::TaskScheduler s;
  s.submit({1, "a", 5});
  EXPECT_FALSE(s.complete(999).has_value());
  EXPECT_EQ(s.running()->id, 1u);                  // 不受影响
}

// ── DeviceRegistry ────────────────────────────────────────────────────

TEST(DeviceRegistry, AddDedupRemove) {
  ddev::DeviceRegistry r;
  EXPECT_TRUE(r.add("lidar_front", "sick_tim781", 1));
  EXPECT_FALSE(r.add("lidar_front", "sick_tim781", 2));  // 重复注册拒绝
  EXPECT_EQ(r.size(), 1u);
  EXPECT_TRUE(r.remove("lidar_front"));
  EXPECT_EQ(r.size(), 0u);
}

TEST(DeviceRegistry, HealthFilterAndLookup) {
  ddev::DeviceRegistry r;
  r.add("lidar", "sick", 1);
  r.add("imu", "bmi088", 1);
  r.set_health("imu", 0);            // imu 失效
  auto healthy = r.list(1);
  ASSERT_EQ(healthy.size(), 1u);
  EXPECT_EQ(healthy[0].id, "lidar");
  EXPECT_NE(r.find("imu"), nullptr); // 失效≠注销（仍在清单，可诊断）
  EXPECT_EQ(r.find("imu")->health, 0u);
}
