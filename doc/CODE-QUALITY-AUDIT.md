# 代码质量审计报告

> 审计标准：CLAUDE.md（类上限 150/.cc 250/函数 40/公有方法 9/成员 5）+ Google C++ Style Guide + ISO 25010 + DDD 分层原则。
> 审计日期：2026-07-19

---

## 一、红线违规（立即修复）

| # | 文件 | 行数 | 违规类型 | 红线 |
|---|------|:---:|------|------|
| 1 | `domain/perception/kalman_filter.hpp` | **334** | 头文件超限 | ≤150 行 |
| 2 | `domain/perception/cluster_detector.hpp` | **167** | 头文件超限 | ≤150 行 |
| 3 | `domain/perception/tracker.hpp` | **153** | 头文件超限 | ≤150 行 |
| 4 | `src/infrastructure/health_monitor_node.cpp` | **493** | 源文件超限（近 2 倍） | ≤250 行 |

**根因分析**：
- `kalman_filter.hpp`：模板类 + 两个测量模型（Linear/RangeBearing）+ Joseph 协方差更新 → 一个头文件包含了多个类的职责
- `health_monitor_node.cpp`：HealthMonitor 的 SRP 严重违反——心跳检测/看门狗重启/Prometheus HTTP Server/diagnostics 发布四种职责混合在一个类中

**修复方向**：
- `kalman_filter.hpp` → 拆分：`linear_measurement.hpp` + `range_bearing_measurement.hpp` + 保留核心 KF 逻辑
- `health_monitor_node.cpp` → 拆分：`prometheus_http_server.hpp/.cpp` + `diagnostics_publisher.hpp/.cpp`，HealthMonitorNode 仅保留心跳/看门狗编排

---

## 二、ISO 25010 可维护性违规

### 2.1 SRP（单一职责）违规

**HealthMonitorNode** 承担 5 种职责：

| 职责 | 行数（估算） | 应属于 |
|------|:---:|------|
| 心跳订阅 + 超时检测 | ~120 | HealthMonitorNode（保留） |
| 看门狗 lifecycle 重启 | ~80 | HealthMonitorNode（待简化，见 ADR-12） |
| Prometheus HTTP Server | ~150 | **独立类** `PrometheusHttpServer` |
| diagnostics 发布 | ~50 | **独立类** `DiagnosticsPublisher` |
| metrics 数据格式化 | ~90 | **独立类** `MetricsFormatter` |

### 2.2 测试代码重复

`test_helpers.hpp` 已创建，但 4 个测试文件（`test_motor_ctrl.cpp`、`test_decision.cpp`、`test_fusion.cpp`）仍含有本地 `spin_until` 副本，未使用共享 helper。

### 2.3 DDD 分层泄漏

`serverinterface.hpp` 中的 `CameraFrame` 包含 `#ifndef NDEBUG` 的 debug generation counter——debug 构建逻辑泄漏到了数据定义中。应该分离为运行时 assertion 宏。

---

## 三、性能与安全

### 3.1 无 UB 违规

审计未发现裸 `new/delete`、未初始化的指针、数组越界模式。智能指针使用规范。

### 3.2 潜在的实时性风险

`health_monitor_node.cpp:293-358` — `setup_prometheus()` 中使用 `accept()` 阻塞调用。虽然运行在独立线程，但 `accept()` 超时由 OS 控制，缺乏应用层超时保护。

### 3.3 编译警告

当前 `colcon build` 零 warning（已验证）。

---

## 四、迭代修复计划

### P0：红线修复（2d）

| 任务 | 预估 | 描述 |
|------|:---:|------|
| kalman_filter.hpp 拆分 | 3h | 测量模型独立为 `linear_measurement.hpp` + `range_bearing_measurement.hpp` |
| cluster_detector.hpp 精简 | 1h | DBSCAN 的 `region_query` + 质心计算移入 `.cpp` |
| tracker.hpp 精简 | 1h | 模板成员移入 `.ipp` 或 `.cpp` |
| health_monitor_node.cpp 拆分 | 4h | 拆分 PrometheusHttpServer + DiagnosticsPublisher 独立类 |
| **合计** | **9h** | |

### P1：SRP 清理 + 测试（2d）

| 任务 | 预估 | 描述 |
|------|:---:|------|
| test_helpers.hpp 推广 | 2h | 4 个测试文件移除本地 `spin_until` 副本 |
| HealthMonitor 职责简化 | 2h | 移除 `try_restart_sequence()` 中的 lifecycle reset（ADR-12 已决策） |
| CameraFrame debug 分离 | 0.5h | generation counter 改为独立 debug assertion 宏 |
| **合计** | **4.5h** | |

### P2：架构强化（3d）

| 任务 | 预估 | 描述 |
|------|:---:|------|
| domain 层零 ROS2 依赖审计 | 2h | `grep -r "rclcpp\|sensor_msgs" domain/` → 全部通过 |
| quality 门禁增强 | 2h | 新增 `clang-tidy` + `iwyu` 到 CI |
| 类上限自动检查脚本 | 1h | `quality/scripts/check_limits.sh` — CI 阻塞 |
| **合计** | **5h** | |

---

## 五、检查脚本

```bash
# quality/scripts/check_limits.sh
# CI 中阻塞：头文件 >150 行或 .cc >250 行 → exit 1

MAX_H=150; MAX_CC=250; FAIL=0
for f in include/ros2_robot_middleware/**/*.hpp; do
  lines=$(wc -l < "$f")
  [ "$lines" -gt "$MAX_H" ] && echo "HDR $f ($lines > $MAX_H)" && FAIL=1
done
for f in src/infrastructure/*.cpp; do
  lines=$(wc -l < "$f")
  [ "$lines" -gt "$MAX_CC" ] && echo "SRC $f ($lines > $MAX_CC)" && FAIL=1
done
exit $FAIL
```
