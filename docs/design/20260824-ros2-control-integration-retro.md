# ros2_control 集成故障复盘（2026-08-24）

> 背景：AMR HAL 首次接入 ros2_control（DiffDriveSystem 插件 + controller_manager + 原厂控制器），验证链 `scripts/verify_ros2_control.sh`。
> 方法：按《Technical Research Framework》接口4（问题驱动：现象→原因→解决→预防）复盘。
> 环境：WSL2 · ROS2 Jazzy · ros2_control 4.x · RMW=CycloneDDS（FastDDS 在本机 WSL2 有 discovery 不稳定前科，见 ~/code/mw-bench ADR-001）。

## 集成成果（复盘对象的正常态）

- `cmd_vel(TwistStamped)` → diff_drive_controller → DiffDriveSystem 插件 → `/joint_states` 闭环，位置积分 6.25 rad 与 0.2m/s×2.5s÷0.08m 理论值精确吻合；
- claim 独占语义实证：第二个控制器抢 `left_wheel_joint/velocity` 被 cm 拒绝激活（日志原话在故障点 1 之后）。

---

## 故障点 1：状态接口 NaN 导致控制器被 cm 自动失活（核心复盘）

### 现象描述

控制器激活成功后，`/joint_states` 的 position/velocity 全为 `.nan`；发送 cmd_vel 后轮子不转；`list_controllers` 显示 diff_drive_controller 状态从 active 变 **inactive**。cm 日志：

```
[ERROR] [diff_drive_controller]: Either the left or right wheel position is invalid for index [0]
[ERROR] [controller_manager]: Deactivating controllers : [ diff_drive_controller ] as their update resulted in an error!
```

### 原因分析

Jazzy（ros2_control 4.x）的接口内存由**框架持有**，初始值是 **NaN 而非 0**（设计意图：强制插件显式声明初值，防止"默认 0 假装是真实采样"）。插件的 `on_activate()` 只写了 position 状态和 velocity **命令**，漏写 velocity **状态**接口 → NaN 进入控制器 update() → ddc 判定轮位无效 → cm 依"update 出错即失活"策略自动 deactivate。

根因是**对"接管语义"的理解只做了一半**：接管 = 命令初值取自当前状态 ✓ + **激活时显式写入全部状态接口** ✗（后者是前者的前提——你接管的"当前状态"必须先存在）。

### 实际影响

控制回路全链路瘫痪但**无崩溃、无异常抛出**——故障表现是"安静的 NaN"，只有 /joint_states 消费端和 cm 日志可见。这类故障在真机上对应"上电后机器人不动或控制器秒退"，排查窗口在 log 而非代码崩溃点。

### 解决方案

```cpp
// on_activate(): 显式初始化全部接口（状态+命令）
for (i : kWheels) {
  set_state(prefix + "position", wheel_pos_[i]);   // 原来有
  set_state(prefix + "velocity", wheel_vel_[i]);   // ← 补上这一行
  set_command(prefix + "velocity", wheel_vel_[i]); // 接管：命令=当前状态
}
```

修复后位置/速度正常出数，位置积分与理论值吻合。

### 预防措施

- [x] 设计阶段：硬件插件模板（脚手架）固化「on_activate 必须写满全部已导出接口」——新插件从模板拷贝即自带此行；
- [x] 代码审查：插件 review 清单增加检查项「export 的每个接口在 activate 路径上都有显式写入」；
- [ ] 测试覆盖：gtest 冒烟用例「激活后 1s 内所有 state_interface 非 NaN」（接入 CI）；
- [ ] 文档：本复盘即是——坑一旦有名字，就不会踩第二次。

---

## 故障点 2：cm 收不到 robot_description（参数 vs 话题）

**现象**：cm 启动后无限刷 `Waiting for data on 'robot_description' topic`，spawner 全部挂起直至超时。
**原因**：Jazzy 的 `ros2_control_node` **不接受** `-p robot_description:=...` 参数，只从话题收（预期由 robot_state_publisher 发布）。
**解决**：先起 `robot_state_publisher`（它接受参数并 latched 发布话题），再起 cm。
**预防**：验证脚本固定按 rsp → cm 顺序拉起；「参数能设 ≠ 框架会读」——Jazzy 前后的接口语义差异靠查头文件/日志确认，不靠旧教程记忆。

## 故障点 3：cmd_vel 静默类型不匹配（Twist vs TwistStamped）

**现象**：控制器 active、一切正常，发 cmd_vel 后轮子不转，**无任何报错**。
**原因**：Jazzy 的 ddc 顶层命令话题 `/diff_drive_controller/cmd_vel` 收 **TwistStamped**；发 Twist 类型不匹配 → 订阅端静默丢弃。另有一条 WARN 提示 TwistStamped 零时间戳会被 ddc 打成当前时间（`setting it to current time`）——发送端应填 stamp。
**解决**：发布 `geometry_msgs/msg/TwistStamped '{twist: {linear: {x: 0.2}}}'`。
**预防**：排障口诀——**「发了但没反应，先 `ros2 topic info -v` 查发布/订阅类型是否一致」**；静默丢弃是 ROS2 类型化话题的固有行为，不是 bug。

## 故障点 4：双 controller_manager 并存 + 孤儿进程（环境类）

**现象**：第二轮验证时 spawner 报 `Failed loading controller`（上一轮全部正常）。
**原因**：脚本被 timeout 杀死时清理段未执行，旧 cm/rsp 成孤儿；两套 cm 同域并存，spawner 的服务调用打到旧实例（同名控制器已存在 → 加载失败），而 `/joint_states` 来自旧实例的 broadcaster。
**解决**：验证脚本加入口清场（`pkill -f "controller_manager/ros2_control_node"` + `pkill -x robot_state_publisher`）。
**预防**：任何编排脚本遵循「**入口清场 + trap 退出清理**」双保险；孤儿进程是 WSL2/后台作业的常态，不是异常。（与 mw-bench 基准三的孤儿 pong 教训同源——工具脚本一律直接 exec 二进制并追踪 pid。）

---

## 增补：基准验证与仓库整合阶段问题清单（2026-08-24 下午）

ros2_control 集成之外，同日基准工程与仓库整合阶段踩到的问题，按「现象→根因→修复」归档：

| # | 现象 | 根因 | 修复/预防 |
|---|------|------|----------|
| 5 | benchmarks/ 并入后 colcon 发现不了 bench_ipc | colcon 把包目录视为叶子，不向包内递归；仓库根即主包 | `benchmarks/build.sh` 显式 `--base-paths benchmarks/bench_ipc` |
| 6 | cyclictest 结果为 null | `-m`(mlockall) 需 root，失败时 stderr 被吞静默退出；WSL2 上另有 `RLIMIT_RTPRIO=0`，非 root 完全不可运行 | 脚本显式检测 root/sudo 免密并报错；裸机用 sudo 跑；WSL2 结论=平台限制，数据列记 n/a |
| 7 | 脚本 `set -u` 后 source ROS setup.bash 即崩 | `AMENT_TRACE_SETUP_FILES` 未定义触发 unbound | ROS 生态脚本一律不用 `set -u`（source 之后再严格模式亦可） |
| 8 | pkill 命令自杀（整条 Bash 无输出退出） | `pkill -f "xxx"` 模式匹配到了包含同样文本的自身 shell 命令行 | 方括号技巧 `pkill -f "xx[x]"` 或按进程名 `pkill -x`；编排脚本入口清场 + pid 全量追踪 |
| 9 | zenoh 吞吐测试收发双零 | recv 以管道前台运行（`cmd | tee` 阻塞到 recv 超时），sender 从未启动；`$!` 拿到的是 tee 的 pid | 后台化 recv 直接落盘 `>> $OUT &`；后台+管道的组合要显式验证 `$!` 指向谁 |
| 10 | 故障恢复基准首轮全「无效」 | 判据要求 ≥400ms 静默后的首帧，而真实恢复（~300ms）快于阈值——判据与被测量自相矛盾 | 改物理判据：串行乒乓 RTT 亚毫秒 → kill 后 20ms 内的帧=在途回声，之后首帧=真恢复 |
| 11 | CI 隐患：U4 引入 hardware_interface 后 CI 必挂 | ci.yml apt 列表未含新依赖 | 补 hardware-interface/pluginlib + RMW 矩阵一并升级，首推即绿 |

## 增补 2：零拷贝迁移的 use-after-move（2026-08-24 夜，收敛件改造时捕获）

**现象**：声明式容器改造后 compute_container 段错误；Debug 构建定位到
fusion timer_callback 中 `publish(std::move(msg))` 之后的两处 `msg->objects.size()`。

**根因**：值语义改移动语义时，函数内**所有** post-publish 的 msg 使用点都要审——
第一处（metrics 行）当轮修复，第二处（20 行外的节流日志）因扫描只查了 publish 后
3 行而漏网，Release 下 UB 未显形、Debug 显形，二次捕获。

**教训三条**：
1. UB 在 Release 不显形 ≠ 不存在——换优化级别是免费的可疑代码探测器；
2. move 语义迁移的审查单位是「整个函数的 msg 生命周期」，不是「改动行 ±3 行」；
3. 更优解是** move 前把要用的值全部物化**（n_objects 提前取），而不是 move 后补救。

## 复盘结论（可迁移的条目）

1. **「接管」是个双动词**：命令取自当前状态 + 状态显式初始化，缺一半就是 NaN；
2. Jazzy 三个版本差异坑（接口内存 NaN 初始化 / robot_description 走话题 / cmd_vel 收 TwistStamped）——**写插件看本机头文件，不看旧教程**；
3. 静默故障三件套排查顺序：`ros2 topic info -v`（类型）→ cm 日志（update 错误）→ `/joint_states`（NaN 检查）；
4. 编排脚本必须入口清场 + 全量 pid 追踪。

## 关联

- 插件：`src/hal/diff_drive_system.cpp` · URDF：`urdf/diff_drive_ros2_control_test.urdf` · 配置：`config/diff_drive_ros2_control.yaml` · 验证：`scripts/verify_ros2_control.sh`
- 面试叙述版本：`mdDoc/interview/brainco/01-ros2-control.md`
