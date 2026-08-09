# 调试复盘 + 大模型协作方法论

> 2026-08-07 ~ 08-09，AMR 绕障闭环（scene_simulator）+ Ignition gz-sim 对接。
> 过程中两次重大误判（#504 归因 / 绕障看部分轨迹），都在用户质疑下纠正。
> 本文复盘问题 + 提炼「如何和大模型沟通能及早发现问题、缩短定位路径」。

## 一、问题 + 思考路径 + 解决

### A. 绕障闭环（scene_simulator 路径）

| 问题 | 现象 | 曲折/错误路径 | 真因 | 解决 |
|---|---|---|---|---|
| test_scan_to_grid Theta90 | robot(20,20) 越界 | 以为测试逻辑错 | cell 边界浮点截断：robot 压边界，cos(π/2) 微偏让 world_to_grid 跳格 | robot 用 cell 中心坐标 |
| test_decision BlockedGoal | 堵死却下发 | 以为 on_result 没调 | objects 没标 grid（uint8 重构砍了 grid_updater 调用） | 补 grid_updater.inflate |
| objects 没进 grid（运行时） | box_cell=0 | — | objects 在 chassis frame，直接标 map 错位 | robot pose 旋转 chassis→map |
| 车卡 box 前 | cmd_vel≈0 | **误判 "guard 0.3m 死锁"** | A\* inscribed(0.22) < guard stop(0.30)，path 贴 box 被 guard 拦 | inscribed 0.22→0.35 |
| 往返漂移 | 车不停、漂出 grid | 以为 decision 不重规划 | publish_twist 经 smoother，(0,0) 一次只减一步，motor idle 后 scene 用残留 cmd | (0,0) 直接归 0 绕过 smoother |

### B. Ignition gz-sim 对接

| 问题 | 现象 | 曲折/错误路径 | 真因 | 解决 |
|---|---|---|---|---|
| gpu_lidar headless | /scan 全 inf | 跟注释以为 gpu_lidar bug | headless WSL2 无 GPU 上下文 | GUI 模式 |
| **#504 误判** | world_pose 不更新 | **跟代码注释归因 #504（没读 issue）→ 试 GUI/cpu lidar/headless 绕大弯** | #504 是 ogre2 射线几何失真，**不是** world_pose；world_pose 是缓存字段，看 ranges 才准 | rclpy 直订阅看 ranges |
| amr.sdf support | gz "Unsupported geometry" | — | `<sphere>` 缺 `<geometry>` 包裹 | 加 geometry wrapper |
| gz_bridge YAML | topic_name + gz/ros_topic_name 互斥 | 猜 schema 两次都错 | tb4 用 `ros_topic_name`+`gz_topic_name`（无 topic_name，无 qos_profile） | 读官方 tb4_bridge.yaml |
| mock_amcl 崩 | ParameterAlreadyDeclared | — | use_sim_time 是 ROS 内置，重复 declare | 删 declare |
| **绕障误判** | "车直线穿 shelf" | **只看后段 x=8-15（直线 y≈0）就断言没绕** | 车实际绕了 shelf（前段 y→1.21 绕 shelf(3,0)） | 看完整轨迹 |

---

## 二、大模型协作方法论（重点）

### 1. 两次误判的共同模式

- **#504**：跟代码注释的归因（"world_pose bug"），**没读原始 issue 验证** → 归因错，绕大弯试 GUI/cpu lidar/headless，浪费大量时间。
- **绕障**：**只看部分数据**（后段直线）下结论"没绕"，没看完整轨迹 → 误判，差点换 Webots。

**根因**：大模型倾向 ①**附和上下文**（注释/前文假设，确认偏差）②**看部分数据急于收敛** ③**倾向动手而非先验证假设**。

### 2. 如何和大模型沟通能及早发现问题

**(1) 质疑归因，要"如何确认"**
大模型给"根因是 X"时，追问"你怎么确认？读了原始资料吗？"
> 案例：用户问"#504 是社区 bug 吗？确认是这个影响吗？**你是如何确认的**？" → 逼我读 issue，立刻发现归因错。

**(2) 要完整数据，防"部分下结论"**
大模型看一段数据下结论时，要"完整数据/轨迹是什么？"
> 案例：我看后段直线说"没绕"，用户说"接着调" → 看完整轨迹发现车绕了 shelf（y→1.21）。

**(3) 要求验证方法可靠**
大模型用 echo/python/grep 踩坑（截断 / .inf / 慢）时，问"这方法可靠吗？有没有更直接的？"
> 案例：ros2 echo 截断长数组、python `.inf` 报错、gz topic -e 超时，三连败 → 用 rclpy 直订阅才拿到准数据。

**(4) 反向问"有没有可能不是 / 产品代码没问题吧"**
大模型附和假设时，反向问逼它找反例。
> 案例：注释说 gpu_lidar bug，用户问"**产品代码没问题吧？**" → 逼我验证计算容器通用 + gpu_lidar 实际行为，分清"代码问题"vs"工具/环境问题"。

**(5) 让大模型先说验证计划**
结论前要它先说"我怎么验证这个假设" → 暴露方法缺陷。
> 反例：我直接拿 world_pose 字段当结论，没说"我要看 ranges 才算 scan 真实数据"。

### 3. 大模型的局限（协作时心里有数）

- **附和上下文**：注释/前文说啥就往那靠（确认偏差），少主动找反例
- **工具踩坑**：echo 截断 / python `.inf` / `pkill -f` 自杀 / cwd 持久 / colcon 漏编
- **部分下结论**：看一段数据就收敛，急于"动手修"
- **重动作轻验证**：倾向改代码/起仿真，少先验证假设

### 4. 高效提问清单（用户侧，照着问能少走弯路）

- "这个根因，**你怎么确认的**？读了原始 issue/完整数据吗？"
- "**完整轨迹/数据**是什么？有没有反例？"
- "验证方法**可靠**吗？（echo 会不会截断？.inf 能转吗？）"
- "**产品代码没问题吧**？问题在工具/环境吗？"
- "先说**验证计划**，别急着改。"

---

## 三、结论

绕障闭环 + Ignition 对接都成功（计算容器不改一行代码，接标准 Gazebo 绕 shelf 到 goal）。两次误判（#504 + 绕障）暴露大模型的**确认偏差 + 部分下结论**倾向；用户的四次有效质疑（"#504 确认?""完整数据?""产品代码?""接着调(看完整)"）是纠正关键。

**协作核心**：大模型给结论时，要求**验证路径 + 完整数据 + 反例**，别让它附和假设一路走到底。大模型是高效的执行者，但归因和收敛判断需要人来把关。
