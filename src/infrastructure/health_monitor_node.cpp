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
  for (const auto &cfg : probes_) {
    monitor_.register_node(cfg.node, cfg.timeout_s);
  }

  // Create DiagnosticsPublisher (extracted from HealthMonitorNode — SRP)
  diagnostics_ = std::make_unique<DiagnosticsPublisher>(this,
    [this]() -> std::vector<std::pair<std::string, amr::domain::monitoring::NodeStatus>> {
      std::vector<std::pair<std::string, amr::domain::monitoring::NodeStatus>> result;
      for (const auto &cfg : probes_) {
        result.emplace_back(cfg.node, monitor_.escalated_status(cfg.node));
      }
      return result;
    });

  RCLCPP_INFO(this->get_logger(),
              "HealthMonitor configured: %zu nodes, %.1fs interval, restart=%s",
              probes_.size(), check_interval_s_, restart_enabled_ ? "on" : "off");

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
  subs_.clear();
  lifecycle_probe_.reset();
  pub_.reset();
  health_srv_.reset();
  diagnostics_.reset();

  return CallbackReturn::SUCCESS;
}

HealthMonitorNode::CallbackReturn
HealthMonitorNode::on_shutdown(const rclcpp_lifecycle::State &)
{
  timer_.reset();

  prometheus_.reset();

  subs_.clear();
  lifecycle_probe_.reset();
  pub_.reset();
  health_srv_.reset();
  diagnostics_.reset();

  return CallbackReturn::SUCCESS;
}

void HealthMonitorNode::declare_parameters()
{
  this->declare_parameter<double>("check_interval_s", 1.0);
  this->declare_parameter<bool>("health_monitor.restart_enabled", true);

  // 监视清单：不传 = 自研栈 6 节点（既有行为零改动）
  std::vector<std::string> defaults;
  for (const auto &cfg : kDefaultNodes) {
    defaults.emplace_back(cfg.node);
  }
  const auto nodes = this->declare_parameter<std::vector<std::string>>(
    "health_monitor.nodes", defaults);

  for (const auto &n : nodes) {
    const NodeConfig *def = nullptr;
    for (const auto &cfg : kDefaultNodes) {
      if (n == cfg.node) { def = &cfg; break; }
    }
    this->declare_parameter<std::string>("health_monitor." + n + ".probe", "topic");
    this->declare_parameter<std::string>(
      "health_monitor." + n + ".topic", def != nullptr ? def->topic : "");
    this->declare_parameter<double>(
      "health_monitor." + n + ".timeout_s", def != nullptr ? def->default_timeout_s : 2.0);
  }
}

void HealthMonitorNode::load_parameters()
{
  check_interval_s_ = this->get_parameter("check_interval_s").as_double();
  restart_enabled_ = this->get_parameter("health_monitor.restart_enabled").as_bool();

  // 注意：as_string_array() 返回的是临时 Parameter 内部的引用，必须先落局部
  const std::vector<std::string> nodes =
    this->get_parameter("health_monitor.nodes").as_string_array();
  probes_.clear();
  for (const auto &n : nodes) {
    ProbeConfig p;
    p.node      = n;
    p.topic     = this->get_parameter("health_monitor." + n + ".topic").as_string();
    p.timeout_s = this->get_parameter("health_monitor." + n + ".timeout_s").as_double();
    p.lifecycle = this->get_parameter("health_monitor." + n + ".probe").as_string()
                  == "lifecycle";
    probes_.push_back(p);
  }
}

void HealthMonitorNode::create_subscriptions()
{
  std::vector<std::string> lifecycle_targets;
  for (const auto &cfg : probes_) {
    if (cfg.lifecycle) {
      lifecycle_targets.push_back(cfg.node);  // 无自研心跳，走 get_state 探针
      continue;
    }
    subs_.push_back(this->create_subscription<std_msgs::msg::String>(
      cfg.topic, amr::qos::reliable_stream(),
      [this, node = cfg.node](std_msgs::msg::String::SharedPtr /*msg*/) {
        monitor_.heartbeat_received(node);
      }));
  }
  if (!lifecycle_targets.empty()) {
    lifecycle_probe_ = std::make_unique<amr::infrastructure::LifecycleProbe>(
      this->get_node_base_interface(), this->get_node_graph_interface(),
      this->get_node_services_interface(), lifecycle_targets,
      [this](const std::string & n) { monitor_.heartbeat_received(n); });
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

  // lifecycle 探针：1Hz 查询（服务未就绪则跳过——不上报即自然超时）
  if (lifecycle_probe_) {
    lifecycle_probe_->poll();
  }

  auto report = ros2_robot_middleware::msg::HealthReport{};
  report.header.stamp = now;
  report.header.frame_id = "health_monitor";

  for (const auto &cfg : probes_) {
    auto node_status = monitor_.escalated_status(cfg.node);

    auto status = ros2_robot_middleware::msg::HealthStatus{};
    status.node_name = cfg.node;
    status.timeout_s = cfg.timeout_s;
    status.status = amr::domain::monitoring::to_string(node_status);

    for (const auto &[name, hb] : monitor_.heartbeats()) {
      if (name == cfg.node) status.last_seen_s = hb.last_seen_s;
    }

    // Watchdog recovery via ROS2 lifecycle service
    if (node_status == amr::domain::monitoring::NodeStatus::ERROR) {
      if (!restart_enabled_) {
        // NAV2 形态：只报告不处置——处置权归 supervisor 的进程级恢复
        // （lifecycle 四步链会与 lifecycle_manager 的状态跟踪脱节）
        RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                              "[%s] ERROR（restart_enabled=false，仅上报）",
                              cfg.node.c_str());
      } else if (monitor_.should_recover(cfg.node)) {
        RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                              "[%s] ERROR: triggering restart", cfg.node.c_str());
        begin_restart(cfg.node);  // 异步发起，回调内零阻塞（P0-C）
      } else {
        RCLCPP_ERROR(this->get_logger(), "[%s] FATAL: restart limit exceeded",
                     cfg.node.c_str());
        status.status = "FATAL";
      }
    } else if (node_status == amr::domain::monitoring::NodeStatus::OK) {
      monitor_.on_recovered(cfg.node);
    } else if (node_status == amr::domain::monitoring::NodeStatus::STALE) {
      RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                           "[%s] STALE: no data received", cfg.node.c_str());
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
      double timeout = 0.0;
      for (const auto &[name, hb] : monitor_.heartbeats()) {
        if (name == req->param_name) {
          elapsed = hb.last_seen_s;
          timeout = hb.timeout_s;
          break;
        }
      }
      if (elapsed < 0) {
        resp->success = false;
        resp->message = "Unknown node: " + req->param_name;
        return;
      }
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
  for (const auto &cfg : probes_) {
    if (cfg.lifecycle) continue;  // stock NAV2 节点不参与 lifecycle 重启（只报告）
    restart_clients_[cfg.node] =
      this->create_client<lifecycle_msgs::srv::ChangeState>(
        cfg.node + "/change_state",
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
  for (const auto &cfg : probes_) {
    double val = -1.0;
    for (const auto &[name, hb] : monitor_.heartbeats()) {
      if (name == cfg.node) { val = hb.last_seen_s; break; }
    }
    out << "ros2_node_health_seconds{node=\"" << cfg.node << "\"} " << val << "\n";
  }
  out << "# HELP ros2_node_timeout_seconds Configured timeout\n";
  out << "# TYPE ros2_node_timeout_seconds gauge\n";
  for (const auto &cfg : probes_) {
    out << "ros2_node_timeout_seconds{node=\"" << cfg.node << "\"} "
        << cfg.timeout_s << "\n";
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
