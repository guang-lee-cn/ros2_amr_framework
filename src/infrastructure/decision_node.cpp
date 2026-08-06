#include "ros2_robot_middleware/infrastructure/aliases.hpp"
#include "ros2_robot_middleware/infrastructure/decision_node.hpp"
#include "generated/perf_instrumentation.hpp"
#include "ros2_robot_middleware/observability/metrics_registry.hpp"
#include "ros2_robot_middleware/observability/trace_points.hpp"
#include "ros2_robot_middleware/observability/tracer.hpp"

#include "tf2/exceptions.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <rclcpp_components/register_node_macro.hpp>

// ── Constructors ────────────────────────────────────────────────────────────

DecisionNode::DecisionNode()
  : rclcpp_lifecycle::LifecycleNode("decision"),
    astar_(amr::domain::planning::AStarPlanner::Params{50000, 1.0F})
{
  // Task-derived navigation goal (from launch params / fleet manager).
  // Perception objects are obstacles, NOT the goal — see on_perception.
  this->declare_parameter<float>("goal_x", 0.0F);
  this->declare_parameter<float>("goal_y", 0.0F);
}

DecisionNode::DecisionNode(const rclcpp::NodeOptions &options)
  : rclcpp_lifecycle::LifecycleNode("decision", options),
    astar_(amr::domain::planning::AStarPlanner::Params{50000, 1.0F}) {
  this->declare_parameter<float>("goal_x", 0.0F);
  this->declare_parameter<float>("goal_y", 0.0F);
}

// ── Lifecycle callbacks ──────────────────────────────────────────────────────

DecisionNode::CallbackReturn
DecisionNode::on_configure(const rclcpp_lifecycle::State &)
{
  // Own callback group so perception/pose subscriptions are not starved by
  // the sibling motor_ctrl action server or the high-rate AMCL pose stream.
  // Reentrant: on_amcl_pose (high-rate) must not serialize-block on_perception.
  perception_cb_group_ = this->create_callback_group(rclcpp::CallbackGroupType::Reentrant);
  rclcpp::SubscriptionOptions decision_opts;
  decision_opts.callback_group = perception_cb_group_;
  decision_sub_ = this->create_subscription<PerceptionObjects>(
    "/perception/objects", rclcpp::QoS(10).reliable(),
    [this](PerceptionObjects::SharedPtr msg) { on_perception(msg); },
    decision_opts);

  // Robot pose in the map frame (AMCL), used as the A* start — G1c.
  rclcpp::SubscriptionOptions amcl_opts;
  amcl_opts.callback_group = perception_cb_group_;
  amcl_pose_sub_ = this->create_subscription<geometry_msgs::msg::PoseWithCovarianceStamped>(
    "/amcl_pose", rclcpp::QoS(10).reliable(),
    [this](geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr msg) { on_amcl_pose(msg); },
    amcl_opts);

  // map→odom TF buffer (AMCL publishes it) — used to dispatch goals in the
  // motor's odom/world frame while planning in the map frame.
  tf_buffer_ = std::make_unique<tf2_ros::Buffer>(this->get_clock());
  tf_listener_ = std::make_unique<tf2_ros::TransformListener>(*tf_buffer_);

  client_ = rclcpp_action::create_client<MoveToPose>(this, "/cmd/move_to_pose");

  heartbeat_pub_ = this->create_publisher<std_msgs::msg::String>(
    "/decision/heartbeat", rclcpp::QoS(10).reliable());

  path_pub_ = this->create_publisher<geometry_msgs::msg::PoseArray>(
    "/planning/path", rclcpp::QoS(10).reliable());

  // Initialize OccupancyGrid (400×400 grid, 5cm resolution, 20m×20m).
  // G1c: A* now plans in the map frame — warehouse obstacles sit at
  // map coords x[8,15] y[6,15], so a 20m grid is required (10m overflowed).
  demo_grid_.width = 400;
  demo_grid_.height = 400;
  demo_grid_.resolution = 0.05F;
  demo_grid_.origin = {0.0F, 0.0F};
  demo_grid_.cells.assign(400 * 400, false);

  return CallbackReturn::SUCCESS;
}

DecisionNode::CallbackReturn
DecisionNode::on_activate(const rclcpp_lifecycle::State &)
{
  using namespace std::chrono_literals;
  heartbeat_timer_ = this->create_wall_timer(1s, [this]() {
    auto msg = std_msgs::msg::String{};
    msg.data = "alive";
    heartbeat_pub_->publish(msg);
  });
  heartbeat_pub_->on_activate();
  path_pub_->on_activate();
  return CallbackReturn::SUCCESS;
}

DecisionNode::CallbackReturn
DecisionNode::on_deactivate(const rclcpp_lifecycle::State &)
{
  heartbeat_timer_.reset();
  heartbeat_pub_->on_deactivate();
  path_pub_->on_deactivate();
  return CallbackReturn::SUCCESS;
}

DecisionNode::CallbackReturn
DecisionNode::on_cleanup(const rclcpp_lifecycle::State &)
{
  decision_sub_.reset();
  amcl_pose_sub_.reset();
  client_.reset();
  heartbeat_pub_.reset();
  path_pub_.reset();
  tf_listener_.reset();
  tf_buffer_.reset();
  return CallbackReturn::SUCCESS;
}

DecisionNode::CallbackReturn
DecisionNode::on_shutdown(const rclcpp_lifecycle::State &)
{
  heartbeat_timer_.reset();
  decision_sub_.reset();
  amcl_pose_sub_.reset();
  client_.reset();
  heartbeat_pub_.reset();
  path_pub_.reset();
  tf_listener_.reset();
  tf_buffer_.reset();
  return CallbackReturn::SUCCESS;
}

// ── Perception callback — delegates to PlanningService ───────────────────────

void DecisionNode::on_perception(const PerceptionObjects::SharedPtr& objs)
{
  AMR_PERF_PHASE("decision:on_perception");
  TRACE_SCOPE(amr::trace::DECISION_ON_PERCEPTION);
  auto t_start = std::chrono::steady_clock::now();

  auto &m = amr::observability::shared_metrics();
  m.object_count.store(static_cast<int32_t>(objs->objects.size()),
                       std::memory_order_relaxed);

  // 1. Perception objects are OBSTACLES — mark all of them into the grid.
  //    (Regression: objects[0] was picked as the navigation goal via
  //    TargetSelector, so in a static scene the robot chased the nearest
  //    wall/rack forever. The goal now comes from the task layer below.)
  std::fill(demo_grid_.cells.begin(), demo_grid_.cells.end(), false);
  amr::domain::planning::PerceivedObject obstacles[8];
  std::size_t n_obs = 0;
  for (std::size_t i = 0; i < objs->objects.size() && n_obs < 8; ++i) {
    obstacles[n_obs++] = {objs->objects[i].x, objs->objects[i].y,
                          objs->objects[i].id.c_str()};
  }
  if (n_obs > 0) {
    grid_updater_.mark_obstacles(demo_grid_, obstacles, n_obs,
                                 std::numeric_limits<std::size_t>::max());
  }

  // 2. Task-derived goal (launch param / fleet manager), NOT perception.
  const float gx = static_cast<float>(this->get_parameter("goal_x").as_double());
  const float gy = static_cast<float>(this->get_parameter("goal_y").as_double());

  // 3. A* from the real robot pose (fallback origin pre-odom).
  amr::domain::planning::Pose start;
  {
    std::lock_guard<std::mutex> lock(pose_mutex_);
    start = current_pose_;
  }
  amr::domain::planning::Pose goal_pose{gx, gy};

  // 4. Task complete? Robot already within arrival tolerance of the goal —
  //    do not re-dispatch the same goal (prevents the restart-overshoot loop).
  constexpr float kArrivalTolerance = 0.15F;  // > PurePursuit goal_tolerance (0.1)
  const float start_to_goal = std::hypot(start.x - gx, start.y - gy);
  if (start_to_goal < kArrivalTolerance) {
    return;  // already there
  }

  auto path = astar_.plan(demo_grid_, start, goal_pose);
  if (path.empty()) {
    // Goal blocked: no useless dispatch; re-plan on the next perception
    // cycle once the obstacle clears. An in-flight goal is kept.
    return;
  }

  // 4. Goal lock: perception noise must not preempt an executing goal.
  if (active_goal_) return;

  { AMR_PERF_PHASE("decision:astar");
    auto smooth = smoother_.smooth(path);
    publish_path(smooth);
  }
  // 5. Dispatch in the motor's frame: decision plans in map (start = AMCL
  //    map pose, goal = map params) but motor tracks /odom (= world). AMCL
  //    publishes map→odom — transform the goal before dispatching. Without
  //    the TF the goal would be read as world coordinates and the robot
  //    would drive out of the warehouse (G1 coordinate mismatch).
  float odom_gx = gx, odom_gy = gy;
  if (tf_buffer_) {
    try {
      const auto t = tf_buffer_->lookupTransform(
          "amr/odom", "map", rclcpp::Time(0),
          rclcpp::Duration::from_seconds(0.1));
      // 2D map→odom transform applied to the goal point.
      const double yaw = 2.0 * std::atan2(t.transform.rotation.z,
                                          t.transform.rotation.w);
      const double c = std::cos(yaw), s = std::sin(yaw);
      odom_gx = static_cast<float>(t.transform.translation.x + c * gx - s * gy);
      odom_gy = static_cast<float>(t.transform.translation.y + s * gx + c * gy);
    } catch (const tf2::TransformException &e) {
      RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                           "map→odom TF unavailable, deferring dispatch: %s",
                           e.what());
      return;  // cannot dispatch in the motor's frame yet
    }
  }
  { AMR_PERF_PHASE("decision:send_goal");
    send_goal(odom_gx, odom_gy); }

  auto lat_us = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - t_start).count();
  m.decision_latency.record(lat_us);
}

void DecisionNode::on_amcl_pose(
    const geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr msg)
{
  std::lock_guard<std::mutex> lock(pose_mutex_);
  current_pose_.x = static_cast<float>(msg->pose.pose.position.x);
  current_pose_.y = static_cast<float>(msg->pose.pose.position.y);
  has_pose_.store(true, std::memory_order_release);
}

// ── Action client wiring (ROS2-specific, stays in Node) ─────────────────────

void DecisionNode::send_goal(float target_x, float target_y)
{
  if (!client_->action_server_is_ready()) {
    RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                         "Action server not available");
    return;
  }

  last_target_x_ = target_x;
  last_target_y_ = target_y;

  auto goal         = MoveToPose::Goal{};
  goal.target_x     = target_x;
  goal.target_y     = target_y;
  goal.target_theta = 0.0F;
  goal.max_speed    = 0.5F;

  auto opts = rclcpp_action::Client<MoveToPose>::SendGoalOptions{};
  opts.goal_response_callback =
    [this](ClientGoalHandle::SharedPtr gh) { on_goal_response(gh); };
  opts.result_callback =
    [this](const ClientGoalHandle::WrappedResult& r) { on_result(r); };

  client_->async_send_goal(goal, opts);
}

void DecisionNode::cancel_active_goal()
{
  if (!active_goal_) return;
  RCLCPP_INFO(this->get_logger(), "Preempting previous goal");
  client_->async_cancel_goal(active_goal_);
  active_goal_.reset();
  retry_count_ = 0;
}

void DecisionNode::on_goal_response(const ClientGoalHandle::SharedPtr& goal_handle)
{
  if (!goal_handle) {
    if (selector_.should_retry(retry_count_)) {
      retry_count_++;
      RCLCPP_WARN(this->get_logger(), "Goal rejected, retrying %d/%d (%.2f, %.2f)",
                   retry_count_, amr::domain::planning::TargetSelector::kMaxRetries,
                   last_target_x_, last_target_y_);
      send_goal(last_target_x_, last_target_y_);
    } else {
      RCLCPP_ERROR(this->get_logger(), "Goal rejected after retries, giving up");
      retry_count_ = 0;
    }
    return;
  }
  active_goal_ = goal_handle;
  retry_count_ = 0;
  RCLCPP_INFO(this->get_logger(), "Goal accepted by motor_ctrl");
}

void DecisionNode::on_result(const ClientGoalHandle::WrappedResult& result)
{
  active_goal_.reset();
  switch (result.code) {
    case rclcpp_action::ResultCode::SUCCEEDED:
      RCLCPP_INFO(this->get_logger(), "MoveToPose succeeded: reached (%.2f, %.2f)",
                  result.result->final_x, result.result->final_y);
      break;
    case rclcpp_action::ResultCode::CANCELED:
      RCLCPP_INFO(this->get_logger(), "MoveToPose canceled");
      break;
    default:
      RCLCPP_ERROR(this->get_logger(), "MoveToPose failed");
      break;
  }
}

void DecisionNode::publish_path(const std::vector<amr::domain::planning::Waypoint> &path) {
  auto msg = geometry_msgs::msg::PoseArray{};
  msg.header.stamp = this->now();
  msg.header.frame_id = "map";
  for (const auto &wp : path) {
    auto pose = geometry_msgs::msg::Pose{};
    pose.position.x = wp.x;
    pose.position.y = wp.y;
    pose.position.z = 0.0;
    pose.orientation.w = 1.0;
    msg.poses.push_back(pose);
  }
  path_pub_->publish(msg);
}

RCLCPP_COMPONENTS_REGISTER_NODE(DecisionNode)
