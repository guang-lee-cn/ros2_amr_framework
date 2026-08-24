# DDS QoS 实测与三家对比笔记（U3，2026-08-24）

> 数据：`results/bench4_20260824_192604.jsonl`（CycloneDDS @ WSL2，方法同 ADR-001）
> 结论服务于选型/调优叙事，非绝对性能承诺。

## 1. QoS 时延矩阵（reliable vs best_effort）

| 载荷 | Reliability | RTT P50 | RTT P99 | RTT max |
|------|-------------|---------|---------|---------|
| 1KB | reliable | 212µs | 630µs | 6.6ms |
| 1KB | best_effort | 209µs | 542µs | 2.7ms |
| 1MB | reliable | 3.4ms | **317ms** | **613ms** |
| 1MB | best_effort | 2.8ms | **4.7ms** | 5.6ms |

**读数（比数字重要的三条）**：

1. **可靠性不花中位数，花尾部**：小消息两者几乎无差（无分片→无重传）；1MB 时 reliable 的 P50 也还好，但 P99 崩到 317ms——分片丢一片就触发 HEARTBEAT/ACKNACK 重传，串行 ping-pong 把重传代价放大两个数量级。
2. **1MB best_effort 的 P99 比 reliable 好 67 倍**（4.7ms vs 317ms）。对「晚到的帧等于死帧」的流式数据（数采、视觉流），best_effort 是架构正确选择，不是省事选择。
3. 跨 run 方差本身就是发现：本轮 1MB reliable P50 3.4ms vs 基准一轮 14.5ms——回环缓冲压力下尾部不稳定，**尾部数字必须带运行条件声明**。

## 2. Durability 晚加入补帧（教科书结果）

| Durability | 发布 K 帧后订阅者启动 | 补收 |
|------------|----------------------|------|
| transient_local (depth=5) | 收到 seq 1-5 | **5/5 ✓** |
| volatile | 收到 0 帧 | 0 ✓ |

落点：机器人描述/静态配置话题必须 TransientLocal（晚启动的节点不用等下一次发布）；高频流必须 Volatile（晚加入者被灌一屏旧数据反而是事故）。

## 3. 三家对比（本地实验 + 文档口径，诚实标注）

| 维度 | FastDDS | CycloneDDS | RTI Connext |
|------|---------|------------|-------------|
| 出身/许可 | eProsima，Apache2 | Eclipse 基金会（ZettaScale 主导），EPL | 商业许可（付费），有 DDS-Security/认证版 |
| 本地实测 | WSL2 discovery 偶发不通（ADR-001 环境观察）；默认传输可跑通（清理后） | **全程稳定**；1KB/1MB/1M 时延矩阵 + QoS 矩阵全出自它 | **无本地实验**——仅文档认知 |
| ROS2 地位 | Jazzy 默认 | 一等公民（zero-copy/iceoryx 集成的官方主推方向） | rmw_connextdds 可用，工业/军工场景多 |
| 差异化 | XML profile 生态全 | 极简依赖、iceoryx 零拷贝路径 | 安全认证（DO-178 级）、商用支持 |

选型口径：默认 CycloneDDS（本机实证稳定 + 零拷贝路线图），FastDDS 保留可换（防腐层价值再次实证），RTI 在强认证需求时评估。

## 4. DDS Discovery 取证清单（下次复现时执行）

三刀法：先分**发出/没发出**，再分**收到/没收到**，最后分**匹配/没匹配**——网络问题 / 配置问题 / 版本问题各归其位。

| 假说 | 机制 | 验证动作 |
|------|------|---------|
| H1 多播路径 | PDP 依赖 UDP 多播；WSL2 eth0 走 NAT、IGMP 残缺；SHM 传输只换数据面，discovery 元数据仍走多播（实测佐证：SHM 模式同样失败） | `tcpdump -i any host 239.255.0.1`（发出？应答？）；对比 `ss -ulnp` 两家监听端口 |
| H2 环境污染 | 残留 participant 占用端口/participant_id，新参与者匹配异常（实测：清出 9 个孤儿 pong 后同配置复测成功） | `ss -ulnp \| grep -E "5800\|7400"`；清场后立即复测（复现-清除-恢复三段论） |
| H3 回退差异 | 多播不通时 Cyclone 对 localhost 有单播 SPDP 回退可自愈；FastDDS 默认周期静默重试表现为挂死 | 同故障现场抓包：有无单播 SPDP 探测报文 |

## 关联

- 基础矩阵：`docs/results-2026-08-20.md` ｜ 方法与局限：`docs/ADR-001-v0-scope.md`
- 复跑：`scripts/run_bench4.sh`
