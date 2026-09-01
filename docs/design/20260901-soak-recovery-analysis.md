# Soak 恢复链失败分析（ISO 框架 · 2026-09-01）

## 一、症状 → 根因链

```
症状: scene_simulator 注入后 900s 超时未恢复
  ↓
发现: 全部进程状态 = Z（zombie/defunct）
  ↓
根因: Docker 容器 PID 1 = sleep infinity
      → 不调用 wait() 回收子进程
      → kill -9 后进程变僵尸，永久残留
  ↓
放大: 僵尸 PID 仍被 pgrep 匹配
      → soak 注入 kill -9 打在僵尸上（无效）
      → 恢复检测永远看不到新进程起来
  ↓
二次: supervisor 自身也是僵尸
      → 说明 ros2 launch 父进程也死了
      → supervisor 的 posix_spawn 子进程随之变孤儿→僵尸
```

## 二、三层修复方案

### Phase 1 · 环境修复（Docker init）

| 项 | 修法 | 验证门 |
|---|------|--------|
| Docker `--init` | 容器加 `--init` 标志——PID 1 变为 tini（正确回收僵尸） | `docker run --init ...` 后 kill 子进程，`ps` 无 Z 状态 |
| 僵尸检测 | soak 脚本的 pgrep 加僵尸过滤（读 /proc/PID/stat 排除 Z） | 僵尸不被当作活进程 |

### Phase 2 · supervisor 恢复链修复

| 项 | 修法 | 验证门 |
|---|------|--------|
| supervisor 存活 | 确保 supervisor 不因子进程死而自身退出（检查退出路径） | kill scene_simulator 后 supervisor 仍在运行 |
| 重启后健康 | scene_simulator 重启后 /odom 恢复发布 | 探针 V≥1 |

### Phase 3 · 重跑

清僵尸 → `--init` 容器 → supervisor → soak
