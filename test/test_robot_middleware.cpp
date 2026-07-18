#include "ros2_robot_middleware/infrastructure/camera_node.hpp"
#include "ros2_robot_middleware/infrastructure/decision_node.hpp"
#include "ros2_robot_middleware/infrastructure/fusion_node.hpp"
#include "ros2_robot_middleware/infrastructure/imu_node.hpp"
#include "ros2_robot_middleware/infrastructure/lidar_node.hpp"
#include "ros2_robot_middleware/infrastructure/motor_ctrl_node.hpp"

#include "ros2_robot_middleware/action/move_to_pose.hpp"
#include "ros2_robot_middleware/msg/perception_objects.hpp"
#include "ros2_robot_middleware/srv/set_param.hpp"

#include <sensor_msgs/msg/image.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>

#include <gtest/gtest.h>

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>

#include <chrono>
#include <memory>

// ---------------------------------------------------------------------------
// Fixture — rclcpp init once per suite (DDS cleanup is async, per-test
// shutdown/init is unreliable for isolation)
// ---------------------------------------------------------------------------
class RobotMiddlewareTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() { rclcpp::init(0, nullptr); }
  static void TearDownTestSuite() { rclcpp::shutdown(); }
};

// ---------------------------------------------------------------------------
// Helper — spin executor until predicate is true or timeout expires
// ---------------------------------------------------------------------------
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

// ===================================================================
// Sensor layer tests
// ===================================================================

// ── LidarNode ──────────────────────────────────────────────────────

TEST_F(RobotMiddlewareTest, LidarNode_TimerFires_RangesInPhysicalBounds) {
  // Given: LidarNode publishes simulated SICK TiM781 data at 10Hz
  auto node = std::make_shared<LidarNode>();
  node->configure();
  node->activate();

  sensor_msgs::msg::LaserScan::SharedPtr last_msg;
  auto sub = node->create_subscription<sensor_msgs::msg::LaserScan>(
      "/sensor/lidar", rclcpp::QoS(10).best_effort(),
      [&last_msg](sensor_msgs::msg::LaserScan::SharedPtr msg) { last_msg = msg; });

  // When: the timer callback fires
  ASSERT_TRUE(spin_until(node->get_node_base_interface(),
                         [&last_msg] { return last_msg != nullptr; },
                         std::chrono::seconds(3)));

  // Then: 360 valid ranges and intensities within physical bounds
  EXPECT_EQ(last_msg->ranges.size(), 360u);
  EXPECT_EQ(last_msg->intensities.size(), 360u);
  EXPECT_NEAR(last_msg->angle_min, -M_PI, 0.001F);
  EXPECT_NEAR(last_msg->angle_max, M_PI, 0.001F);

  for (size_t i = 0; i < last_msg->ranges.size(); ++i) {
    EXPECT_GE(last_msg->ranges[i], 0.1F) << "range[" << i << "] below min";
    EXPECT_LE(last_msg->ranges[i], 6.5F) << "range[" << i << "] above max";
  }
  for (size_t i = 0; i < last_msg->intensities.size(); ++i) {
    EXPECT_GE(last_msg->intensities[i], 0.0F) << "intensity[" << i << "] negative";
    EXPECT_LE(last_msg->intensities[i], 1.0F) << "intensity[" << i << "] above 1.0";
  }
}

// ── ImuNode ────────────────────────────────────────────────────────

TEST_F(RobotMiddlewareTest, ImuNode_TimerFires_DataWithinSensorSpec) {
  // Given: ImuNode publishes simulated Bosch BMI088 data at 100Hz
  auto node = std::make_shared<ImuNode>();
  node->configure();
  node->activate();

  sensor_msgs::msg::Imu::SharedPtr last_msg;
  auto sub = node->create_subscription<sensor_msgs::msg::Imu>(
      "/sensor/imu", rclcpp::QoS(10).reliable(),
      [&last_msg](sensor_msgs::msg::Imu::SharedPtr msg) { last_msg = msg; });

  // When: the timer callback fires
  ASSERT_TRUE(spin_until(node->get_node_base_interface(),
                         [&last_msg] { return last_msg != nullptr; },
                         std::chrono::seconds(3)));

  // Then: angular velocity within BMI088 noise spec, acceleration within bounds
  // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
  EXPECT_NEAR(last_msg->angular_velocity.x, 0.0, 0.04);
  EXPECT_NEAR(last_msg->angular_velocity.y, 0.0, 0.04);
  EXPECT_NEAR(last_msg->angular_velocity.z, 0.0, 0.04);
  EXPECT_NEAR(last_msg->linear_acceleration.x, 0.0, 0.2);
  EXPECT_NEAR(last_msg->linear_acceleration.y, 0.0, 0.2);
  EXPECT_NEAR(last_msg->linear_acceleration.z, 0.0, 0.2);
  // NOLINTEND(cppcoreguidelines-pro-type-union-access)
  EXPECT_EQ(last_msg->header.frame_id, "imu_link");
}

// ── CameraNode ─────────────────────────────────────────────────────

TEST_F(RobotMiddlewareTest, CameraNode_TimerFires_640x480Rgb8Image) {
  // Given: CameraNode publishes simulated Intel RealSense D435 frames at 5Hz
  auto node = std::make_shared<CameraNode>();
  node->configure();
  node->activate();

  sensor_msgs::msg::Image::SharedPtr last_msg;
  auto sub = node->create_subscription<sensor_msgs::msg::Image>(
      "/sensor/camera", rclcpp::QoS(10).best_effort(),
      [&last_msg](sensor_msgs::msg::Image::SharedPtr msg) { last_msg = msg; });

  // When: the timer callback fires
  ASSERT_TRUE(spin_until(node->get_node_base_interface(),
                         [&last_msg] { return last_msg != nullptr; },
                         std::chrono::seconds(3)));

  // Then: 640×480 rgb8 image, 900KB payload
  EXPECT_EQ(last_msg->width, 640u);
  EXPECT_EQ(last_msg->height, 480u);
  EXPECT_EQ(last_msg->step, 1920u);
  EXPECT_EQ(last_msg->encoding, "rgb8");
  EXPECT_EQ(last_msg->data.size(), 921600u);
}

// ===================================================================
// Motor control layer tests
// ===================================================================

TEST_F(RobotMiddlewareTest, MotorCtrl_CloseTarget_ReachesImmediately) {
  // Given: MotorCtrlNode with step_size = 0.05, action client ready
  auto motor = std::make_shared<MotorCtrlNode>();
  motor->configure();
  motor->activate();
  auto client = rclcpp_action::create_client<ros2_robot_middleware::action::MoveToPose>(
      motor, "/cmd/move_to_pose");
  ASSERT_TRUE(client->wait_for_action_server(std::chrono::seconds(2)));

  auto goal = ros2_robot_middleware::action::MoveToPose::Goal{};
  goal.target_x = 0.0F;
  goal.target_y = 0.03F;  // within one step
  goal.target_theta = 0.0F;
  goal.max_speed = 0.5F;

  bool done = false;
  bool reached = false;
  auto opts = rclcpp_action::Client<ros2_robot_middleware::action::MoveToPose>::SendGoalOptions{};
  opts.result_callback =
      [&done, &reached](
          const rclcpp_action::ClientGoalHandle<ros2_robot_middleware::action::MoveToPose>::WrappedResult &result) {
        done = true;
        reached = result.result->reached;
      };

  // When: goal sent to a target within step_size distance
  client->async_send_goal(goal, opts);

  ASSERT_TRUE(spin_until(motor->get_node_base_interface(),
                         [&done] { return done; },
                         std::chrono::seconds(5)));

  // Then: goal reaches target immediately
  EXPECT_TRUE(reached);
}

TEST_F(RobotMiddlewareTest, MotorCtrl_FarTarget_StepsUntilReached) {
  // Given: MotorCtrlNode with step_size = 0.05, target at distance 0.15 (4 steps)
  auto motor = std::make_shared<MotorCtrlNode>();
  motor->configure();
  motor->activate();
  auto client = rclcpp_action::create_client<ros2_robot_middleware::action::MoveToPose>(
      motor, "/cmd/move_to_pose");
  ASSERT_TRUE(client->wait_for_action_server(std::chrono::seconds(2)));

  auto goal = ros2_robot_middleware::action::MoveToPose::Goal{};
  goal.target_x = 0.0F;
  goal.target_y = 0.15F;  // requires multiple steps
  goal.target_theta = 0.0F;
  goal.max_speed = 0.5F;

  bool done = false;
  bool reached = false;
  auto opts = rclcpp_action::Client<ros2_robot_middleware::action::MoveToPose>::SendGoalOptions{};
  opts.result_callback =
      [&done, &reached](
          const rclcpp_action::ClientGoalHandle<ros2_robot_middleware::action::MoveToPose>::WrappedResult &result) {
        done = true;
        reached = result.result->reached;
      };

  // When: goal sent requiring multiple interpolation steps
  client->async_send_goal(goal, opts);

  ASSERT_TRUE(spin_until(motor->get_node_base_interface(),
                         [&done] { return done; },
                         std::chrono::seconds(5)));

  // Then: goal eventually reached after stepping
  EXPECT_TRUE(reached);
}

TEST_F(RobotMiddlewareTest, MotorCtrl_SetParamKnown_UpdatesAndAcks) {
  // Given: MotorCtrlNode with SetParam service
  auto motor = std::make_shared<MotorCtrlNode>();
  motor->configure();
  motor->activate();
  auto client = motor->create_client<ros2_robot_middleware::srv::SetParam>("/cmd/set_param");
  ASSERT_TRUE(client->wait_for_service(std::chrono::seconds(2)));

  // When: set_param "step_size" = 0.10
  auto request = std::make_shared<ros2_robot_middleware::srv::SetParam::Request>();
  request->param_name = "step_size";
  request->value = 0.10;
  auto future = client->async_send_request(request);

  ASSERT_TRUE(spin_until(motor->get_node_base_interface(),
                         [&future] {
                           return future.wait_for(std::chrono::milliseconds(0)) ==
                                  std::future_status::ready;
                         },
                         std::chrono::seconds(2)));

  // Then: acknowledged with success
  auto response = future.get();
  EXPECT_TRUE(response->success);
  EXPECT_EQ(response->message, "Parameter updated");
}

TEST_F(RobotMiddlewareTest, MotorCtrl_SetParamUnknown_ReturnsMessage) {
  // Given: MotorCtrlNode with SetParam service
  auto motor = std::make_shared<MotorCtrlNode>();
  motor->configure();
  motor->activate();
  auto client = motor->create_client<ros2_robot_middleware::srv::SetParam>("/cmd/set_param");
  ASSERT_TRUE(client->wait_for_service(std::chrono::seconds(2)));

  // When: set_param called with an unrecognized parameter name
  auto request = std::make_shared<ros2_robot_middleware::srv::SetParam::Request>();
  request->param_name = "unknown_param";
  request->value = 1.0;
  auto future = client->async_send_request(request);

  ASSERT_TRUE(spin_until(motor->get_node_base_interface(),
                         [&future] {
                           return future.wait_for(std::chrono::milliseconds(0)) ==
                                  std::future_status::ready;
                         },
                         std::chrono::seconds(2)));

  // Then: success is still true (non-fatal), but message indicates unknown
  auto response = future.get();
  EXPECT_TRUE(response->success);
  EXPECT_EQ(response->message, "Unknown parameter");
}

// ===================================================================
// Decision layer tests
// ===================================================================

TEST_F(RobotMiddlewareTest, DecisionNode_PerceptionArrives_SendsGoalToTargetPose) {
  // Given: DecisionNode + a mock action server recording incoming goals
  auto decision = std::make_shared<DecisionNode>();
  decision->configure();
  decision->activate();

  // Mock action server: accept all goals, store the received coordinates
  float received_x = -1.0F;
  float received_y = -1.0F;
  bool goal_received = false;

  auto mock_server = rclcpp_action::create_server<ros2_robot_middleware::action::MoveToPose>(
      decision, "/cmd/move_to_pose",
      [](const rclcpp_action::GoalUUID &,
         std::shared_ptr<const ros2_robot_middleware::action::MoveToPose::Goal>) {
        return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
      },
      [](const std::shared_ptr<rclcpp_action::ServerGoalHandle<ros2_robot_middleware::action::MoveToPose>>) {
        return rclcpp_action::CancelResponse::ACCEPT;
      },
      [&received_x, &received_y, &goal_received](
          const std::shared_ptr<rclcpp_action::ServerGoalHandle<ros2_robot_middleware::action::MoveToPose>> gh) {
        received_x = gh->get_goal()->target_x;
        received_y = gh->get_goal()->target_y;
        goal_received = true;
        auto result = std::make_shared<ros2_robot_middleware::action::MoveToPose::Result>();
        result->reached = true;
        result->final_x = received_x;
        result->final_y = received_y;
        gh->succeed(result);
      });

  // When: a PerceptionObjects message with one object at (2.0, 1.5) arrives
  auto perception = ros2_robot_middleware::msg::PerceptionObjects{};
  perception.header.frame_id = "base_link";
  auto obj = ros2_robot_middleware::msg::Object{};
  obj.id = "obj_0";
  obj.x = 2.0F;
  obj.y = 1.5F;
  obj.z = 0.0F;
  perception.objects.push_back(obj);

  auto pub = decision->create_publisher<ros2_robot_middleware::msg::PerceptionObjects>(
      "/perception/objects", rclcpp::QoS(10).reliable());
  pub->on_activate();
  pub->publish(perception);

  ASSERT_TRUE(spin_until(decision->get_node_base_interface(),
                         [&goal_received] { return goal_received; },
                         std::chrono::seconds(3)));

  // Then: a MoveToPose goal is sent with matching object coordinates
  EXPECT_TRUE(goal_received);
  EXPECT_FLOAT_EQ(received_x, 2.0F);
  EXPECT_FLOAT_EQ(received_y, 1.5F);
}

// ===================================================================
// Fusion layer tests (last — publishes perception data)
// ===================================================================

// Helper: build a lidar frame with configurable range at given angular span
static sensor_msgs::msg::LaserScan
make_lidar(float range_near, float range_far, int center_idx, int half_width) {
  auto msg = sensor_msgs::msg::LaserScan{};
  msg.header.frame_id = "lidar_frame";
  msg.angle_min       = -M_PI;
  msg.angle_max       = M_PI;
  msg.angle_increment = 2.0F * M_PI / 360.0F;
  msg.time_increment  = 0.0001F;
  msg.scan_time       = 0.1F;
  msg.range_min       = 0.1F;
  msg.range_max       = 6.5F;
  msg.ranges.resize(360);
  msg.intensities.resize(360);
  for (int i = 0; i < 360; ++i) {
    int dist_from_center = std::abs(i - center_idx);
    if (dist_from_center <= half_width) {
      msg.ranges[i] = range_near;
      msg.intensities[i] = 0.8F;
    } else {
      msg.ranges[i] = range_far;
      msg.intensities[i] = 0.1F;
    }
  }
  return msg;
}

static sensor_msgs::msg::Imu make_imu() {
  auto msg = sensor_msgs::msg::Imu{};
  msg.header.frame_id = "imu_link";
  msg.orientation.w = 1.0;
  return msg;
}

static sensor_msgs::msg::Image make_camera() {
  auto msg = sensor_msgs::msg::Image{};
  msg.header.frame_id = "camera_link";
  msg.width  = 640;
  msg.height = 480;
  msg.step   = 1920;
  msg.encoding = "rgb8";
  msg.is_bigendian = 0;
  msg.data.resize(921600);
  return msg;
}

TEST_F(RobotMiddlewareTest, FusionNode_AllSensorsReady_DetectsNearbyCluster) {
  // Given: FusionNode with all 3 sensor caches populated,
  //        lidar has a cluster at 1.5m (within the 3m detection threshold)
  auto fusion = std::make_shared<FusionNode>();
  fusion->configure();
  fusion->activate();

  ros2_robot_middleware::msg::PerceptionObjects::SharedPtr last_output;
  auto out_sub = fusion->create_subscription<ros2_robot_middleware::msg::PerceptionObjects>(
      "/perception/objects", rclcpp::QoS(10).reliable(),
      [&last_output](ros2_robot_middleware::msg::PerceptionObjects::SharedPtr msg) { last_output = msg; });

  auto lidar_pub = fusion->create_publisher<sensor_msgs::msg::LaserScan>(
      "/sensor/lidar", rclcpp::QoS(10).best_effort());
  auto imu_pub = fusion->create_publisher<sensor_msgs::msg::Imu>(
      "/sensor/imu", rclcpp::QoS(10).reliable());
  auto cam_pub = fusion->create_publisher<sensor_msgs::msg::Image>(
      "/sensor/camera", rclcpp::QoS(10).best_effort());
  lidar_pub->on_activate();
  imu_pub->on_activate();
  cam_pub->on_activate();

  // When: all three sensors are published
  lidar_pub->publish(make_lidar(1.5F, 10.0F, 180, 10));
  imu_pub->publish(make_imu());
  cam_pub->publish(make_camera());

  ASSERT_TRUE(spin_until(fusion->get_node_base_interface(),
                         [&last_output] { return last_output != nullptr; },
                         std::chrono::seconds(3)));

  // Then: at least 1 object detected from the near-range cluster
  EXPECT_GE(last_output->objects.size(), 1u);
  if (!last_output->objects.empty()) {
    EXPECT_EQ(last_output->objects[0].z, 0.0F);
  }
}

// ===================================================================
// Fault injection tests — M2.4 传感器降级验证
//
// 注意：LifecycleNode 的 callback 和 timer 必须有 executor 在 spin 才会执行。
// rclcpp::sleep_for 只是阻塞当前线程，不驱动事件循环。
// 所有降级测试必须使用 spin_until 或手动 spin loop。
// ===================================================================

// ✓ All sensors present → FULL ─────────────────────────────────────────

TEST_F(RobotMiddlewareTest, FusionNode_AllSensorsPresent_DegradationFull) {
  auto fusion = std::make_shared<FusionNode>();
  fusion->configure();
  fusion->activate();

  auto lidar_pub = fusion->create_publisher<sensor_msgs::msg::LaserScan>(
      "/sensor/lidar", rclcpp::QoS(10).best_effort());
  auto imu_pub = fusion->create_publisher<sensor_msgs::msg::Imu>(
      "/sensor/imu", rclcpp::QoS(10).reliable());
  auto cam_pub = fusion->create_publisher<sensor_msgs::msg::Image>(
      "/sensor/camera", rclcpp::QoS(10).best_effort());
  lidar_pub->on_activate();
  imu_pub->on_activate();
  cam_pub->on_activate();

  lidar_pub->publish(make_lidar(2.0F, 10.0F, 180, 10));
  imu_pub->publish(make_imu());
  cam_pub->publish(make_camera());

  // spin_until: 让 executor 处理回调 + 至少一次 timer（200ms）
  bool degraded = false;
  spin_until(fusion->get_node_base_interface(),
             [&degraded, &fusion] {
               if (fusion->degradation_level() != FusionNode::DegradationLevel::FULL) {
                 degraded = true;
               }
               return degraded;  // 退出条件：出现降级（本测试期望不出现）
             },
             std::chrono::milliseconds(500));
  // 500ms 内全部传感器都有数据，不应降级
  EXPECT_FALSE(degraded);
  EXPECT_EQ(fusion->degradation_level(), FusionNode::DegradationLevel::FULL);
}

// ✓ IMU stale → NO_IMU ─────────────────────────────────────────────────

TEST_F(RobotMiddlewareTest, FusionNode_ImuMissing_DegradedToNoImu) {
  auto fusion = std::make_shared<FusionNode>();
  fusion->configure();
  fusion->activate();

  auto lidar_pub = fusion->create_publisher<sensor_msgs::msg::LaserScan>(
      "/sensor/lidar", rclcpp::QoS(10).best_effort());
  auto cam_pub = fusion->create_publisher<sensor_msgs::msg::Image>(
      "/sensor/camera", rclcpp::QoS(10).best_effort());
  lidar_pub->on_activate();
  cam_pub->on_activate();

  // Publish lidar + camera（IMU 不发 → 将超时）
  lidar_pub->publish(make_lidar(2.0F, 10.0F, 180, 10));
  cam_pub->publish(make_camera());

  // spin executor，让 timer 定期 evaluate_degradation()
  // IMU timeout = 0.5s, timer = 200ms → 最多 700ms 后应检测到 NO_IMU
  // 但必须在 1.5s 内（否则 lidar 也会 stale → CRITICAL）
  bool imu_degraded = false;
  ASSERT_TRUE(spin_until(fusion->get_node_base_interface(),
                          [&imu_degraded, &fusion] {
                            imu_degraded =
                                (fusion->degradation_level() == FusionNode::DegradationLevel::NO_IMU);
                            return imu_degraded;
                          },
                          std::chrono::milliseconds(1200)));

  EXPECT_TRUE(imu_degraded);
  EXPECT_EQ(fusion->degradation_level(), FusionNode::DegradationLevel::NO_IMU);
}

// ✓ LiDAR stale → NO_LIDAR（需要持续刷新 IMU/Camera 避免它们也不新鲜）───

TEST_F(RobotMiddlewareTest, FusionNode_LidarMissing_DegradedToNoLidar) {
  auto fusion = std::make_shared<FusionNode>();
  fusion->configure();
  fusion->activate();

  auto imu_pub = fusion->create_publisher<sensor_msgs::msg::Imu>(
      "/sensor/imu", rclcpp::QoS(10).reliable());
  auto cam_pub = fusion->create_publisher<sensor_msgs::msg::Image>(
      "/sensor/camera", rclcpp::QoS(10).best_effort());
  imu_pub->on_activate();
  cam_pub->on_activate();

  // 手动 spin loop：先发数据再 spin，保证 subscription callback 先于 timer 处理
  // 关键：spin 间隔必须短于最短 timeout（0.5s IMU），否则 IMU callback 积压导致误判
  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(fusion->get_node_base_interface());
  auto start    = std::chrono::steady_clock::now();
  auto last_pub = start;
  auto result   = FusionNode::DegradationLevel::FULL;

  while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(2500)) {
    // 每 250ms 发一次数据（IMU timeout=0.5s, 250ms < 500ms 保证新鲜）
    if (std::chrono::steady_clock::now() - last_pub > std::chrono::milliseconds(250)) {
      imu_pub->publish(make_imu());
      cam_pub->publish(make_camera());
      last_pub = std::chrono::steady_clock::now();
    }
    // spin 间隔要短，确保每次 publish 后的 callback 被及时处理
    exec.spin_once(std::chrono::milliseconds(50));
    result = fusion->degradation_level();
    if (result == FusionNode::DegradationLevel::NO_LIDAR) break;
  }
  exec.remove_node(fusion->get_node_base_interface());

  EXPECT_EQ(result, FusionNode::DegradationLevel::NO_LIDAR);
}

// ✓ IMU + Camera stale → CRITICAL ──────────────────────────────────────

TEST_F(RobotMiddlewareTest, FusionNode_ImuAndCameraMissing_DegradedToCritical) {
  auto fusion = std::make_shared<FusionNode>();
  fusion->configure();
  fusion->activate();

  auto lidar_pub = fusion->create_publisher<sensor_msgs::msg::LaserScan>(
      "/sensor/lidar", rclcpp::QoS(10).best_effort());
  lidar_pub->on_activate();

  // 手动 spin loop：只刷新 LiDAR，IMU(0.5s) + Camera(3.0s) 自然超时
  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(fusion->get_node_base_interface());
  auto start    = std::chrono::steady_clock::now();
  auto last_pub = start;
  auto result   = FusionNode::DegradationLevel::FULL;

  while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(4000)) {
    // 每 200ms 发 LiDAR 数据保持新鲜，IMU/Camera 不发 → 自然超时
    if (std::chrono::steady_clock::now() - last_pub > std::chrono::milliseconds(200)) {
      lidar_pub->publish(make_lidar(2.0F, 10.0F, 180, 10));
      last_pub = std::chrono::steady_clock::now();
    }
    exec.spin_once(std::chrono::milliseconds(50));
    result = fusion->degradation_level();
    if (result == FusionNode::DegradationLevel::CRITICAL) break;
  }
  exec.remove_node(fusion->get_node_base_interface());

  EXPECT_EQ(result, FusionNode::DegradationLevel::CRITICAL);
}
