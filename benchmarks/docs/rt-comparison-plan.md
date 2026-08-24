# PREEMPT_RT 复测 Runbook 与对照表（基准五）

> 目标：同一套 DDS 基准脚本在「WSL2 / 裸机非RT / 裸机 PREEMPT_RT」三环境对照，
> 产出「实时内核对 P99 尾部改善」的实测数据——JD 加分项（PREEMPT_RT）的实证闭环。

## 一、旧机器安装 Runbook（用户操作，约 30 分钟）

```bash
# 1. 裸机安装 Ubuntu 24.04 LTS（或更新；不能在 WSL2/虚拟机里做——时钟虚拟化）
# 2. 装 RT 内核（Ubuntu 官方源，无需自己打补丁）
sudo apt update
sudo apt install linux-realtime rt-tests
sudo reboot
# 3. 验证：uname -v 输出含 "PREEMPT_RT" 即成功
uname -v
# 4. 装 ROS2 Jazzy 基座 + 构建
sudo apt install ros-jazzy-ros-base ros-jazzy-rmw-cyclonedds-cpp \
     ros-jazzy-hardware-interface ros-jazzy-pluginlib \
     python3-colcon-common-extensions build-essential git
git clone https://github.com/guang-lee-cn/ros2_amr_framework.git
cd ros2_amr_framework
colcon build --packages-select ros2_robot_middleware --cmake-args -DCMAKE_BUILD_TYPE=Release
./benchmarks/build.sh
# 5. 一键复测
./benchmarks/scripts/run_bench5_rt.sh
```

注意：RT 内核与部分闭源驱动（NVIDIA）冲突——旧机器用核显即可；GRUB 默认启动项会指向
RT 内核，想切回普通内核在开机高级选项里选。

## 二、采集矩阵（三个环境 × 相同脚本）

| 环境 | 状态 | 数据 |
|------|------|------|
| WSL2（12核 x86，非RT） | ✅ 已采 | `results/bench1_20260820_153636.jsonl`、`bench4_20260824_192604.jsonl` |
| 裸机 x86（非RT） | ⬜ 待采 | 装普通 Ubuntu 跑一次 `run_bench5_rt.sh`（preempt_rt:false） |
| 裸机 x86（PREEMPT_RT） | ⬜ 待采 | `apt install linux-realtime` 后再跑一次（preempt_rt:true） |

结果文件首行自动带 `preempt_rt` 元数据标签——不同环境的数据永不混淆。

## 三、对照表模板（数据齐后填写）

| 指标 | WSL2 非RT | 裸机 非RT | 裸机 RT | RT 改善 |
|------|-----------|-----------|---------|---------|
| cyclictest max（加载下，µs） | n/a | ___ | ___ | ___ |
| inter 1K reliable P99（µs） | 630* | ___ | ___ | ___ |
| inter 1M reliable P99（µs） | 316587* | ___ | ___ | ___ |
| inter 1M best_effort P99（µs） | 4723* | ___ | ___ | ___ |
| intra 1M P99（µs） | 253* | ___ | ___ | ___ |

\* WSL2 基线取自 bench1/bench4（不同硬件，只作形态参照；严谨结论以同机 RT 前后对照为准——
裸机非RT 与 RT 是同一台机器，这才是论文级对照）。

## 四、预期与解读口径（先立假设，数据回来验证）

- **cyclictest**：RT 对 max 尾部改善最显著（非RT 加载下可到几十 ms；RT 通常压到几十 µs）——
  这是 RT 的主战场（调度确定性），预期改善 100× 量级；
- **DDS P99 尾部**：RT 应显著改善 1M reliable 的尾部（调度抖动是尾部元凶之一），
  但协议栈自身开销不变，P50 改善预期有限——「RT 修尾巴不修中位数」与 QoS 实测
  （reliable 尾部 67× 恶化）是同一个洞察的两次验证；
- **intra 零拷贝**：受调度影响最小，预期改善最小——反向印证「零拷贝路径离内核远」。

## 五、后续（第二、三步路线图）

1. SOEM+SOES veth 对打（¥0 EtherCAT 协议层）→ 2. 树莓派5 RT + 二手 EtherCAT 驱动（雷赛
   DM3E/台达 B3-E）+ SOEM → 3. 写 ros2_control 的 EtherCAT hardware_interface 插件
   （DiffDriveSystem 第二季：仿真闭环 → 总线真机闭环）。
