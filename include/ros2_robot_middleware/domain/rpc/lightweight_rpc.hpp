#ifndef ROS2_ROBOT_MIDDLEWARE_DOMAIN_RPC_LIGHTWEIGHT_RPC_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DOMAIN_RPC_LIGHTWEIGHT_RPC_HPP_

/// @file   lightweight_rpc.hpp
/// @brief  轻量 RPC 核心（纯 Domain，零 ROS 依赖）——话题之上自研的请求-应答层。
///
/// 定位：补 rclcpp service 的「策略缺口」——超时/重试/幂等/错误码 service 全不带，
/// 全靠调用方自律。本组件把这些策略做成平台能力：
///
///   请求头:  magic | client_id | sequence | idem_key | deadline_ns | status | payload
///   应答头:  同结构回带 (client_id, sequence)，客户端据此配对
///   幂等:    server 端按 idem_key 去重（有界缓存）——「重试 N 次执行 1 次」
///   错误码:  Status 枚举，拒绝 bool+string 裸惯例
///
/// 传输绑定：同 OtaCoordinator 的 SlotOps 模式——Domain 只做协议与策略，
/// 底层 pub/sub 由 infra 注入（ROS2 话题 / zenoh / 进程内均可挂）。
/// 说明：rclcpp service 底层同样是「两话题 + GUID/序列号关联」，本组件
/// 即该机制的显式推导 + 策略层补全。

#include <cstdint>
#include <cstring>
#include <deque>
#include <unordered_set>
#include <optional>
#include <unordered_map>
#include <vector>

namespace amr {
namespace rpc {

inline constexpr uint16_t kMagic = 0x4152;  // "AR"

/// 错误码体系（拒绝 bool+string 惯例）
enum class Status : uint8_t {
  OK = 0,
  TIMEOUT = 1,        // 客户端判定：超过 deadline 未收到应答
  NO_ROUTE = 2,       // 无服务实例
  SERVER_ERROR = 3,   // 业务执行失败
  OVERLOADED = 4,     // 服务端背压拒绝
  BAD_REQUEST = 5,    // 请求解码/校验失败
};

/// 线上头（定长 32 字节，小端）
struct Header {
  uint16_t magic = kMagic;
  uint16_t client_id = 0;
  uint64_t sequence = 0;
  uint64_t idem_key = 0;
  int64_t deadline_ns = 0;
  uint8_t status = static_cast<uint8_t>(Status::OK);
  uint8_t reserved = 0;
  uint16_t payload_len = 0;
};
inline constexpr size_t kHeaderSize = 32;

inline void encode_header(std::vector<uint8_t> &buf, const Header &h)
{
  buf.resize(kHeaderSize + h.payload_len);
  auto put16 = [&](size_t o, uint16_t v) { std::memcpy(&buf[o], &v, 2); };
  auto put64 = [&](size_t o, uint64_t v) { std::memcpy(&buf[o], &v, 8); };
  put16(0, h.magic); put16(2, h.client_id);
  put64(4, h.sequence); put64(12, h.idem_key);
  int64_t dl = h.deadline_ns; std::memcpy(&buf[20], &dl, 8);
  buf[28] = h.status; buf[29] = h.reserved; put16(30, h.payload_len);
}

inline std::optional<Header> decode_header(const uint8_t *data, size_t size)
{
  if (size < kHeaderSize) return std::nullopt;
  Header h;
  auto get16 = [&](size_t o) { uint16_t v; std::memcpy(&v, data + o, 2); return v; };
  auto get64 = [&](size_t o) { uint64_t v; std::memcpy(&v, data + o, 8); return v; };
  h.magic = get16(0);
  if (h.magic != kMagic) return std::nullopt;
  h.client_id = get16(2);
  h.sequence = get64(4);
  h.idem_key = get64(12);
  std::memcpy(&h.deadline_ns, data + 20, 8);
  h.status = data[28]; h.reserved = data[29];
  h.payload_len = get16(30);
  if (size < kHeaderSize + h.payload_len) return std::nullopt;
  return h;
}

/// 客户端：在途调用表——关联配对 + 超时判定
class PendingCalls
{
public:
  void register_call(uint64_t sequence, int64_t deadline_ns)
  {
    pending_[sequence] = deadline_ns;
  }

  /// 应答配对：命中返回 true 并移除在途项；过期/未知序列返回 TIMEOUT 状态
  Status match(const Header & reply, int64_t now_ns)
  {
    auto it = pending_.find(reply.sequence);
    if (it == pending_.end()) return Status::TIMEOUT;  // 迟到或未知
    if (now_ns > it->second) {
      pending_.erase(it);
      return Status::TIMEOUT;
    }
    pending_.erase(it);
    return static_cast<Status>(reply.status);
  }

  /// 清扫过期项（返回过期的 sequence，供上层触发重试）
  std::vector<uint64_t> sweep_expired(int64_t now_ns)
  {
    std::vector<uint64_t> expired;
    for (auto it = pending_.begin(); it != pending_.end();) {
      if (now_ns > it->second) {
        expired.push_back(it->first);
        it = pending_.erase(it);
      } else {
        ++it;
      }
    }
    return expired;
  }

  size_t size() const { return pending_.size(); }

private:
  std::unordered_map<uint64_t, int64_t> pending_;
};

/// 重试策略：机制 vs 策略分离——框架给 hook，策略由业务声明
struct RetryPolicy {
  uint32_t max_retries = 2;
  bool should_retry(uint32_t attempts) const { return attempts <= max_retries; }
};

/// 服务端：幂等去重缓存（有界，FIFO 逐出）
class DedupCache
{
public:
  explicit DedupCache(size_t max_entries = 128) : max_entries_(max_entries) {}

  /// 首见返回 true（应执行）；重复见返回 false（应回放缓存结果，不重复执行）
  bool first_seen(uint64_t idem_key)
  {
    if (seen_.count(idem_key)) return false;
    if (order_.size() >= max_entries_) {
      seen_.erase(order_.front());
      order_.pop_front();
    }
    seen_.insert(idem_key);
    order_.push_back(idem_key);
    return true;
  }

  void cache_result(uint64_t idem_key, const std::vector<uint8_t> &payload)
  {
    results_[idem_key] = payload;  // 与 seen_ 同生命周期，容量同阶
    if (results_.size() > max_entries_ * 2) results_.clear();  // 粗逐出：极简取舍
  }

  const std::vector<uint8_t> * cached(uint64_t idem_key) const
  {
    auto it = results_.find(idem_key);
    return it == results_.end() ? nullptr : &it->second;
  }

  size_t size() const { return seen_.size(); }

private:
  size_t max_entries_;
  std::deque<uint64_t> order_;
  std::unordered_set<uint64_t> seen_;
  std::unordered_map<uint64_t, std::vector<uint8_t>> results_;
};

}  // namespace rpc
}  // namespace amr

#endif  // ROS2_ROBOT_MIDDLEWARE_DOMAIN_RPC_LIGHTWEIGHT_RPC_HPP_
