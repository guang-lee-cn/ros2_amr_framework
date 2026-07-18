#include "ros2_robot_middleware/infrastructure/aliases.hpp"
#include "ros2_robot_middleware/infrastructure/decision_node.hpp"
#include "ros2_robot_middleware/observability/metrics_registry.hpp"
#include "ros2_robot_middleware/observability/tracer.hpp"

#include <chrono>
#include <rclcpp_components/register_node_macro.hpp>

// ── Constructors ────────────────────────────────────────────────────────────

DecisionNode::DecisionNode()
  : rclcpp_lifecycle::LifecycleNode("decision")
{
}

DecisionNode::DecisionNode(const rclcpp::NodeOptions &options)
  : rclcpp_lifecycle::LifecycleNode("decision", options) {
}

// ── Lifecycle callbacks ──────────────────────────────────────────────────────

DecisionNode::CallbackReturn
DecisionNode::on_configure(const rclcpp_lifecycle::State &)
{
  decision_sub_ = this->create_subscription<PerceptionObjects>(
    "/perception/objects", rclcpp::QoS(10).reliable(),
    [this](PerceptionObjects::SharedPtr msg) { on_perception(msg); });

  client_ = rclcpp_action::create_client<MoveToPose>(this, "/cmd/move_to_pose");

  heartbeat_pub_ = this->create_publisher<std_msgs::msg::String>(
    "/decision/heartbeat", rclcpp::QoS(10).reliable());

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
  return CallbackReturn::SUCCESS;
}

DecisionNode::CallbackReturn
DecisionNode::on_deactivate(const rclcpp_lifecycle::State &)
{
  heartbeat_timer_.reset();
  heartbeat_pub_->on_deactivate();
  return CallbackReturn::SUCCESS;
}

DecisionNode::CallbackReturn
DecisionNode::on_cleanup(const rclcpp_lifecycle::State &)
{
  decision_sub_.reset();
  client_.reset();
  heartbeat_pub_.reset();
  return CallbackReturn::SUCCESS;
}

DecisionNode::CallbackReturn
DecisionNode::on_shutdown(const rclcpp_lifecycle::State &)
{
  heartbeat_timer_.reset();
  decision_sub_.reset();
  client_.reset();
  heartbeat_pub_.reset();
  return CallbackReturn::SUCCESS;
}

// ── Perception callback — delegates to PlanningService ───────────────────────

void DecisionNode::on_perception(const PerceptionObjects::SharedPtr& objs)
{
  TRACE_SCOPE("decision::on_perception");
  auto t_start = std::chrono::steady_clock::now();

  if (objs->objects.empty()) return;

  auto &m = amr::observability::shared_metrics();
  m.object_count.store(static_cast<int32_t>(objs->objects.size()),
                       std::memory_order_relaxed);

  if (planning_.should_preempt(active_goal_ != nullptr)) {
    cancel_active_goal();
  }

  amr::domain::planning::PerceivedObject obj{objs->objects[0].x,
                                              objs->objects[0].y,
                                               objs->objects[0].id.c_str()};
  amr::domain::planning::Goal goal;
  if (planning_.select_goal(&obj, 1, goal)) {
    send_goal(goal.x, goal.y);
  }

  auto lat_us = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - t_start).count();
  m.decision_latency.record(lat_us);
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
    if (planning_.should_retry(retry_count_)) {
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

RCLCPP_COMPONENTS_REGISTER_NODE(DecisionNode)
