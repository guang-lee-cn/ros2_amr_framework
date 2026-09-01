#include "ros2_robot_middleware/infrastructure/fusion_node.hpp"
#include "ros2_robot_middleware/infrastructure/qos_profiles.hpp"
#include "generated/perf_instrumentation.hpp"
#include "ros2_robot_middleware/observability/logging.hpp"
#include "ros2_robot_middleware/observability/metrics_registry.hpp"
#include "ros2_robot_middleware/observability/trace_points.hpp"
#include "ros2_robot_middleware/observability/tracer.hpp"

#include <rclcpp_components/register_node_macro.hpp>

#include <cmath>
#include <cstring>
#include <memory>

// ── Constructors ─────────────────────────────────────────────────────

FusionNode::FusionNode() : amr::infrastructure::AmrNode("fusion") {
  declare_sensor_parameters();
}

FusionNode::FusionNode(const rclcpp::NodeOptions &options)
  : amr::infrastructure::AmrNode("fusion", options) {
  declare_sensor_parameters();
}

FusionNode::FusionNode(const rclcpp::NodeOptions &options,
                       const amr::domain::perception::DegradationPolicy::Config &deg_config)
  : amr::infrastructure::AmrNode("fusion", options) {
  declare_sensor_parameters();
  // Create sensors immediately for test hook (skips on_configure lifecycle)
  create_sensors();
  perception_.emplace(*lidar_, *imu_, *camera_,
                      std::make_unique<amr::domain::perception::ClusterDetector>(),
                      deg_config);
}

// ── Sensor parameter declaration ─────────────────────────────────────

void FusionNode::declare_sensor_parameters() {
  this->declare_parameter("sensors.lidar.type", "simulated");
  this->declare_parameter("sensors.lidar.topic", "/scan");
  this->declare_parameter("sensors.imu.type", "simulated");
  this->declare_parameter("sensors.imu.topic", "/imu/data");
  this->declare_parameter("sensors.camera.type", "simulated");
  this->declare_parameter("sensors.camera.topic", "/camera/color/image_raw");
  // 演示场景：obstacle / slalom / corridor / empty（默认）
  this->declare_parameter("scenario", "empty");
  // 打戳规范 v1：新鲜度容差（ms→ns）+ 故障注入钩子（测试用，默认关）
  this->declare_parameter("stale.lidar_ms", 200);
  this->declare_parameter("stale.imu_ms", 100);
  this->declare_parameter("inject.lidar_stamp_age_ms", 0);

  // 读入并装配门控（声明即装配：测试钩子构造函数也走这里）
  stale_lidar_tol_ns_ =
      static_cast<int64_t>(this->get_parameter("stale.lidar_ms").as_int()) * 1000000LL;
  inject_stamp_age_ns_ =
      static_cast<int64_t>(this->get_parameter("inject.lidar_stamp_age_ms").as_int()) * 1000000LL;
  stamp_gate_.set_tolerance("lidar", stale_lidar_tol_ns_);
  stamp_gate_.set_tolerance(
      "imu", static_cast<int64_t>(this->get_parameter("stale.imu_ms").as_int()) * 1000000LL);
}

/// 按场景名返回障碍物布局（演示用）。
amr::hal::sensor::Scenario FusionNode::load_scenario(const std::string &name) {
  amr::hal::sensor::Scenario s;
  if (name == "obstacle") {
    s.obstacles = {{2.0F, 0.0F, 0.4F}};
  } else if (name == "slalom") {
    s.obstacles = {{1.5F, 0.0F, 0.3F}, {2.5F, 0.8F, 0.3F}, {3.5F, 0.0F, 0.3F}};
  } else if (name == "corridor") {
    s.obstacles = {{2.0F, -0.6F, 0.5F}, {2.0F, 0.6F, 0.5F},
                   {4.0F, -0.6F, 0.5F}, {4.0F, 0.6F, 0.5F}};
  } else if (name == "lowstep") {
    // 低矮障碍（top 0.15 < lidar mount 0.3 → 相机深度补盲）
    // + 常规障碍（lidar/相机都见 → merge 去重）。
    amr::hal::sensor::Obstacle low{1.5F, 0.0F, 0.3F, 0.0F, 0.15F};
    amr::hal::sensor::Obstacle normal{2.5F, 0.5F, 0.4F};
    s.obstacles = {low, normal};
  }
  return s;  // empty 返回空场景
}

void FusionNode::create_sensors() {
  using amr::hal::sensor::SensorFactory;

  lidar_cfg_.type  = this->get_parameter("sensors.lidar.type").as_string();
  lidar_cfg_.topic = this->get_parameter("sensors.lidar.topic").as_string();
  // 演示场景：Simulated LiDAR 按场景生成障碍物点云
  auto scenario = load_scenario(this->get_parameter("scenario").as_string());
  lidar_  = SensorFactory::create_lidar(lidar_cfg_, scenario);

  imu_cfg_.type  = this->get_parameter("sensors.imu.type").as_string();
  imu_cfg_.topic = this->get_parameter("sensors.imu.topic").as_string();
  imu_    = SensorFactory::create_imu(imu_cfg_);

  camera_cfg_.type  = this->get_parameter("sensors.camera.type").as_string();
  camera_cfg_.topic = this->get_parameter("sensors.camera.topic").as_string();
  // 场景同样驱动相机深度：低矮障碍补盲（lidar 看不到的由深度检测到）。
  camera_ = SensorFactory::create_camera(camera_cfg_, scenario);
}

// ── Lifecycle callbacks ──────────────────────────────────────────────

FusionNode::CallbackReturn FusionNode::on_configure(const rclcpp_lifecycle::State &) {
  // Create sensors from YAML-driven params
  create_sensors();
  // fail-fast（2026-08-25 审计 P1-c）：类型未注册（拼写错/缺驱动）→ 配置失败
  // 拒绝启动。旧版静默 fallback 仿真传感器 = 机器人带着假感知上线。
  if (!lidar_ || !imu_ || !camera_) {
    RCLCPP_ERROR(this->get_logger(),
      "传感器创建失败: lidar='%s' imu='%s' camera='%s' —— 类型未注册（fail-fast，"
      "不降级为仿真传感器）",
      lidar_cfg_.type.c_str(), imu_cfg_.type.c_str(), camera_cfg_.type.c_str());
    return CallbackReturn::FAILURE;
  }
  // 若 lidar 是可连接适配器（sick_tim781）：宿主直接订阅并喂入扫描数据。
  // LifecycleNode 组合 rclcpp::Node（非继承），不能直接传 rclcpp::Node&，故用 feed_scan。
  if (auto *adapter = dynamic_cast<amr::hal::sensor::SickTiM781Adapter *>(lidar_.get())) {
    scan_sub_ = this->create_subscription<sensor_msgs::msg::LaserScan>(
        lidar_cfg_.topic, amr::qos::sensor_stream(),
        [adapter](sensor_msgs::msg::LaserScan::SharedPtr msg) {
          adapter->feed_scan(std::move(msg));
        });
  }
  lidar_->init();
  imu_->init();
  camera_->init();

  // Coordinate transform provider (tf2_ros::Buffer + TransformListener)
  tf2_ = std::make_unique<amr::infrastructure::Tf2TransformProvider>(
      this->get_clock());

  // Wire domain layer after sensors + tf2 are ready
  perception_.emplace(*lidar_, *imu_, *camera_);
  perception_->set_transform(tf2_.get());

  fusion_pub_ = this->create_publisher<ros2_robot_middleware::msg::PerceptionObjects>(
      "/perception/objects", amr::qos::reliable_stream());

  heartbeat_pub_ = this->create_publisher<std_msgs::msg::String>(
      "/sensor/fusion/heartbeat", amr::qos::reliable_stream());

  lidar_pub_ = this->create_publisher<sensor_msgs::msg::LaserScan>(
      "/sensor/lidar", amr::qos::reliable_stream());

  pointcloud_pub_ = this->create_publisher<sensor_msgs::msg::PointCloud2>(
      "/sensor/pointcloud", amr::qos::reliable_stream());

  return CallbackReturn::SUCCESS;
}

FusionNode::CallbackReturn FusionNode::on_activate(const rclcpp_lifecycle::State &) {
  using namespace std::chrono_literals;
  timer_ = this->create_wall_timer(200ms, [this]() { timer_callback(); });
  heartbeat_timer_ = this->create_wall_timer(1s, [this]() { update_heartbeat_status(); });

  fusion_pub_->on_activate();
  heartbeat_pub_->on_activate();
  lidar_pub_->on_activate();
  pointcloud_pub_->on_activate();
  return CallbackReturn::SUCCESS;
}

FusionNode::CallbackReturn FusionNode::on_deactivate(const rclcpp_lifecycle::State &) {
  timer_.reset();
  heartbeat_timer_.reset();
  fusion_pub_->on_deactivate();
  heartbeat_pub_->on_deactivate();
  lidar_pub_->on_deactivate();
  pointcloud_pub_->on_deactivate();
  return CallbackReturn::SUCCESS;
}

FusionNode::CallbackReturn FusionNode::on_cleanup(const rclcpp_lifecycle::State &) {
  fusion_pub_.reset();
  heartbeat_pub_.reset();
  lidar_pub_.reset();
  pointcloud_pub_.reset();
  perception_.reset();
  if (lidar_)  lidar_->shutdown();
  if (imu_)    imu_->shutdown();
  if (camera_) camera_->shutdown();
  lidar_.reset();
  imu_.reset();
  camera_.reset();
  return CallbackReturn::SUCCESS;
}

FusionNode::CallbackReturn FusionNode::on_shutdown(const rclcpp_lifecycle::State &) {
  timer_.reset();
  heartbeat_timer_.reset();
  fusion_pub_.reset();
  heartbeat_pub_.reset();
  lidar_pub_.reset();
  pointcloud_pub_.reset();
  return CallbackReturn::SUCCESS;
}

// ── Timer callback — delegates to domain layer ───────────────────────

void FusionNode::timer_callback() {
  AMR_PERF_PHASE("fusion:tick");
  TRACE_SCOPE(amr::trace::FUSION_TIMER);

  auto t_start = std::chrono::steady_clock::now();

  auto now = this->now();
  double dt = 0.2;  // tracker 预测步长：默认 200ms，首个 tick 前无 dt
  if (last_tick_.nanoseconds() > 0) {
    dt = (now - last_tick_).seconds();
    if (dt > 0.001 && dt < 1.0 && perception_) {
      perception_->tick(dt);
    }
  }
  last_tick_ = now;

  if (!perception_) return;

  // 发布模拟 LiDAR 点云（供 Foxglove/RViz 可视化）
  int64_t lidar_stamp_ns = 0;
  amr::hal::sensor::LidarScan scan;
  if (perception_->lidar_snapshot(scan)) {
    lidar_stamp_ns = scan.stamp_ns;
    // LaserScan（RViz 兼容）
    auto scan_msg = sensor_msgs::msg::LaserScan{};
    scan_msg.header.stamp = now;
    scan_msg.header.frame_id = "amr/chassis/lidar";
    scan_msg.angle_min = scan.angle_min;
    scan_msg.angle_increment = scan.angle_increment;
    scan_msg.angle_max = scan.angle_min + scan.angle_increment * static_cast<float>(scan.range_count - 1);
    scan_msg.range_min = 0.1F;
    scan_msg.range_max = 6.5F;
    scan_msg.ranges.assign(scan.ranges, scan.ranges + scan.range_count);
    lidar_pub_->publish(scan_msg);

    // PointCloud2（Foxglove 3D 需要，LaserScan 无法直接渲染）
    auto pc_msg = sensor_msgs::msg::PointCloud2{};
    pc_msg.header.stamp = now;
    pc_msg.header.frame_id = "amr/chassis/lidar";
    pc_msg.height = 1;
    pc_msg.width = static_cast<uint32_t>(scan.range_count);
    pc_msg.is_dense = true;
    pc_msg.is_bigendian = false;
    // 字段: x,y,z (float32 各 4 字节)
    pc_msg.fields.resize(3);
    pc_msg.fields[0].name = "x"; pc_msg.fields[0].offset = 0; pc_msg.fields[0].datatype = 7; pc_msg.fields[0].count = 1;
    pc_msg.fields[1].name = "y"; pc_msg.fields[1].offset = 4; pc_msg.fields[1].datatype = 7; pc_msg.fields[1].count = 1;
    pc_msg.fields[2].name = "z"; pc_msg.fields[2].offset = 8; pc_msg.fields[2].datatype = 7; pc_msg.fields[2].count = 1;
    pc_msg.point_step = 12;
    pc_msg.row_step = pc_msg.point_step * pc_msg.width;
    pc_msg.data.resize(pc_msg.row_step);
    for (uint32_t i = 0; i < pc_msg.width; ++i) {
      float angle = scan.angle_min + static_cast<float>(i) * scan.angle_increment;
      float r = scan.ranges[i];
      float x = r * std::cos(angle);
      float y = r * std::sin(angle);
      float z = 0.0F;
      std::memcpy(&pc_msg.data[i * 12 + 0], &x, 4);
      std::memcpy(&pc_msg.data[i * 12 + 4], &y, 4);
      std::memcpy(&pc_msg.data[i * 12 + 8], &z, 4);
    }
    pointcloud_pub_->publish(pc_msg);
  }

  // ── Stamp gate：事件时刻过期 → 拒绝发布本拍（旧数据不得冒充"现在"） ──
  // lidar_stamp_ns 来源：适配器路径透传 header.stamp；内部合成路径在快照处补 now。
  if (lidar_stamp_ns == 0) lidar_stamp_ns = now.nanoseconds();  // 内部合成：读取即最早点
  lidar_stamp_ns -= inject_stamp_age_ns_;                        // 故障注入（默认 0 不生效）

  // 降级评估先于 gate 拒绝：断源期间数据被抑制时，降级状态也必须演进
  // （否则 current_level_ 冻结在 FULL，心跳继续报健康——e2e 断源场景实证）
  auto old_level = current_level_;
  current_level_ = perception_->evaluate_degradation();

  if (stamp_gate_.check("lidar", lidar_stamp_ns, now.nanoseconds()) ==
      amr::domain::perception::StampGate::Verdict::STALE) {
    ++lidar_stale_rejects_;
    RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000,
                         "lidar snapshot stale (age %ldms, tol %ldms) - suppress fusion output tick",
                         (now.nanoseconds() - lidar_stamp_ns) / 1000000L, stale_lidar_tol_ns_ / 1000000L);
    return;  // 本拍不发布：宁可空一拍，不用旧世界描述现在
  }

  // 零拷贝：unique_ptr 发布（1:1 订阅者=decision，所有权移交无拷贝）
  auto msg            = std::make_unique<ros2_robot_middleware::msg::PerceptionObjects>();
  msg->header.stamp    = this->now();
  // 障碍在车体帧发布；decision 用 TF 变换到 map 帧标记 A* 网格。
  // 用 "amr/chassis"（TF 树帧）而非 "base_link"（TF 树无此帧）。
  msg->header.frame_id = "amr/chassis";

  // 用 tracker 输出（带持久 track_id + KF 速度估计 + IMU 运动补偿），
  // 而非原始 DBSCAN 簇。id 用 track_id（跨帧稳定），供 decision 关联。
  auto tracked = perception_->fuse_tracked(current_level_, dt);
  for (const auto &c : tracked) {
    auto obj = ros2_robot_middleware::msg::Object{};
    obj.id = "trk_" + std::to_string(c.track_id);
    obj.x = c.x; obj.y = c.y; obj.z = 0.0F;
    obj.category = c.category;  // 深度低矮障碍="low"；识别接入后为语义类别
    msg->objects.push_back(obj);
  }

  // 先取计数再 move——publish 后 unique_ptr 已置空（use-after-move 教训，
  // Debug 构建下 segfault 捕获：UB 在 Release 下不显形≠不存在）
  const auto n_objects = static_cast<int32_t>(msg->objects.size());
  fusion_pub_->publish(std::move(msg));

  // ── Observability ────────────────────────────────────────────────
  auto &m = amr::observability::shared_metrics();
  m.fusion_cycle_count.fetch_add(1, std::memory_order_relaxed);
  m.object_count.store(n_objects, std::memory_order_relaxed);
  m.degradation_level.store(static_cast<int32_t>(current_level_), std::memory_order_relaxed);

  if (current_level_ != old_level) {
    m.degradation_events.fetch_add(1, std::memory_order_relaxed);
    TRACE_EVENT(amr::trace::FUSION_DEGRADATION);
  }

  auto t_end = std::chrono::steady_clock::now();
  m.fusion_latency.record(
      std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start).count());

  if (current_level_ != old_level) {
    RCLCPP_WARN(this->get_logger(), "Degradation: %d -> %d",
                 static_cast<int>(old_level), static_cast<int>(current_level_));
  }
  RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                       "PerceptionObjects published: %d object(s) [level=%d]",
                       n_objects, static_cast<int>(current_level_));
}

void FusionNode::update_heartbeat_status() {
  auto msg = std_msgs::msg::String{};
  if (perception_) {
    msg.data = amr::domain::perception::DegradationPolicy::to_heartbeat_string(current_level_);
  } else {
    msg.data = "inactive";
  }
  heartbeat_pub_->publish(msg);
}

FusionNode::DegradationLevel FusionNode::degradation_level() const { return current_level_; }

RCLCPP_COMPONENTS_REGISTER_NODE(FusionNode)
