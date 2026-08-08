/// @file test_integration_scenario.cpp — 场景集成测试
/// 验证：SimulatedLidar + scenario 障碍物 → FusionNode → 点云/物体发布。
#include "ros2_robot_middleware/infrastructure/fusion_node.hpp"
#include "ros2_robot_middleware/msg/perception_objects.hpp"

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>

#include <chrono>
#include <cstring>
#include <memory>

class ScenarioIntegrationTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() { rclcpp::init(0, nullptr); }
  static void TearDownTestSuite() { rclcpp::shutdown(); }
};

/// 用单线程 executor 驱动 node，直到 pred 满足或超时。
template <typename Predicate>
bool spin_until(rclcpp::node_interfaces::NodeBaseInterface::SharedPtr node_iface,
                Predicate pred, std::chrono::milliseconds timeout) {
  auto start = std::chrono::steady_clock::now();
  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(node_iface);
  while (!pred() && (std::chrono::steady_clock::now() - start) < timeout) {
    exec.spin_once(std::chrono::milliseconds(10));
  }
  exec.remove_node(node_iface);
  return pred();
}

/// 构造带 obstacle 场景的 FusionNode。
std::shared_ptr<FusionNode> make_obstacle_fusion() {
  rclcpp::NodeOptions opts;
  opts.parameter_overrides().push_back(
      rclcpp::Parameter("scenario", "obstacle"));
  return std::make_shared<FusionNode>(opts);
}

// ── Given_ObstacleScenario_Then_PublishesPointCloud ──────────────────
// 场景含障碍物 (2,0) r=0.4 → FusionNode 应发布 PointCloud2 点云，
// 且 +x 方向有短距离点（障碍物表面 ~1.6m）。

TEST_F(ScenarioIntegrationTest, Given_ObstacleScenario_Then_PublishesPointCloud) {
  auto fusion = make_obstacle_fusion();
  fusion->configure();
  fusion->activate();

  sensor_msgs::msg::PointCloud2::SharedPtr pc;
  auto pc_sub = fusion->create_subscription<sensor_msgs::msg::PointCloud2>(
      "/sensor/pointcloud", rclcpp::QoS(10).reliable(),
      [&pc](sensor_msgs::msg::PointCloud2::SharedPtr msg) { pc = msg; });

  ASSERT_TRUE(spin_until(fusion->get_node_base_interface(),
                         [&pc] { return pc != nullptr; },
                         std::chrono::seconds(3)));

  ASSERT_NE(pc, nullptr);
  EXPECT_EQ(pc->width, 360u);
  EXPECT_EQ(pc->point_step, 12u);
  // 检查 +x 方向点（障碍物方向）：x ≈ 1.6m（2.0 - 0.4）
  bool found_obstacle = false;
  for (uint32_t i = 0; i < pc->width; ++i) {
    float x;
    std::memcpy(&x, &pc->data[i * 12], 4);
    if (x > 1.0F && x < 2.2F) { found_obstacle = true; break; }
  }
  EXPECT_TRUE(found_obstacle) << "+x 方向应有点（障碍物 2-0.4=1.6m）";
}

// ── Given_ObstacleScenario_Then_DetectsObstacleObject ────────────────
// 场景障碍应被 DBSCAN 聚类 → /perception/objects 出物体。

TEST_F(ScenarioIntegrationTest, Given_ObstacleScenario_Then_DetectsObstacleObject) {
  auto fusion = make_obstacle_fusion();
  fusion->configure();
  fusion->activate();

  ros2_robot_middleware::msg::PerceptionObjects::SharedPtr objs;
  auto objs_sub = fusion->create_subscription<ros2_robot_middleware::msg::PerceptionObjects>(
      "/perception/objects", rclcpp::QoS(10).reliable(),
      [&objs](ros2_robot_middleware::msg::PerceptionObjects::SharedPtr msg) {
        if (msg->objects.size() > 0) objs = msg;
      });

  ASSERT_TRUE(spin_until(fusion->get_node_base_interface(),
                         [&objs] { return objs != nullptr; },
                         std::chrono::seconds(3)));

  ASSERT_NE(objs, nullptr);
  ASSERT_GT(objs->objects.size(), 0u);
  // 障碍物在 (2,0) r=0.4 → 聚类中心应在 x≈1.6-1.7, y≈0
  EXPECT_NEAR(objs->objects[0].x, 1.7F, 0.3F);
  EXPECT_NEAR(objs->objects[0].y, 0.0F, 0.2F);
}

// ── Given_EmptyScenario_Then_NoObjects ────────────────────────────────
// 默认 empty 场景无障碍 → 不应出物体（远空被过滤）。

TEST_F(ScenarioIntegrationTest, Given_EmptyScenario_Then_NoObjects) {
  auto fusion = std::make_shared<FusionNode>();  // 默认 scenario=empty
  fusion->configure();
  fusion->activate();

  int object_count = -1;
  auto objs_sub = fusion->create_subscription<ros2_robot_middleware::msg::PerceptionObjects>(
      "/perception/objects", rclcpp::QoS(10).reliable(),
      [&object_count](ros2_robot_middleware::msg::PerceptionObjects::SharedPtr msg) {
        object_count = static_cast<int>(msg->objects.size());
      });

  ASSERT_TRUE(spin_until(fusion->get_node_base_interface(),
                         [&object_count] { return object_count >= 0; },
                         std::chrono::seconds(3)));

  EXPECT_EQ(object_count, 0) << "empty 场景不应有物体";
}

// ── Given_LowStepScenario_Then_FusesLidarAndDepth ─────────────────────
// lowstep 场景含低矮障碍 (1.5,0) top=0.15（lidar 不可见）+ 常规障碍 (2.5,0.5)。
// 双通道融合：lidar 检出常规障碍（category 空），相机深度检出低矮障碍
// （category="low"）→ 共 2 个物体，且必有一个 category="low"。

TEST_F(ScenarioIntegrationTest, Given_LowStepScenario_Then_FusesLidarAndDepth) {
  rclcpp::NodeOptions opts;
  opts.parameter_overrides().push_back(
      rclcpp::Parameter("scenario", "lowstep"));
  auto fusion = std::make_shared<FusionNode>(opts);
  fusion->configure();
  fusion->activate();

  ros2_robot_middleware::msg::PerceptionObjects::SharedPtr objs;
  auto objs_sub = fusion->create_subscription<ros2_robot_middleware::msg::PerceptionObjects>(
      "/perception/objects", rclcpp::QoS(10).reliable(),
      [&objs](ros2_robot_middleware::msg::PerceptionObjects::SharedPtr msg) {
        if (msg->objects.size() >= 2) objs = msg;  // 等双通道都出
      });

  // 首帧 CRITICAL（传感器 age=-1），等到 FULL 且 ≥2 物体再断言。
  ASSERT_TRUE(spin_until(fusion->get_node_base_interface(),
                         [&objs, &fusion] {
                           return objs != nullptr &&
                                  fusion->degradation_level() ==
                                      FusionNode::DegradationLevel::FULL;
                         },
                         std::chrono::seconds(5)));

  ASSERT_NE(objs, nullptr);
  ASSERT_GE(objs->objects.size(), 2u);

  // 低矮障碍 (1.5,0) 在 x≈1.5 附近、category="low"；常规障碍 category 空。
  bool saw_low = false;
  bool saw_lidar = false;
  for (const auto &obj : objs->objects) {
    if (obj.category == "low") { saw_low = true; }
    else if (obj.category.empty()) { saw_lidar = true; }
  }
  EXPECT_TRUE(saw_low) << "相机深度应检出低矮障碍 (category=low)";
  EXPECT_TRUE(saw_lidar) << "lidar 应检出常规障碍 (category 空)";
}
