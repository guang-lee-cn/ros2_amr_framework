#ifndef ROS2_ROBOT_MIDDLEWARE_DOMAIN_HEARTBEAT_ANALYZER_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DOMAIN_HEARTBEAT_ANALYZER_HPP_

#include <string>
#include <unordered_map>
#include <vector>

namespace amr {
namespace domain {
namespace monitoring {

// Heartbeat analysis — pure domain logic, zero ROS2 dependency.
// Evaluates liveness based on elapsed time since last heartbeat.

enum class NodeStatus {
  OK,
  WARN,
  ERROR,
  STALE,   // never received data
  FATAL,   // exceeded max restart attempts
};

inline const char *to_string(NodeStatus s) {
  switch (s) {
    case NodeStatus::OK:    return "OK";
    case NodeStatus::WARN:  return "WARN";
    case NodeStatus::ERROR: return "ERROR";
    case NodeStatus::STALE: return "STALE";
    case NodeStatus::FATAL: return "FATAL";
  }
  return "UNKNOWN";
}

struct NodeHeartbeat {
  std::string node_name;
  double timeout_s   = 1.0;
  double last_seen_s = -1.0;  // seconds since last heartbeat (<0 = never)
};

class HeartbeatAnalyzer {
public:
  static constexpr double kWarnRatio = 0.8;   // WARN at 80% of timeout

  // Evaluate a single node's status from its heartbeat state.
  NodeStatus evaluate(const NodeHeartbeat &hb) const {
    if (hb.last_seen_s < 0.0) return NodeStatus::STALE;
    if (hb.last_seen_s > hb.timeout_s) return NodeStatus::ERROR;
    if (hb.last_seen_s > hb.timeout_s * kWarnRatio) return NodeStatus::WARN;
    return NodeStatus::OK;
  }

  // Summary of an entire health report.
  struct Summary {
    int ok    = 0;
    int warn  = 0;
    int error = 0;
    int stale = 0;
    int fatal = 0;
  };

  Summary summarize(const std::vector<NodeHeartbeat> &nodes) const {
    Summary s;
    for (const auto &n : nodes) {
      switch (evaluate(n)) {
        case NodeStatus::OK:    s.ok++;    break;
        case NodeStatus::WARN:  s.warn++;  break;
        case NodeStatus::ERROR: s.error++; break;
        case NodeStatus::STALE: s.stale++; break;
        case NodeStatus::FATAL: s.fatal++; break;
      }
    }
    return s;
  }
};

// Fleet-level aggregation
struct FleetSummary {
  int amr_count  = 0;
  int ok_count   = 0;
  int warn_count = 0;
  int err_count  = 0;
  int stale_count = 0;
};

inline FleetSummary fleet_summarize(
  const std::vector<HeartbeatAnalyzer::Summary> &amr_summaries) {
  FleetSummary f;
  f.amr_count = static_cast<int>(amr_summaries.size());
  for (const auto &s : amr_summaries) {
    f.ok_count   += s.ok;
    f.warn_count += s.warn;
    f.err_count  += s.error + s.fatal;
    f.stale_count += s.stale;
  }
  return f;
}

}  // namespace monitoring
}  // namespace domain
}  // namespace amr

#endif
