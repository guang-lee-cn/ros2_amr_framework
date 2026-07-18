#include <rclcpp_components/register_node_macro.hpp>
#include "ros2_robot_middleware/infrastructure/motor_ctrl_node.hpp"
#include "ros2_robot_middleware/infrastructure/aliases.hpp"
#include "ros2_robot_middleware/observability/metrics_registry.hpp"
#include "ros2_robot_middleware/observability/tracer.hpp"

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

  return CallbackReturn::SUCCESS;
}

MotorCtrlNode::CallbackReturn
MotorCtrlNode::on_deactivate(const rclcpp_lifecycle::State &)
{
  status_timer_.reset();
  status_pub_->on_deactivate();

  return CallbackReturn::SUCCESS;
}

MotorCtrlNode::CallbackReturn
MotorCtrlNode::on_cleanup(const rclcpp_lifecycle::State &)
{
  action_server_.reset();
  service_server_.reset();
  status_pub_.reset();

  return CallbackReturn::SUCCESS;
}

MotorCtrlNode::CallbackReturn
MotorCtrlNode::on_shutdown(const rclcpp_lifecycle::State &)
{
  status_timer_.reset();
  action_server_.reset();
  service_server_.reset();
  status_pub_.reset();

  return CallbackReturn::SUCCESS;
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
  TRACE_SCOPE("motor::execute");

  const auto goal= goal_handle->get_goal();
  amr::domain::execution::Interpolator::State current{0.0F, 0.0F};
  amr::domain::execution::Interpolator::State target{goal->target_x, goal->target_y};

  rclcpp::Rate rate(10);

  while (rclcpp::ok()) {
    auto step_start = std::chrono::steady_clock::now();

    if (goal_handle->is_canceling()) {
      auto result          = std::make_shared<MoveToPose::Result>();
      result->reached      = false;
      result->final_x      = current.x;
      result->final_y      = current.y;
      result->elapsed_time = 0;
      goal_handle->canceled(result);
      RCLCPP_INFO(this->get_logger(), "Goal canceled");
      return;
    }

    amr::domain::execution::Interpolator::Feedback fb;
    bool reached = execution_.step(target, current, &fb);

    if (reached) {
      auto result          = std::make_shared<MoveToPose::Result>();
      result->reached      = true;
      result->final_x      = current.x;
      result->final_y      = current.y;
      result->elapsed_time = 0;
      goal_handle->succeed(result);
      RCLCPP_INFO(this->get_logger(), "Goal reached: (%.2f, %.2f)", current.x, current.y);
      return;
    }

    auto feedback                = std::make_shared<MoveToPose::Feedback>();
    feedback->current_x          = fb.current_x;
    feedback->current_y          = fb.current_y;
    feedback->distance_remaining = fb.distance_remaining;
    feedback->percent_complete   = fb.percent_complete;

    goal_handle->publish_feedback(feedback);

    // Observability: per-step motor latency + e2e (sensor→cmd)
    auto now_ns = std::chrono::steady_clock::now();
    auto lat_us = std::chrono::duration_cast<std::chrono::microseconds>(
                      now_ns - step_start).count();
    auto &m = amr::observability::shared_metrics();
    m.motor_latency.record(lat_us);

    auto sensor_ts = m.last_sensor_timestamp_ns.load(std::memory_order_relaxed);
    if (sensor_ts > 0) {
        auto e2e_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                          now_ns.time_since_epoch()).count() - sensor_ts;
        if (e2e_ns > 0 && e2e_ns < 5'000'000'000LL) { // sanity: < 5 seconds
            m.e2e_latency.record(e2e_ns / 1000);       // ns → μs
        }
    }

    rate.sleep();
  }
}

void MotorCtrlNode::handle_set_param(
  const std::shared_ptr<SetParam::Request> request,
  std::shared_ptr<SetParam::Response> response)
{
  RCLCPP_INFO(this->get_logger(),
              "SetParam: %s = %.4f", request->param_name.c_str(), request->value);
  if (request->param_name == "step_size") {
    execution_.set_step_size(static_cast<float>(request->value));
    response->message = "Parameter updated";
  } else {
    response->message = "Unknown parameter";
  }
  response->success = true;
}

RCLCPP_COMPONENTS_REGISTER_NODE(MotorCtrlNode)
