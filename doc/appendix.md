# 附录

## 关键代码片段

### ISensor 接口（依赖注入示例）

```cpp
// PerceptionService 构造函数 — 传感器通过接口注入
PerceptionService(ISensor<LidarScan> &lidar, ISensor<ImuData> &imu, ISensor<CameraFrame> &cam)
  : lidar_(lidar), imu_(imu), camera_(cam) {}

// FusionNode 中创建传感器并注入
lidar_ = SensorFactory::create_lidar(lidar_cfg_);
perception_.emplace(*lidar_, *imu_, *camera_);
```

### SensorFactory — YAML 驱动传感器创建

```cpp
static LidarPtr create_lidar(const SensorConfig &cfg) {
    if (cfg.type == "simulated")    return std::make_unique<SimulatedLidar>();
    if (cfg.type == "sick_tim781")  return std::make_unique<SickTiM781Adapter>(cfg.topic);
    return std::make_unique<SimulatedLidar>();  // fallback
}
```

### 覆盖率门禁逻辑

```bash
# quality/quality.sh
if awk "BEGIN { exit (${COV_NUM} >= 80.0) ? 0 : 1 }"; then
  echo "Gate: SKIP (>= 80%)"
else
  if awk "BEGIN { exit (${COV_NUM} >= ${PREV_NUM}) ? 0 : 1 }"; then
    echo "Gate: PASS"
  else
    echo "Gate: FAIL — coverage dropped below previous while < 80%"
    exit 1
  fi
fi
```

## CMakeLists 结构

```
robot_middleware_lib (STATIC)    ← 8 个 Node .cpp, 链接到所有可执行文件和测试
compute_component   (SHARED)     ← fusion + decision + motor_ctrl, 加载到容器
compute_container   (EXE)        ← 组件容器入口
*_node              (EXE × 6)    ← 传感器 + 监控 + Fleet, 独立进程
test_*              (EXE × 10)   ← GoogleTest, 链接 robot_middleware_lib
```

关键 CMake 变量：

| 变量 | 值 |
|------|-----|
| `BUILD_TESTING` | `ON` 时定义 `AMR_TEST_HOOKS` + 编译测试目标 |
| `AMR_TEST_HOOKS` | 展开 `AMR_TEST_ONLY()` 宏，仅测试构建时生效 |
| `--cmake-clean-first` | 覆盖率构建时强制全量重编译 |

## 第三方组件参考

| 组件 | 文档 | 许可证 |
|------|------|:---:|
| ROS2 Jazzy | [docs.ros.org/en/jazzy](https://docs.ros.org/en/jazzy/) | Apache 2.0 |
| Fast-DDS | [fast-dds.docs.eprosima.com](https://fast-dds.docs.eprosima.com/) | Apache 2.0 |
| spdlog | [github.com/gabime/spdlog](https://github.com/gabime/spdlog) | MIT |
| tf2 | [wiki.ros.org/tf2](https://wiki.ros.org/tf2) | BSD-3 |
| Prometheus | [prometheus.io/docs](https://prometheus.io/docs/) | Apache 2.0 |
| GoogleTest | [google.github.io/googletest](https://google.github.io/googletest/) | BSD-3 |
| LTTng | [lttng.org/docs](https://lttng.org/docs/) | LGPL 2.1 |
| Gazebo Harmonic | [gazebosim.org/docs](https://gazebosim.org/docs/) | Apache 2.0 |
| DBSCAN | Ester et al. (1996), "A Density-Based Algorithm for Discovering Clusters" | Academic |

## 缩略词字典

> 完整术语表见 [ARCHITECTURE.md](ARCHITECTURE.md#术语表)。以下为补充。

| 缩写 | 全称 |
|------|------|
| AMR | Autonomous Mobile Robot |
| ASan | Address Sanitizer |
| CI | Continuous Integration |
| CRTP | Curiously Recurring Template Pattern |
| DIP | Dependency Inversion Principle |
| GWT | Given-When-Then (测试模式) |
| LCM | Lifecycle Node (ROS2 托管节点) |
| MMIO | Memory-Mapped I/O |
| NFR | Non-Functional Requirement |
| RTOS | Real-Time Operating System |
| SOLID | Single Responsibility / Open-Closed / Liskov / Interface Segregation / Dependency Inversion |
