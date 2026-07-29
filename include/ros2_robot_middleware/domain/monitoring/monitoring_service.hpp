#ifndef ROS2_ROBOT_MIDDLEWARE_DOMAIN_MONITORING_SERVICE_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DOMAIN_MONITORING_SERVICE_HPP_

#include "ros2_robot_middleware/domain/monitoring/heartbeat_analyzer.hpp"
#include "ros2_robot_middleware/domain/monitoring/recovery_policy.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace amr {
namespace domain {
namespace monitoring {

// MonitoringService — orchestrates heartbeat analysis + recovery policy.
// Pure C++, zero ROS2 dependency.
// Moved from application/ → domain/monitoring/ (application/ was too thin).
class MonitoringService {
public:
  using Analyzer     = HeartbeatAnalyzer;
  using Recovery     = RecoveryPolicy;

  void register_node(const std::string &name, double timeout_s) {
    NodeHeartbeat hb;
    hb.node_name = name;
    hb.timeout_s  = timeout_s;
    hb.last_seen_s = -1.0;
    heartbeats_[name] = hb;
  }

  void heartbeat_received(const std::string &name, double age_s = 0.0) {
    auto it = heartbeats_.find(name);
    if (it != heartbeats_.end()) it->second.last_seen_s = age_s;
  }

  void tick(double dt) {
    for (auto &[name, hb] : heartbeats_) {
      if (hb.last_seen_s >= 0) hb.last_seen_s += dt;
    }
  }

  NodeStatus evaluate(const std::string &name) const {
    auto it = heartbeats_.find(name);
    if (it == heartbeats_.end()) return NodeStatus::STALE;
    return analyzer_.evaluate(it->second);
  }

  bool should_recover(const std::string &name) {
    auto status = evaluate(name);
    auto &rec = recovery_[name];
    bool ok = recovery_policy_.should_recover(status, rec);
    if (ok) rec.attempts++;
    return ok;
  }

  void on_recovered(const std::string &name) {
    recovery_[name].attempts = 0;
  }

  NodeStatus escalated_status(const std::string &name) {
    auto status = evaluate(name);
    return recovery_policy_.escalate(status, recovery_[name]);
  }

  std::vector<NodeHeartbeat> snapshot() const {
    std::vector<NodeHeartbeat> result;
    result.reserve(heartbeats_.size());
    for (const auto &[_, hb] : heartbeats_) result.push_back(hb);
    return result;
  }

  Analyzer::Summary summary() const {
    return analyzer_.summarize(snapshot());
  }

  const auto &heartbeats() const { return heartbeats_; }

  static FleetSummary fleet_summary(
    const std::vector<Analyzer::Summary> &per_amr) {
    return fleet_summarize(per_amr);
  }

private:
  Analyzer analyzer_;
  Recovery recovery_policy_;
  std::unordered_map<std::string, NodeHeartbeat> heartbeats_;
  std::unordered_map<std::string, Recovery::RecoveryState> recovery_;
};

}  // namespace monitoring
}  // namespace domain
}  // namespace amr

#endif
