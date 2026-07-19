# 决策管线

## 在总体架构中的位置

```mermaid
flowchart LR
    FUSION[融合管线] --> D[DecisionNode<br/>PlanningService]
    D --> MOTOR[执行管线]
    
    style D fill:#fff3e0,stroke:#f57c00
```

> 决策管线是控制流的入口——接收感知结果，决定是否发送导航目标。

## 核心业务

```mermaid
sequenceDiagram
    participant DDS as perception_sub_
    participant DN as DecisionNode
    participant PS as PlanningService
    participant ACT as Action Client

    Note over DDS,ACT: on_perception (event-driven)

    DDS->>DN: PerceptionObjects 到达
    DN->>PS: should_preempt(has_active_goal)
    
    alt 需要抢占
        DN->>ACT: cancel_active_goal()
    end

    PS->>PS: select_goal(objects, count)
    PS-->>DN: Goal{x, y}

    DN->>ACT: async_send_goal(goal)
    ACT-->>DN: GoalHandle (future)

    Note over ACT: 异步等待执行管线响应<br/>Goal→Feedback→Result
```

### 抢占/重试策略

- **抢占**：新感知目标到达 + 上一个 goal 未完成 → `cancel_active_goal()` → 发送新 goal
- **重试**：goal 被拒绝 → 最多重试 `kMaxRetries` 次（`TargetSelector`）
- **取消**：执行管线接受 `CancelResponse::ACCEPT`，交由执行管线自己决定如何取消

## 依赖

| 依赖 | 说明 |
|------|------|
| `domain/planning/target_selector.hpp` | 感知目标 → Goal 映射 |
| `domain/planning/preempt_policy.hpp` | 抢占决策 |
| `PerceptionObjects` | 来自融合管线的 DDS 消息 |
| ROS2 Action Client | `MoveToPose` action |

## 被依赖

- [执行管线](actuation-pipeline.md) — 接收 `MoveToPose` goal

## 关键设计决策

- **Action 而非 Service**：导航是长时间运行的任务，需要 Goal→Feedback→Result + 取消能力。Service 是请求-响应，不适用
- **异步发送**：`async_send_goal` 返回 future，不阻塞回调线程
