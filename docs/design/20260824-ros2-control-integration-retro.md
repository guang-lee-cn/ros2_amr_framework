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

## 复盘结论（可迁移的条目）

1. **「接管」是个双动词**：命令取自当前状态 + 状态显式初始化，缺一半就是 NaN；
2. Jazzy 三个版本差异坑（接口内存 NaN 初始化 / robot_description 走话题 / cmd_vel 收 TwistStamped）——**写插件看本机头文件，不看旧教程**；
3. 静默故障三件套排查顺序：`ros2 topic info -v`（类型）→ cm 日志（update 错误）→ `/joint_states`（NaN 检查）；
4. 编排脚本必须入口清场 + 全量 pid 追踪。

## 关联

- 插件：`src/hal/diff_drive_system.cpp` · URDF：`urdf/diff_drive_ros2_control_test.urdf` · 配置：`config/diff_drive_ros2_control.yaml` · 验证：`scripts/verify_ros2_control.sh`
- 面试叙述版本：`mdDoc/interview/brainco/01-ros2-control.md`
