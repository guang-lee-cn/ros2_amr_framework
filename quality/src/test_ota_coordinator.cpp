// U8 OTA 状态机单测：三条安全不变量 + 快乐路径 + 防降级 + 回滚
#include <gtest/gtest.h>

#include <vector>

#include "ros2_robot_middleware/domain/ota/ota_coordinator.hpp"

namespace dot = amr::domain::ota;

namespace {
struct Harness {
  std::vector<std::pair<char, int64_t>> writes;
  std::vector<char> boot_targets;
  int rollbacks = 0;
  bool write_ok = true;

  dot::OtaCoordinator make(char active = 'A', int64_t ver = 10, int64_t sec = 8)
  {
    dot::OtaCoordinator::SlotOps ops;
    ops.write = [this](char slot, int64_t v) {
      writes.push_back({slot, v});
      return write_ok;
    };
    ops.set_boot_target = [this](char slot) { boot_targets.push_back(slot); return true; };
    ops.rollback_marker = [this]() { ++rollbacks; };
    return dot::OtaCoordinator(active, ver, sec, std::move(ops));
  }
};
}  // namespace

TEST(OtaCoordinator, HappyPathCommitsAndFlipsSlot) {
  Harness h;
  auto ota = h.make('A', 10, 8);
  ASSERT_EQ(ota.request_update(12, true), dot::Result::ACCEPTED);
  ASSERT_EQ(ota.on_download_complete(), dot::Result::ACCEPTED);
  ASSERT_EQ(ota.begin_install(), dot::Result::ACCEPTED);
  ASSERT_EQ(ota.on_install_complete(), dot::Result::ACCEPTED);
  ASSERT_EQ(ota.switch_boot_target(), dot::Result::ACCEPTED);
  ASSERT_EQ(ota.state(), dot::State::HEALTH_GATE);
  ASSERT_EQ(ota.on_health(true), dot::Result::ACCEPTED);

  EXPECT_EQ(ota.state(), dot::State::COMMITTED);
  EXPECT_EQ(ota.active_slot(), 'B');            // 槽位翻转 A→B
  EXPECT_EQ(ota.active_version(), 12);
  EXPECT_EQ(ota.security_counter(), 12);        // 计数器前移
  // I1：写入只指向非活动槽
  ASSERT_EQ(h.writes.size(), 1u);
  EXPECT_EQ(h.writes[0].first, 'B');
}

TEST(OtaCoordinator, Invariant2_RejectBeforeTouchingPartitions) {
  Harness h;
  auto ota = h.make();
  EXPECT_EQ(ota.request_update(12, /*signature_valid=*/false),
            dot::Result::REJECTED_SIGNATURE);
  EXPECT_EQ(ota.state(), dot::State::IDLE);
  EXPECT_TRUE(h.writes.empty());                // 分区毫发无损
  EXPECT_TRUE(h.boot_targets.empty());
}

TEST(OtaCoordinator, AntiRollback_RejectsDowngrade) {
  Harness h;
  auto ota = h.make('A', 10, /*security_counter=*/9);
  EXPECT_EQ(ota.request_update(8, true), dot::Result::REJECTED_VERSION);
  EXPECT_EQ(ota.state(), dot::State::IDLE);
  EXPECT_TRUE(h.writes.empty());
}

TEST(OtaCoordinator, Invariant3_HealthFailRollsBack) {
  Harness h;
  auto ota = h.make('A', 10, 8);
  ASSERT_EQ(ota.request_update(12, true), dot::Result::ACCEPTED);
  ASSERT_EQ(ota.on_download_complete(), dot::Result::ACCEPTED);
  ASSERT_EQ(ota.begin_install(), dot::Result::ACCEPTED);
  ASSERT_EQ(ota.on_install_complete(), dot::Result::ACCEPTED);
  ASSERT_EQ(ota.switch_boot_target(), dot::Result::ACCEPTED);
  ASSERT_EQ(ota.on_health(false), dot::Result::ACCEPTED);

  EXPECT_EQ(ota.state(), dot::State::ROLLED_BACK);
  EXPECT_EQ(h.rollbacks, 1);                    // 引导标记回滚
  EXPECT_EQ(ota.active_slot(), 'A');            // 仍指旧槽
  EXPECT_EQ(ota.active_version(), 10);          // 版本未变
}

TEST(OtaCoordinator, HealthGateTimeoutForcesRollback) {
  Harness h;
  auto ota = h.make();
  ASSERT_EQ(ota.request_update(11, true), dot::Result::ACCEPTED);
  ASSERT_EQ(ota.on_download_complete(), dot::Result::ACCEPTED);
  ASSERT_EQ(ota.begin_install(), dot::Result::ACCEPTED);
  ASSERT_EQ(ota.on_install_complete(), dot::Result::ACCEPTED);
  ASSERT_EQ(ota.switch_boot_target(), dot::Result::ACCEPTED);
  EXPECT_EQ(ota.force_rollback(), dot::Result::ACCEPTED);  // watchdog 超时
  EXPECT_EQ(ota.state(), dot::State::ROLLED_BACK);
  EXPECT_EQ(h.rollbacks, 1);
}

TEST(OtaCoordinator, WrongStateEventRejected) {
  Harness h;
  auto ota = h.make();
  EXPECT_EQ(ota.on_install_complete(), dot::Result::REJECTED_STATE);  // IDLE 里装完?
  EXPECT_EQ(ota.request_update(11, true), dot::Result::ACCEPTED);
  EXPECT_EQ(ota.begin_install(), dot::Result::REJECTED_STATE);        // 还没下载完
  EXPECT_EQ(ota.state(), dot::State::DOWNLOADING);
}

TEST(OtaCoordinator, WriteFailureAbortsLeavesOldIntact) {
  Harness h;
  h.write_ok = false;  // 模拟刷写中途失败（断电/坏块）
  auto ota = h.make();
  ASSERT_EQ(ota.request_update(11, true), dot::Result::ACCEPTED);
  ASSERT_EQ(ota.on_download_complete(), dot::Result::ACCEPTED);
  EXPECT_EQ(ota.begin_install(), dot::Result::ABORTED);
  EXPECT_EQ(ota.state(), dot::State::IDLE);     // 回到起点，旧系统完好
  EXPECT_TRUE(h.boot_targets.empty());          // 引导标记从未被碰
}
