#include "ros2_robot_middleware/infrastructure/aliases.hpp"
#include "ros2_robot_middleware/infrastructure/decision_node.hpp"
#include "ros2_robot_middleware/domain/perception/degradation_policy.hpp"
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
    astar_(amr::domain::planning::AStarPlanner::Params{200000, 1.0F, 0.75F})
{
  // Task-derived navigation goal (from launch params / fleet manager).
  // Perception objects are obstacles, NOT the goal — see on_perception.
  this->declare_parameter<float>("goal_x", 0.0F);
  this->declare_parameter<float>("goal_y", 0.0F);
}

DecisionNode::DecisionNode(const rclcpp::NodeOptions &options)
  : rclcpp_lifecycle::LifecycleNode("decision", options),
    astar_(amr::domain::planning::AStarPlanner::Params{200000, 1.0F, 0.75F}) {
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

  // Fusion-ready gate: dispatch only after fusion reports alive (cold-start
  // A* used to plan straight through the wall while the grid was still empty).
  // Same callback group as perception — low-rate (1 Hz) but semantically tied.
  fusion_hb_sub_ = this->create_subscription<std_msgs::msg::String>(
    "/sensor/fusion/heartbeat", rclcpp::QoS(10).reliable(),
    [this](std_msgs::msg::String::SharedPtr msg) { on_fusion_heartbeat(msg); },
    decision_opts);
  // patrol_3c publish /goal_pose（替代 ros2 param set，lifecycle node param 不暴露）
  goal_sub_ = this->create_subscription<geometry_msgs::msg::PoseStamped>(
    "/goal_pose", rclcpp::QoS(10).reliable(),
    [this](geometry_msgs::msg::PoseStamped::SharedPtr msg) { on_goal_pose(msg); },
    decision_opts);

  // /scan raytrace（NAV2 ObstacleLayer）— scan 驱动 grid，同 perception 组。
  scan_sub_ = this->create_subscription<sensor_msgs::msg::LaserScan>(
    "/scan", rclcpp::QoS(10).best_effort(),
    [this](sensor_msgs::msg::LaserScan::SharedPtr msg) { on_scan(msg); },
    decision_opts);

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
  demo_grid_.origin = {0.0F, -10.0F};  // y∈[-10,10]：覆盖 factory_3c y∈[-6,6]
  demo_grid_.cells.assign(400 * 400, amr::domain::planning::OccupancyGrid::FREE);

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
  fusion_hb_sub_.reset();
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
  fusion_hb_sub_.reset();
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
  // /scan raytrace（NAV2 ObstacleLayer 对标）替代质心 mark_obstacles：
  // scan 射线覆盖障碍表面多点 LETHAL（治撞 box）+ 射线 clearing（替代每帧全清）。
  float rx = 0.0F, ry = 0.0F, rtheta = 0.0F;
  {
    std::lock_guard<std::mutex> lk(pose_mutex_);
    rx = current_pose_.x;
    ry = current_pose_.y;
    rtheta = current_theta_;
  }
  std::vector<float> ranges;
  float amin = 0.0F, ainc = 0.0F;
  bool has_scan = false;
  {
    std::lock_guard<std::mutex> lk(scan_mutex_);
    ranges = latest_ranges_;
    amin = latest_angle_min_;
    ainc = latest_angle_inc_;
    has_scan = has_scan_;
  }
  if (has_scan && !ranges.empty()) {
    // lidar 在 chassis 前 0.25m（chassis→lidar TF）— raytrace 必须从 lidar
    // 发射，否则 hit 坐标整体偏前 0.25m，障碍本体漏标（box 本体在 hit 后）。
    const float lx = rx + 0.25F * std::cos(rtheta);
    const float ly = ry + 0.25F * std::sin(rtheta);
    scan_to_grid_.raytrace(demo_grid_, ranges.data(), ranges.size(),
                           amin, ainc, lx, ly, rtheta);
    // 诊断：grid box 位置 cost。box (8,0) → cell (160,200)：
    // gx=(8-0)/0.05, gy=(0-(-10))/0.05（origin {0,-10}）。原 (160,0) 查错 cell
    // （世界 8,-10 空旷区），恒 0，曾误导以为 box 没进 grid。
    RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 2000,
        "raytrace: ranges=%zu robot=(%.2f,%.2f,%.0fdeg) box_cell(160,200)=%d",
        ranges.size(), rx, ry, rtheta * 180.0F / M_PI,
        demo_grid_.cost_at(160, 200));
  } else {
    RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 2000,
        "raytrace SKIP: has_scan=%d ranges=%zu", has_scan, ranges.size());
  }

  // fusion objects 标 grid（质心 LETHAL + 指数 inflation）— 补 /scan 盲区：
  // depth 检出的低矮障碍 lidar 扫不到，须进 grid 才能 A*/VFH 绕。
  // objects 在 amr/chassis frame（fusion_node:258），用 robot map 位姿旋/平移到 map 再标。
  // fusion object 标 grid。跳过 robot inscribed 内的 object：这些是 lidar 自命中/
  // 噪点（real lidar 扫不到 robot 体内），inflate 会把 robot 自己的 cell 标 INSCRIBED
  // → A* start 不可走 → path_pts=0（曾让 inscribed 0.55 看似"A* 找不到路"）。
  // 贴 robot 的真实障碍由 collision_guard(stop_dist) 兜底，decision 不必再 inflate。
  const float oc = std::cos(rtheta), os = std::sin(rtheta);
  const float inscribed = grid_updater_.params().inscribed_radius;
  for (const auto &obj : objs->objects) {
    const float mx = rx + oc * obj.x - os * obj.y;
    const float my = ry + os * obj.x + oc * obj.y;
    if (std::hypot(mx - rx, my - ry) < inscribed) continue;
    grid_updater_.inflate(demo_grid_, mx, my);
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
  // Dispatch gate: fusion-ready gating + same-goal dedup on the MAP-frame
  // goal identity. Previous dedup compared the map-frame goal param against
  // the odom-frame dispatched value — with a real map→odom offset they never
  // matched and the robot re-dispatched the same goal forever (spin at goal).
  if (!gate_.should_dispatch(gx, gy)) {
    RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 2000,
        "plan: gate blocked fusion_ready=%d gx=%.1f gy=%.1f", gate_.fusion_ready(), gx, gy);
    return;
  }

  auto path = astar_.plan(demo_grid_, start, goal_pose);
  RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 2000,
      "plan: start=(%.1f,%.1f) goal=(%.1f,%.1f) inscribed=%.2f path_pts=%zu",
      start.x, start.y, gx, gy, grid_updater_.params().inscribed_radius, path.size());
  if (path.empty()) {
    // 诊断（20260817 死锁排查）：空路径时 dump 端点 cost + 起点周边窗口，
    // 区分"起点被堵/目标被堵/搜索不可达"三种空因。
    int scx = 0, scy = 0, gcx = 0, gcy = 0;
    amr::domain::planning::world_to_grid(demo_grid_, start.x, start.y, scx, scy);
    amr::domain::planning::world_to_grid(demo_grid_, gx, gy, gcx, gcy);
    std::string win;
    for (int dy = -8; dy <= 8; ++dy) {
      for (int dx = -8; dx <= 8; ++dx)
        win += std::to_string(demo_grid_.cost_at(scx + dx, scy + dy) > 252 ? '#' :
                              demo_grid_.cost_at(scx + dx, scy + dy) > 0 ? '+' : '.');
      win += '\n';
    }
    RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
        "EMPTY path: start_cost=%d(%d,%d) goal_cost=%d(%d,%d) window16x16[#=inscribed/lthal +=decay . =free]:\n%s",
        demo_grid_.cost_at(scx, scy), scx, scy, demo_grid_.cost_at(gcx, gcy), gcx, gcy, win.c_str());
    // Goal blocked: no useless dispatch; re-plan on the next perception
    // cycle once the obstacle clears. An in-flight goal is kept.
    return;
  }

  // 端点吸附可观测性（20260817 评审 §4.2）：持续吸附 = stale 膨胀盘或传感器
  // 劣化的前兆信号，必须显性暴露，不能被吸附静默吞掉。
  const float snap_start = std::hypot(path.front().x - start.x, path.front().y - start.y);
  const float snap_goal = std::hypot(path.back().x - goal_pose.x, path.back().y - goal_pose.y);
  if (snap_start > 0.1F || snap_goal > 0.1F) {
    RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 2000,
        "endpoint snapped: start+%.2fm goal+%.2fm", snap_start, snap_goal);
  }

  // 4. Goal lock: perception noise must not preempt an executing goal.
  // active_goal_ 跨 callback group 共享（on_perception 在 Reentrant 组读，
  // on_goal_response/on_result 在默认组写）—— 锁内快照后立即释放（B4）。
  bool has_active_goal = false;
  {
    std::lock_guard<std::mutex> lk(goal_mutex_);
    has_active_goal = static_cast<bool>(active_goal_);
  }
  if (has_active_goal) return;

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
  gate_.note_dispatched(gx, gy);  // map-frame goal identity, for dedup

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
  // quaternion → yaw（scan raytrace 把 lidar 角向转到 map 角向用）
  const auto &q = msg->pose.pose.orientation;
  current_theta_ = static_cast<float>(std::atan2(
      2.0 * (q.w * q.z + q.x * q.y), 1.0 - 2.0 * (q.y * q.y + q.z * q.z)));
  has_pose_.store(true, std::memory_order_release);
}

void DecisionNode::on_scan(const sensor_msgs::msg::LaserScan::SharedPtr msg) {
  std::lock_guard<std::mutex> lk(scan_mutex_);
  latest_ranges_ = msg->ranges;
  latest_angle_min_ = msg->angle_min;
  latest_angle_inc_ = msg->angle_increment;
  has_scan_ = true;
}

void DecisionNode::on_fusion_heartbeat(const std_msgs::msg::String::SharedPtr msg)
{
  using amr::domain::perception::DegradationLevel;
  using amr::domain::perception::DegradationPolicy;
  DegradationLevel level;
  const bool parsed = DegradationPolicy::from_heartbeat_string(msg->data, level);
  const bool ready = parsed && DegradationPolicy::is_nominal(level);
  gate_.set_fusion_ready(ready);
  RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 3000,
      "fusion_hb: data=%s parsed=%d ready=%d", msg->data.c_str(), parsed, ready);
}

void DecisionNode::on_goal_pose(const geometry_msgs::msg::PoseStamped::SharedPtr msg)
{
  // patrol_3c 发 /goal_pose（替代 ros2 param set /decision，lifecycle 不暴露 param）。
  // 内部 set_parameter → plan loop get_parameter 读新 goal。
  this->set_parameter(rclcpp::Parameter("goal_x", static_cast<double>(msg->pose.position.x)));
  this->set_parameter(rclcpp::Parameter("goal_y", static_cast<double>(msg->pose.position.y)));
  RCLCPP_INFO(this->get_logger(), "goal_pose: (%.1f, %.1f)",
      msg->pose.position.x, msg->pose.position.y);
}

// ── Action client wiring (ROS2-specific, stays in Node) ─────────────────────
// goal_mutex_ 保护 active_goal_/retry_count_/last_target_（B4）：on_perception
// (Reentrant 组) 与 on_goal_response/on_result (默认组) 跨组并发。锁内绝不调
// ROS 异步接口（async_send_goal/async_cancel_goal）——在锁外调用，避免回调
// 重入死锁。

void DecisionNode::send_goal(float target_x, float target_y)
{
  if (!client_->action_server_is_ready()) {
    RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                         "Action server not available");
    return;
  }

  {
    std::lock_guard<std::mutex> lk(goal_mutex_);
    last_target_x_ = target_x;
    last_target_y_ = target_y;
  }

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

  client_->async_send_goal(goal, opts);  // 锁外：回调可能重入
}

void DecisionNode::cancel_active_goal()
{
  ClientGoalHandle::SharedPtr gh;
  {
    std::lock_guard<std::mutex> lk(goal_mutex_);
    gh = active_goal_;
    active_goal_.reset();
    retry_count_ = 0;
  }
  if (!gh) return;
  RCLCPP_INFO(this->get_logger(), "Preempting previous goal");
  client_->async_cancel_goal(gh);  // 锁外：避免回调重入死锁
}

void DecisionNode::on_goal_response(const ClientGoalHandle::SharedPtr& goal_handle)
{
  if (!goal_handle) {
    // 被拒：锁内原子地判定 retry + 累加 + 取 last_target，锁外再 send_goal。
    bool retry = false;
    float tx = 0.0F, ty = 0.0F;
    int attempt = 0;
    {
      std::lock_guard<std::mutex> lk(goal_mutex_);
      retry = selector_.should_retry(retry_count_);
      tx = last_target_x_;
      ty = last_target_y_;
      if (retry) {
        attempt = ++retry_count_;
      } else {
        retry_count_ = 0;
      }
    }
    if (retry) {
      RCLCPP_WARN(this->get_logger(), "Goal rejected, retrying %d/%d (%.2f, %.2f)",
                   attempt, amr::domain::planning::TargetSelector::kMaxRetries, tx, ty);
      send_goal(tx, ty);  // 锁外：send_goal 内部自带短锁，不重入
    } else {
      RCLCPP_ERROR(this->get_logger(), "Goal rejected after retries, giving up");
    }
    return;
  }
  {
    std::lock_guard<std::mutex> lk(goal_mutex_);
    active_goal_ = goal_handle;
    retry_count_ = 0;
  }
  RCLCPP_INFO(this->get_logger(), "Goal accepted by motor_ctrl");
}

void DecisionNode::on_result(const ClientGoalHandle::WrappedResult& result)
{
  {
    std::lock_guard<std::mutex> lk(goal_mutex_);
    active_goal_.reset();
  }
  switch (result.code) {
    case rclcpp_action::ResultCode::SUCCEEDED:
      RCLCPP_INFO(this->get_logger(), "MoveToPose succeeded: reached (%.2f, %.2f)",
                  result.result->final_x, result.result->final_y);
      break;
    case rclcpp_action::ResultCode::CANCELED:
      RCLCPP_INFO(this->get_logger(), "MoveToPose canceled");
      break;
    default:
      // ABORTED (collision-guard anti-deadlock, track-error stop, …):
      // clear the same-goal dedup so the next perception cycle re-dispatches
      // and A* replans around the now-marked obstacle. Without this the gate
      // kept denying the goal forever after a single abort (2026-08-17:
      // abort → "gate blocked" loop right after the rack incident).
      gate_.reset();
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
