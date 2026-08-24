#ifndef ROS2_ROBOT_MIDDLEWARE_DOMAIN_SCHEDULING_TASK_SCHEDULER_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DOMAIN_SCHEDULING_TASK_SCHEDULER_HPP_

/// @file   task_scheduler.hpp
/// @brief  任务调度策略内核（纯 Domain）——优先级 + 可抢占 + 同级 FIFO。
///
/// 定位：职责4「任务调度」的策略层最小内核。上层语义（mission 状态机/行为树）
/// 排队到这里，内核只管三件事：谁先跑（优先级）、能不能插队（抢占）、何时取下一个。
/// 产品化方向（需要时再加）：定时触发/截止期(EDF)/持久化——见各 TODO 注释。

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace amr {
namespace scheduling {

struct Task
{
  uint64_t id = 0;
  std::string name;
  int32_t priority = 0;      // 数值大 = 优先级高
  bool preemptible = true;   // 低优先可被高优先抢占；紧急任务可声明不可抢占
};

class TaskScheduler
{
public:
  /// 入队。若新任务优先级高于当前运行任务且当前可抢占 → 当前任务被挤回队首，
  /// 新任务立即成为 running（抢占语义在此刻发生）。
  /// 返回 true = 本次提交触发了抢占。
  bool submit(const Task & t)
  {
    if (running_ && t.priority > running_->priority && running_->preemptible) {
      ready_.push_front(*running_);   // 被抢占者回到队首（恢复时最先跑）
      running_ = t;
      return true;
    }
    insert_by_priority(t);
    if (!running_) advance();
    return false;
  }

  /// 完成当前任务 → 取下一个（最高优先级，同级 FIFO）
  std::optional<Task> complete(uint64_t id)
  {
    if (!running_ || running_->id != id) return std::nullopt;
    auto done = *running_;
    running_.reset();
    advance();
    return done;
  }

  std::optional<Task> running() const { return running_; }
  size_t queued() const { return ready_.size(); }

  /// 快照（诊断/监控用）
  std::vector<Task> snapshot_queue() const
  {
    return {ready_.begin(), ready_.end()};
  }

private:
  void insert_by_priority(const Task & t)
  {
    auto it = ready_.begin();
    while (it != ready_.end() && it->priority >= t.priority) ++it;  // 同级 FIFO：插到同级尾
    ready_.insert(it, t);
  }
  void advance()
  {
    if (!ready_.empty()) {
      running_ = ready_.front();
      ready_.pop_front();
    }
  }

  std::optional<Task> running_;
  std::deque<Task> ready_;
};

}  // namespace scheduling
}  // namespace amr

#endif  // ROS2_ROBOT_MIDDLEWARE_DOMAIN_SCHEDULING_TASK_SCHEDULER_HPP_
