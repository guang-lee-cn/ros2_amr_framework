#include "ros2_robot_middleware/infrastructure/health_monitor_node.hpp"
#include "ros2_robot_middleware/infrastructure/qos_profiles.hpp"
#include "ros2_robot_middleware/observability/metrics_registry.hpp"

#include <chrono>
#include <sstream>

static constexpr double kWarnRatio = 0.8;

HealthMonitorNode::HealthMonitorNode()
  : rclcpp_lifecycle::LifecycleNode("health_monitor")
{
}

HealthMonitorNode::CallbackReturn
HealthMonitorNode::on_configure(const rclcpp_lifecycle::State &)
{
  declare_parameters();
  load_parameters();
  create_subscriptions();
  create_report_publisher();
  create_service_server();
  create_restart_clients();

  // Register monitored nodes with domain service
  for (const auto &cfg : kNdes) {
    monitor_.register_node(cfg.node, timeouts_[cfg.node]);
  }

  // Create DiagnosticsPublisher (extracted from HealthMonitorNode — SRP)
  diagnostics_ = std::make_unique<DiagnosticsPublisher>(this,
    [this]() -> std::vector<std::pair<std::string, amr::domain::monitoring::NodeStatus>> {
      std::vector<std::pair<std::string, amr::domain::monitoring::NodeStatus>> result;
      for (const auto &cfg : kNdes) {
        result.emplace_back(std::string(cfg.node), monitor_.escalated_status(cfg.node));
      }
      return result;
    });

  RCLCPP_INFO(this->get_logger(),
              "HealthMonitor configured: %d nodes, %.1fs interval",
              kNumNodes, check_interval_s_);

  return CallbackReturn::SUCCESS;
}

HealthMonitorNode::CallbackReturn
HealthMonitorNode::on_activate(const rclcpp_lifecycle::State &)
{
  create_health_timer();

  // Start Prometheus HTTP server (extracted — standalone POSIX socket server)
  prometheus_ = std::make_unique<PrometheusHttpServer>(kPrometheusPort,
    [this]() { return prometheus_metrics(); });
  prometheus_->start();

  pub_->on_activate();
  diagnostics_->on_activate();

  RCLCPP_INFO(this->get_logger(),
              "HealthMonitor activated: Prometheus on :%d/metrics",
              kPrometheusPort);

  return CallbackReturn::SUCCESS;
}

HealthMonitorNode::CallbackReturn
HealthMonitorNode::on_deactivate(const rclcpp_lifecycle::State &)
{
  timer_.reset();

  pub_->on_deactivate();
  diagnostics_->on_deactivate();

  prometheus_.reset();

  return CallbackReturn::SUCCESS;
}

HealthMonitorNode::CallbackReturn
HealthMonitorNode::on_cleanup(const rclcpp_lifecycle::State &)
{
  for (int i = 0; i < kNumNodes; ++i) {
    subs_[i].reset();
  }
  pub_.reset();
  health_srv_.reset();
  diagnostics_.reset();

  timeouts_.clear();

  return CallbackReturn::SUCCESS;
}

HealthMonitorNode::CallbackReturn
HealthMonitorNode::on_shutdown(const rclcpp_lifecycle::State &)
{
  timer_.reset();

  prometheus_.reset();

  for (int i = 0; i < kNumNodes; ++i) {
    subs_[i].reset();
  }
  pub_.reset();
  health_srv_.reset();
  diagnostics_.reset();

  return CallbackReturn::SUCCESS;
}

void HealthMonitorNode::declare_parameters()
{
  this->declare_parameter<double>("check_interval_s", 1.0);
  for (const auto &cfg : kNdes) {
    std::string key = std::string(cfg.node) + "_timeout_s";
    this->declare_parameter<double>(key, cfg.default_timeout_s);
  }
}

void HealthMonitorNode::load_parameters()
{
  check_interval_s_ = this->get_parameter("check_interval_s").as_double();
  for (const auto &cfg : kNdes) {
    std::string key = std::string(cfg.node) + "_timeout_s";
    timeouts_[cfg.node] = this->get_parameter(key).as_double();
  }
}

void HealthMonitorNode::create_subscriptions()
{
  for (int i = 0; i < kNumNodes; ++i) {
    subs_[i] = this->create_subscription<std_msgs::msg::String>(
      kNdes[i].topic, amr::qos::reliable_stream(),
      [this, node = std::string(kNdes[i].node)](std_msgs::msg::String::SharedPtr /*msg*/) {
        monitor_.heartbeat_received(node);
      });
  }
}

void HealthMonitorNode::create_health_timer()
{
  using namespace std::chrono_literals;
  auto period = std::chrono::milliseconds(
    static_cast<int>(check_interval_s_ * 1000));
  timer_ = this->create_wall_timer(period, [this]() { check_health(); });
}

void HealthMonitorNode::check_health()
{
  // Tick domain service — age all heartbeats
  auto now = this->now();
  if (last_tick_.nanoseconds() > 0) {
    monitor_.tick((now - last_tick_).seconds());
  }
  last_tick_ = now;

  auto report = ros2_robot_middleware::msg::HealthReport{};
  report.header.stamp = now;
  report.header.frame_id = "health_monitor";

  for (const auto &cfg : kNdes) {
    auto node_status = monitor_.escalated_status(cfg.node);

    auto status = ros2_robot_middleware::msg::HealthStatus{};
    status.node_name = cfg.node;
    status.timeout_s = timeouts_[cfg.node];
    status.status = amr::domain::monitoring::to_string(node_status);

    for (const auto &[name, hb] : monitor_.heartbeats()) {
      if (name == cfg.node) status.last_seen_s = hb.last_seen_s;
    }

    // Watchdog recovery via ROS2 lifecycle service
    if (node_status == amr::domain::monitoring::NodeStatus::ERROR) {
      if (monitor_.should_recover(cfg.node)) {
        RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                              "[%s] ERROR: triggering restart", cfg.node);
        begin_restart(cfg.node);  // 异步发起，回调内零阻塞（P0-C）
      } else {
        RCLCPP_ERROR(this->get_logger(), "[%s] FATAL: restart limit exceeded",
                     cfg.node);
        status.status = "FATAL";
      }
    } else if (node_status == amr::domain::monitoring::NodeStatus::OK) {
      monitor_.on_recovered(cfg.node);
    } else if (node_status == amr::domain::monitoring::NodeStatus::STALE) {
      RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                           "[%s] STALE: no data received", cfg.node);
    }

    report.nodes.push_back(status);
  }

  pub_->publish(report);

  // Delegated to DiagnosticsPublisher (extracted class)
  diagnostics_->publish(now);
}

void HealthMonitorNode::create_service_server()
{
  health_srv_ = this->create_service<ros2_robot_middleware::srv::SetParam>(
    "/health/check",
    [this](const std::shared_ptr<ros2_robot_middleware::srv::SetParam::Request> req,
           std::shared_ptr<ros2_robot_middleware::srv::SetParam::Response> resp) {
      double elapsed = -1.0;
      for (const auto &[name, hb] : monitor_.heartbeats()) {
        if (name == req->param_name) { elapsed = hb.last_seen_s; break; }
      }
      if (elapsed < 0) {
        resp->success = false;
        resp->message = "Unknown node: " + req->param_name;
        return;
      }
      double timeout = timeouts_[req->param_name];
      if (elapsed > timeout) {
        resp->success = false;
        resp->message = "ERROR: " + std::to_string(elapsed) + "s";
      } else if (elapsed > timeout * kWarnRatio) {
        resp->success = true;
        resp->message = "WARN: " + std::to_string(elapsed) + "s";
      } else {
        resp->success = true;
        resp->message = "OK: " + std::to_string(elapsed) + "s";
      }
    });
}

void HealthMonitorNode::create_report_publisher()
{
  pub_ = this->create_publisher<ros2_robot_middleware::msg::HealthReport>(
    "/health/report", amr::qos::reliable_stream());
}

void HealthMonitorNode::create_restart_clients()
{
  restart_group_ = this->create_callback_group(
      rclcpp::CallbackGroupType::MutuallyExclusive);
  for (const auto &cfg : kNdes) {
    restart_clients_[cfg.node] =
      this->create_client<lifecycle_msgs::srv::ChangeState>(
        std::string(cfg.node) + "/change_state",
        rclcpp::ServicesQoS(), restart_group_);
  }
}

void HealthMonitorNode::begin_restart(const std::string &node_name)
{
  if (restart_.in_progress) {
    RCLCPP_DEBUG(this->get_logger(),
                 "[%s] 重启已在进行（%s step %zu），跳过", node_name.c_str(),
                 restart_.node.c_str(), restart_.step);
    return;
  }
  auto it = restart_clients_.find(node_name);
  if (it == restart_clients_.end()) return;
  restart_ = {node_name, 0, true};
  restart_.deadline = this->now().seconds() + kRestartTimeoutS;
  RCLCPP_WARN(this->get_logger(), "[%s] 启动异步重启序列（4 步 transition，超时 %.0fs）",
              node_name.c_str(), kRestartTimeoutS);
  send_next_transition();
}

void HealthMonitorNode::send_next_transition()
{
  using Transition = lifecycle_msgs::msg::Transition;
  static constexpr std::array<std::pair<uint8_t, const char *>, 4> kSequence = {{
    {Transition::TRANSITION_DEACTIVATE, "deactivate"},
    {Transition::TRANSITION_CLEANUP,    "cleanup"},
    {Transition::TRANSITION_CONFIGURE,  "configure"},
    {Transition::TRANSITION_ACTIVATE,   "activate"},
  }};

  // 超时检查（N-3 修复）：transition 响应永不到来则放弃，不挂死
  if (this->now().seconds() > restart_.deadline) {
    RCLCPP_ERROR(this->get_logger(),
        "[%s] 重启序列超时（step %zu/%zu）— 放弃本轮，节点保持当前状态",
        restart_.node.c_str(), restart_.step, kSequence.size());
    // 置 ERROR 上 /health/report（R4.2 诚实降级：可观测，不假装接管）
    restart_ = {};
    return;
  }

  if (restart_.step >= kSequence.size()) {
    RCLCPP_INFO(this->get_logger(), "[%s] restart sequence completed successfully",
                restart_.node.c_str());
    restart_ = {};
    return;
  }

  auto it = restart_clients_.find(restart_.node);
  if (it == restart_clients_.end()) { restart_ = {}; return; }
  auto &client = it->second;

  // 非阻塞就绪检查（旧 wait_for_service(1s) 是死锁要素之一）
  if (!client->service_is_ready()) {
    RCLCPP_WARN(this->get_logger(), "[%s] lifecycle service unreachable — 放弃本轮重启",
                restart_.node.c_str());
    restart_ = {};
    return;
  }

  auto request = std::make_shared<lifecycle_msgs::srv::ChangeState::Request>();
  request->transition.id    = kSequence[restart_.step].first;
  request->transition.label = kSequence[restart_.step].second;

  // 响应回调推进状态机——不等待（旧 future.wait_for(2s) 是死锁主因：
  // 单线程 spin 里响应永远轮不到被处理）
  client->async_send_request(
      request,
      [this](rclcpp::Client<lifecycle_msgs::srv::ChangeState>::SharedFuture f) {
        handle_transition_response(f);
      });
}

void HealthMonitorNode::handle_transition_response(
    rclcpp::Client<lifecycle_msgs::srv::ChangeState>::SharedFuture future)
{
  const auto response = future.get();
  const auto step_labels = std::array<const char *, 4>{
      "deactivate", "cleanup", "configure", "activate"};
  if (!response->success) {
    RCLCPP_WARN(this->get_logger(), "[%s] %s rejected（状态不对/已死）— 中止重启",
                restart_.node.c_str(), step_labels[restart_.step < 4 ? restart_.step : 0]);
    restart_ = {};
    return;
  }
  ++restart_.step;
  send_next_transition();  // 链式推进，零阻塞
}

std::string HealthMonitorNode::prometheus_metrics() const
{
  auto &m = amr::observability::shared_metrics();
  std::ostringstream out;

  // Node health gauges
  out << "# HELP ros2_node_health_seconds Seconds since last data from node\n";
  out << "# TYPE ros2_node_health_seconds gauge\n";
  for (const auto &cfg : kNdes) {
    double val = -1.0;
    for (const auto &[name, hb] : monitor_.heartbeats()) {
      if (name == cfg.node) { val = hb.last_seen_s; break; }
    }
    out << "ros2_node_health_seconds{node=\"" << cfg.node << "\"} " << val << "\n";
  }
  out << "# HELP ros2_node_timeout_seconds Configured timeout\n";
  out << "# TYPE ros2_node_timeout_seconds gauge\n";
  for (const auto &cfg : kNdes) {
    out << "ros2_node_timeout_seconds{node=\"" << cfg.node << "\"} "
        << timeouts_.at(cfg.node) << "\n";
  }

  // Sensor rates
  out << "# HELP amr_sensor_rate_hz Sensor publication rate (Hz)\n";
  out << "# TYPE amr_sensor_rate_hz gauge\n";
  out << "amr_sensor_rate_hz{sensor=\"lidar\"} "
      << (m.lidar_rate_ds.load(std::memory_order_relaxed) / 10.0) << "\n";
  out << "amr_sensor_rate_hz{sensor=\"imu\"} "
      << (m.imu_rate_ds.load(std::memory_order_relaxed) / 10.0) << "\n";
  out << "amr_sensor_rate_hz{sensor=\"camera\"} "
      << (m.camera_rate_ds.load(std::memory_order_relaxed) / 10.0) << "\n";

  // Latency histograms
  auto write_histogram = [&](const char *name, const char *help,
                              const amr::observability::Histogram &h) {
    out << "# HELP " << name << " " << help << "\n";
    out << "# TYPE " << name << " histogram\n";
    auto total = h.total_count.load(std::memory_order_relaxed);
    auto sum   = h.total_sum_us.load(std::memory_order_relaxed);
    out << name << "_count " << total << "\n";
    out << name << "_sum " << (sum / 1'000'000.0) << "\n";
    int64_t cumulative = 0;
    int64_t bound_us = amr::observability::Histogram::kBaseUs;
    for (int i = 0; i < amr::observability::Histogram::kBucketCount; ++i) {
      cumulative += h.buckets[i].load(std::memory_order_relaxed);
      out << name << "_bucket{le=\"" << (bound_us / 1'000'000.0) << "\"} "
          << cumulative << "\n";
      bound_us *= amr::observability::Histogram::kBaseUs;
    }
    out << name << "_bucket{le=\"+Inf\"} " << total << "\n";
  };

  write_histogram("amr_fusion_latency_seconds",
                  "Fusion compute latency", m.fusion_latency);
  write_histogram("amr_decision_latency_seconds",
                  "Decision compute latency", m.decision_latency);
  write_histogram("amr_motor_latency_seconds",
                  "Motor control per-step latency", m.motor_latency);
  write_histogram("amr_e2e_latency_seconds",
                  "End-to-end latency sensor→cmd", m.e2e_latency);

  // State gauges
  out << "# HELP amr_degradation_level Current degradation level (0-4)\n";
  out << "# TYPE amr_degradation_level gauge\n";
  out << "amr_degradation_level "
      << m.degradation_level.load(std::memory_order_relaxed) << "\n";

  out << "# HELP amr_object_count Current tracked object count\n";
  out << "# TYPE amr_object_count gauge\n";
  out << "amr_object_count "
      << m.object_count.load(std::memory_order_relaxed) << "\n";

  // Event counters
  out << "# HELP amr_degradation_events_total Degradation events (monotonic)\n";
  out << "# TYPE amr_degradation_events_total counter\n";
  out << "amr_degradation_events_total "
      << m.degradation_events.load(std::memory_order_relaxed) << "\n";

  out << "# HELP amr_recovery_events_total Recovery events (monotonic)\n";
  out << "# TYPE amr_recovery_events_total counter\n";
  out << "amr_recovery_events_total "
      << m.recovery_events.load(std::memory_order_relaxed) << "\n";

  out << "# HELP amr_fusion_cycles_total Fusion cycles (monotonic)\n";
  out << "# TYPE amr_fusion_cycles_total counter\n";
  out << "amr_fusion_cycles_total "
      << m.fusion_cycle_count.load(std::memory_order_relaxed) << "\n";

  return out.str();
}
