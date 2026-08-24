# ADR：时间戳规范与融合新鲜度门控（2026-08-24）

> 状态：已接受（v1）｜ 对标：强脑中间件岗 JD 职责5「多传感器时间戳同步、数据融合」｜ 关联单测 5 例 + 故障注入实证

## 背景与问题

时间戳维度在本仓库原本**完全缺失**：HAL 数据结构（LidarScan/ImuData/CameraFrame）无时间字段；适配器 `read_impl` 丢弃上游 `header.stamp` 真值；融合按「手里最新值」盲用，**没有任何机制发现数据过期**。风险形态：传感器卡顿 500ms 后，一帧旧扫描会被当成「现在」参与融合——输出无异常但描述的是旧世界（安静的毒数据）。

## 决策

### 打戳规则（生产端，5 条）

1. **最早可得点打戳**：仿真合成=合成时刻；真实驱动=上游 `header.stamp`（对应总线/硬件戳）；软件戳（收包时刻）只是兜底且须可区分；
2. **驱动只透传、不覆盖**：`sick_tim781_adapter::read_impl` 现将 `header.stamp` 转为 `stamp_ns` 填入结构体（修复了原先的丢弃）；
3. **Domain 层零 ROS 依赖**：戳为裸 `int64_t stamp_ns`，时钟由 infra 层持有并传入（保持本项目分层纪律）；
4. **stamp_ns==0 语义 = 未盖章**：消费端保守拒绝——强制显式打戳，不靠默认值蒙混；
5. 单一时钟域（v1：节点 ros clock）。跨域换算（设备↔主机↔云）见「v2 方向」。

### 消费规则（fusion 端）

- 新增 `StampGate`（纯 Domain，`domain/perception/stamp_gate.hpp`）：按传感器注册容差，`check(stamp, now)` 判 OK/STALE，白名单语义（未注册容差的传感器直接拒绝）；
- fusion `timer_callback`：发布输出前过门控，**lidar 过期 → 抑制本拍输出并节流告警**——宁可空一拍，不用旧世界描述现在；
- 拒绝计数 `stale_count` 供可观测/测试断言。

## 验证证据

| 项 | 结果 |
|----|------|
| 单测 `test_stamp_gate`（新鲜/过期/未盖章/未注册/边界） | **5/5 通过** |
| 故障注入（`inject.lidar_stamp_age_ms:=600`，容差 200ms） | WARN `lidar snapshot stale (age 600ms, tol 200ms) - suppress fusion output tick` 按 2s 节流触发 ✓ |
| 对照组（无注入） | 0 条 stale 告警，正常发布 ✓ |

## v2 方向（真机时再做）

- 时钟域独立化：stamp 换 steady-clock 单调域，ros time 仅在消息出口换算（消除 sim_time 干扰）；
- 设备侧同步：手内 MCU ↔ 主机 offset 交换（四时间戳 + 线性回归滤漂移）或 EtherCAT DC；
- 多传感器全量门控（v1 只 gate lidar 关键路径）+ message_filters 近似时间策略做精确对齐缓冲；
- 数采出口：相对戳 + 墙钟锚点（云不需要 µs 同步——对齐发生在边缘）。

## 变更清单

- `hal/sensor/isensor.hpp`：3 结构体 +stamp_ns
- `hal/sensor/sick_tim781_adapter.hpp`：read_impl 透传真值
- `domain/perception/stamp_gate.hpp`：新增
- `infrastructure/fusion_node.{hpp,cpp}`：参数（stale.*.ms / inject.*）+ 门控接线 + standalone 可执行（验证用）
- `quality/src/test_stamp_gate.cpp`：5 例单测
