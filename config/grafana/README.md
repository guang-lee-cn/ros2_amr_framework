# Grafana + Prometheus 可观测性

## 数据源端口

| 端口 | 进程 | 指标 |
|:---:|------|------|
| **9090** | health_monitor_node | 健康状态、传感器速率、降级等级、融合延迟（历史聚合） |
| **9091** | compute_container | AMR_PERF_PHASE 插桩数据（阶段延迟 count/avg/p50/p99）——仅 AMR_PERF_INSTRUMENTATION=ON 构建 |

## 快速启动

```bash
# 1. 启动系统（CI 模式构建，带 9091 插桩端点）
colcon build --packages-select ros2_robot_middleware --cmake-args -DAMR_PERF_INSTRUMENTATION=ON
source install/setup.bash
ros2 launch ros2_robot_middleware system.launch.py

# 2. 验证端点
curl -s localhost:9090/metrics | grep amr_fusion_latency   # health 指标
curl -s localhost:9091/metrics | grep amr_phase            # 插桩指标

# 3. Prometheus（若有）
#    prometheus.yml:
#      scrape_configs:
#        - job_name: 'amr_health'
#          static_configs: [{ targets: ['localhost:9090'] }]
#        - job_name: 'amr_perf'
#          static_configs: [{ targets: ['localhost:9091'] }]

# 4. Grafana
#    导入 amr_dashboard.json（Import → Upload JSON）
#    datasource uid 用 ${DS_PROMETHEUS}，或替换为你的 Prometheus datasource uid
```

## Dashboard 面板

| 面板 | 数据源 | 说明 |
|------|:---:|------|
| Phase Latency (avg) | :9091 | fusion/decision/motor 各阶段平均耗时 |
| Phase P99 | :9091 | 各阶段 P99 延迟（抖动监控） |
| Fusion/Motor Latency | :9090 | 系统级聚合延迟 |
| Sensor Rates + Degradation | :9090 | 传感器速率 + 降级等级（双轴） |
| Objects + Fusion Cycles | :9090 | 物体数 + 融合周期率 |

## 注意

- 9091 端点仅在 `AMR_PERF_INSTRUMENTATION=ON` 构建存在（生产构建零插桩）
- 插桩指标名含 `:`（如 `amr_phase_fusion:tick`）——Prometheus 允许，但需在表达式里加引号或注意转义
