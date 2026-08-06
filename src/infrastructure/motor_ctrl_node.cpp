#include <rclcpp_components/register_node_macro.hpp>
#include "ros2_robot_middleware/infrastructure/motor_ctrl_node.hpp"
#include "ros2_robot_middleware/infrastructure/aliases.hpp"
#include "generated/perf_instrumentation.hpp"
#include "ros2_robot_middleware/observability/metrics_registry.hpp"
#include "ros2_robot_middleware/observability/tracer.hpp"
#include "geometry_msgs/msg/twist.hpp"

#include <chrono>
#include <cmath>

MotorCtrlNode::MotorCtrlNode()
  : rclcpp_lifecycle::LifecycleNode("motor_ctrl")
{
}

MotorCtrlNode::MotorCtrlNode(const rclcpp::NodeOptions &options)
  : rclcpp_lifecycle::LifecycleNode("motor_ctrl", options) {
}

MotorCtrlNode::CallbackReturn
MotorCtrlNode::on_configure(const rclcpp_lifecycle::State &)
{
  action_server_ = rclcpp_action::create_server<MoveToPose>(
    this, "/cmd/move_to_pose",
    [this](const rclcpp_action::GoalUUID &uuid, std::shared_ptr<const MoveToPose::Goal> goal) {
      return handle_goal(uuid, goal);
    },
    [this](const std::shared_ptr<ServerGoalHandle> goal_handle) {
      return handle_cancel(goal_handle);
    },
    [this](const std::shared_ptr<ServerGoalHandle> goal_handle) {
      execute(goal_handle);
    });

  service_server_ = this->create_service<SetParam>(
    "/cmd/set_param",
    [this](const std::shared_ptr<SetParam::Request> req,
           std::shared_ptr<SetParam::Response> resp) {
      handle_set_param(req, resp);
    });

  status_pub_ = this->create_publisher<std_msgs::msg::String>(
    "/cmd/status", rclcpp::QoS(10).reliable());

  // Velocity command to the base (DiffDrive in sim, IActuator in prod)
  cmd_vel_pub_ = this->create_publisher<geometry_msgs::msg::Twist>(
    "/cmd_vel", rclcpp::QoS(10).reliable());

  // Subscribe to /odom (robot_localization EKF) — closed-loop pose source
  odom_sub_ = this->create_subscription<nav_msgs::msg::Odometry>(
    "/odom", rclcpp::QoS(10).reliable(),
    [this](nav_msgs::msg::Odometry::SharedPtr msg) { on_odom(msg); });

  return CallbackReturn::SUCCESS;
}

MotorCtrlNode::CallbackReturn
MotorCtrlNode::on_activate(const rclcpp_lifecycle::State &)
{
  using namespace std::chrono_literals;
  status_timer_ = this->create_wall_timer(1s, [this]() {
    auto msg = std_msgs::msg::String{};
    msg.data = "idle";
    status_pub_->publish(msg);
  });

  status_pub_->on_activate();
  cmd_vel_pub_->on_activate();

  return CallbackReturn::SUCCESS;
}

MotorCtrlNode::CallbackReturn
MotorCtrlNode::on_deactivate(const rclcpp_lifecycle::State &)
{
  status_timer_.reset();
  status_pub_->on_deactivate();
  cmd_vel_pub_->on_deactivate();

  return CallbackReturn::SUCCESS;
}

MotorCtrlNode::CallbackReturn
MotorCtrlNode::on_cleanup(const rclcpp_lifecycle::State &)
{
  action_server_.reset();
  service_server_.reset();
  status_pub_.reset();
  cmd_vel_pub_.reset();
  odom_sub_.reset();

  return CallbackReturn::SUCCESS;
}

MotorCtrlNode::CallbackReturn
MotorCtrlNode::on_shutdown(const rclcpp_lifecycle::State &)
{
  status_timer_.reset();
  action_server_.reset();
  service_server_.reset();
  status_pub_.reset();
  cmd_vel_pub_.reset();
  odom_sub_.reset();

  return CallbackReturn::SUCCESS;
}

void MotorCtrlNode::on_odom(const nav_msgs::msg::Odometry::SharedPtr msg)
{
  std::lock_guard<std::mutex> lock(pose_mutex_);
  current_pose_.x = static_cast<float>(msg->pose.pose.position.x);
  current_pose_.y = static_cast<float>(msg->pose.pose.position.y);
  // Extract yaw from quaternion (z = sin(yaw/2))
  const auto &q = msg->pose.pose.orientation;
  current_pose_.theta = static_cast<float>(
      2.0 * std::atan2(q.z, q.w));
  odom_valid_.store(true, std::memory_order_release);
}

rclcpp_action::GoalResponse MotorCtrlNode::handle_goal(
  const rclcpp_action::GoalUUID &,
  std::shared_ptr<const MoveToPose::Goal> goal)
{
  RCLCPP_INFO(this->get_logger(),
              "Received goal: target=(%.2f, %.2f, %.2f) speed=%.2f",
              goal->target_x, goal->target_y, goal->target_theta, goal->max_speed);
  return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
}

rclcpp_action::CancelResponse MotorCtrlNode::handle_cancel(
  const std::shared_ptr<ServerGoalHandle>)
{
  RCLCPP_INFO(this->get_logger(), "Cancel request received");
  return rclcpp_action::CancelResponse::ACCEPT;
}

void MotorCtrlNode::execute(const std::shared_ptr<ServerGoalHandle> goal_handle)
{
  AMR_PERF_PHASE("motor:execute");
  TRACE_SCOPE("motor::execute");

  const auto goal = goal_handle->get_goal();
  using amr::domain::execution::Pose2D;
  using amr::domain::execution::Waypoint;

  // Use odom-fused pose as the current state; fall back to (0,0,0) pre-odom.
  Pose2D current;
  {
    std::lock_guard<std::mutex> lock(pose_mutex_);
    current = current_pose_;
  }
  Waypoint target{goal->target_x, goal->target_y};

  // 2-point path: current position → goal
  std::vector<Waypoint> path = {{current.x, current.y}, target};
  float total_dist = std::sqrt(goal->target_x * goal->target_x + goal->target_y * goal->target_y);

  rclcpp::Rate rate(20);  // 20 Hz control loop

  while (rclcpp::ok()) {
    auto step_start = std::chrono::steady_clock::now();

    // Read latest odom pose (closed-loop). Lock briefly, copy.
    {
      std::lock_guard<std::mutex> lock(pose_mutex_);
      current = current_pose_;
    }

    if (goal_handle->is_canceling()) {
      auto result = std::make_shared<MoveToPose::Result>();
      result->reached = false;
      result->final_x = current.x;
      result->final_y = current.y;
      result->elapsed_time = 0;
      goal_handle->canceled(result);
      RCLCPP_INFO(this->get_logger(), "Goal canceled");
      return;
    }

    // Pure Pursuit tracking (uses real odom pose, not self-integrated)
    auto twist = tracker_.track(path, current);

    if (twist.linear == 0.0F && twist.angular == 0.0F) {
      // Command a full stop before reporting arrival — the base keeps its
      // last speed until it receives a new message.
      publish_twist(0.0F, 0.0F);
      auto result = std::make_shared<MoveToPose::Result>();
      result->reached = true;
      result->final_x = current.x;
      result->final_y = current.y;
      result->elapsed_time = 0;
      goal_handle->succeed(result);
      RCLCPP_INFO(this->get_logger(), "Goal reached: (%.2f, %.2f)", current.x, current.y);
      return;
    }

    // Lateral tracking error monitor: scale speed by deviation from path.
    auto err = error_monitor_.evaluate(current, path);
    if (err.level == amr::domain::planning::TrackErrorLevel::ERROR) {
      RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 2000,
                           "Track error %.2fm beyond stop threshold — stopping",
                           err.lateral_error);
      twist.linear = 0.0F;
      twist.angular = 0.0F;
    } else if (err.level == amr::domain::planning::TrackErrorLevel::WARN) {
      twist.linear *= err.speed_scale;  // slow down when deviating
    }

    // Publish the velocity command to the base (closed loop last hop).
    publish_twist(twist.linear, twist.angular);

    // Pose advance: closed-loop when /odom is available; otherwise fall back
    // to kinematic integration (simulation/demo mode without a real base).
    if (!odom_valid_.load(std::memory_order_acquire)) {
      const float dt = 1.0F / 20.0F;
      current.x += twist.linear * std::cos(current.theta) * dt;
      current.y += twist.linear * std::sin(current.theta) * dt;
      current.theta += twist.angular * dt;
      // Keep the fused pose in sync so odom arrival doesn't jump backwards.
      std::lock_guard<std::mutex> lock(pose_mutex_);
      current_pose_ = current;
    }
    // Update path start to current position (closing the gap)
    path[0] = {current.x, current.y};

    // Feedback
    float dx = target.x - current.x;
    float dy = target.y - current.y;
    float remaining = std::sqrt(dx * dx + dy * dy);

    auto feedback = std::make_shared<MoveToPose::Feedback>();
    feedback->current_x = current.x;
    feedback->current_y = current.y;
    feedback->distance_remaining = remaining;
    feedback->percent_complete = total_dist > 0.0F
      ? (1.0F - remaining / total_dist) * 100.0F
      : 100.0F;

    goal_handle->publish_feedback(feedback);

    // Observability
    auto now_ns = std::chrono::steady_clock::now();
    auto &m = amr::observability::shared_metrics();
    auto lat_us = std::chrono::duration_cast<std::chrono::microseconds>(
                      now_ns - step_start).count();
    m.motor_latency.record(lat_us);

    auto sensor_ts = m.last_sensor_timestamp_ns.load(std::memory_order_relaxed);
    if (sensor_ts > 0) {
      auto e2e_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        now_ns.time_since_epoch()).count() - sensor_ts;
      if (e2e_ns > 0 && e2e_ns < 5'000'000'000LL) {
        m.e2e_latency.record(e2e_ns / 1000);
      }
    }

    rate.sleep();
  }
}

void MotorCtrlNode::publish_twist(float linear, float angular) {
  geometry_msgs::msg::Twist msg;
  msg.linear.x = linear;
  msg.angular.z = angular;
  cmd_vel_pub_->publish(msg);
}

void MotorCtrlNode::handle_set_param(
  const std::shared_ptr<SetParam::Request> request,
  std::shared_ptr<SetParam::Response> response)
{
  RCLCPP_INFO(this->get_logger(),
              "SetParam: %s = %.4f", request->param_name.c_str(), request->value);
  // Known parameters are acknowledged. PurePursuit params are constructor-
  // configured (max_linear via Params) — runtime tuning via this service is
  // a compatibility stub for now.
  if (request->param_name == "step_size") {
    response->message = "Parameter updated";
  } else {
    response->message = "Unknown parameter";
  }
  response->success = true;
}

RCLCPP_COMPONENTS_REGISTER_NODE(MotorCtrlNode)
