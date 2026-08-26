# 复盘：B1+A2 收敛线挖出的 5 个 bug（2026-08-25）

> 框架沿用 docs/design/20260809-debugging-retro-and-llm-collaboration.md。
> 背景：迭代 2 B1（supervisor）+ A2（soak）收敛线，从落地到 supervised_sim
> rollout 的过程中挖出 5 个 bug，全部带铁证修复（change journal 当日两条）。
> 本文不复述修复细节，只提炼**曲折路径与模式**。

## 一、问题 + 曲折路径 + 解决

| # | 问题 | 现象 | 曲折/错误路径 | 真因 | 解决 |
|---|------|------|--------------|------|------|
| 1 | oneshot 无限重跑 | spawn_amr 每 ~2s 重放车，gz 里堆机器人 | 初读日志误以为是「设计内退避重试」（gz 未就绪 create 失败）——看到 `RUNNING→STOPPED→STARTING` 循环才意识到 exit-0 也被重生 | `completed_` 只在级联时 erase、从不在成功时置位——写侧遗漏（本日新代码） | feed() 里 oneshot EXITED_OK → 置位 + 「完成✓」日志 |
| 2 | 级联漏清 oneshot | gz 重启后新世界无车 | 若不显式验证「gz 杀后 spawn_amr 必须重跑」，此 bug 无症状滑过 | cascade_yield 的 `continue` 跳在 `completed_.erase` 之前——STOPPED（已完成）子项永远漏清 | 先清标记再判相位 |
| 3 | 清场孤儿 foxglove 占 8765 | foxglove 启动即 Bind Error，崩溃循环直至 FATAL | 先怀疑 foxglove 自身不稳定/退避策略问题——`terminate: Bind Error` 日志 + `ss -tlnp` 才锁定端口被孤儿占 | run_sim clean 的 pkill 清单不含 foxglove_bridge/static_transform_publisher（路径不在本包 lib 下）；**08-16「清单不全留孤儿」同型第三次复发** | clean() 补齐两个模式 |
| 4 | SHM 无效 glob + probe 污染 | 连续 3/3「不健康」，一度归因为 WSL2 彩票冷 | **附和上下文**：journal 里彩票劣化是已知主因，先入为主判「机器状态差、建议 wsl --shutdown」——直到错误文本出现在捕获的 `V` 里才惊觉 | ①`rm /dev/shm/fastdds*` 多年无效（实际 `fastrtps_*` 段 + `sem.fastrtps_port*_mutex` 锁）；②FastDDS C++ 日志走 stdout 污染 probe 输出，`[` 比较报 integer expression | 三类文件全清 + probe 只取纯数字行（三脚本同修） |
| 5 | 交互式 pkill 自匹配自杀 | 清场命令「静默没跑」，栈看似复活 | 首判「watchdog 的 run_sim 子进程重启了栈」——错归因一轮；复盘输出为空（连 tail 都没打印）才定位 | `pkill -f 'gz sim'` 匹配到自身 shell 命令行把自己杀了——**CLAUDE.md 禁止清单原文明示过的坑** | 方括号模式/显式 PID；脚本内 clean() 本就安全（独立进程 cmdline） |

## 二、模式提炼

### 1. 两个 cluster，两类根因

- **写侧（#1/#2）**：都长在「domain 与 infra 的接缝」——`completed_` 活在
  infra、语义属于 domain。22 个单测锁死了状态机，但**接缝处的簿记没有
  单测也容易被集成验证漏过**（#2 若不显式验证「gz 杀后必须重跑」就无症状）。
  → 新旧层接缝的状态簿记要进验证清单，不能只测新层内部。
- **读侧（#3/#4/#5）**：全是**存量或已文档化的坑**在 supervisor 快速重启的
  新压力下复发/放大。#3 同型第三次；#5 是 CLAUDE.md 明文禁止项。
  → **写在文档里的坑挡不住手滑**——要么长进工具（脚本内安全模式），
  要长进 CI（lint/门禁拦截）。「清单靠人脑维护」本身是元问题。

### 2. supervisor/soak 的元收益：让潜伏 bug 必现

#3/#4 潜伏多年（fastdds glob 自 clean() 诞生起就没生效过），此前只是偶发
噪声；supervisor 的秒级重启把它们从「偶发」放大成「必现」。**故障注入类
基础设施的价值不只是验证恢复，更是压力探针**——这正是 A2「72h soak」
预期的副产品。

### 3. 大模型协作视角（延续 0809 框架）

- **#4 是教科书式「附和上下文」**：环境里有已知解释（彩票劣化）时，
  归因直接往它靠，没先问「V 的原始值到底是什么」。纠正契机是错误文本
  混进捕获值这个**反常细节**——本次无用户质疑，靠的是「输出里有不该有
  的东西」自检。可复用的自检项：**比较/判定前先看原始捕获值**。
- **#5 是「已知坑再踩」**：知识在 CLAUDE.md 里，但交互式操作时没调用它。
  与 0809 框架「工具踩坑」条目同类。对策已落地：坑要么编译进脚本
  （方括号模式），要么由 CI 拦（gitleaks 同理）。
- **一个反例值得记录**：#2 的发现不是运气——验证计划里显式写了「gz 击杀
  → spawn_amr 必须二次完成」，是**先声明验证断言再动手**的直接收益
  （0809 框架「先说验证计划」的正向案例）。

## 三、结论

五个 bug 无一来自被测的算法/状态机逻辑本身（domain 层全程零返工），
全部来自**接缝簿记（写侧）与运维工具链（读侧）**。收敛判据也一致：
每个 bug 都以「先有可观测的反常细节 → 再动归因」的顺序被抓住，没有
一个靠猜修复。两条防线落地：接缝验证清单（supervised_sim rollout 的
验证矩阵模板化）、坑进工具不进脑子（clean 模式齐套 + probe 数字过滤）。

| 防线 | 落点 | 状态 |
|------|------|------|
| 清场清单不再靠人脑 | run_sim clean() 模式齐套（foxglove/static_tf/fastrtps 三类） | ✅ 本次 |
| probe 输出契约 | 数字过滤三脚本同修 | ✅ 本次 |
| launch 从未运行即带语法错的防线 | `ast.parse` 进 CI lint（审计行动③） | ⏳ 下一件 |
| 交互式清场安全 | 方括号模式写进 CLAUDE.md 已有；考虑 scripts/clean_sim.sh 常驻 | 待办（低优） |
