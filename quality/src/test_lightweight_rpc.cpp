// 轻量 RPC 核心单测：编解码往返 / 关联配对 / 超时 / 重试策略 / 幂等去重
#include <gtest/gtest.h>

#include "ros2_robot_middleware/domain/rpc/lightweight_rpc.hpp"

namespace drpc = amr::rpc;

namespace {
std::vector<uint8_t> make_frame(uint16_t client, uint64_t seq, uint64_t idem,
                                int64_t deadline, uint8_t status,
                                const std::vector<uint8_t> &payload = {})
{
  drpc::Header h;
  h.client_id = client; h.sequence = seq; h.idem_key = idem;
  h.deadline_ns = deadline; h.status = status;
  h.payload_len = static_cast<uint16_t>(payload.size());
  std::vector<uint8_t> buf;
  drpc::encode_header(buf, h);
  if (!payload.empty())
    buf.insert(buf.end(), payload.begin(), payload.end());
  return buf;
}
}  // namespace

TEST(LightweightRpc, CodecRoundtrip) {
  auto frame = make_frame(7, 42, 100, 123456, 0, {0x01, 0x02, 0x03});
  auto h = drpc::decode_header(frame.data(), frame.size());
  ASSERT_TRUE(h.has_value());
  EXPECT_EQ(h->magic, drpc::kMagic);
  EXPECT_EQ(h->client_id, 7u);
  EXPECT_EQ(h->sequence, 42u);
  EXPECT_EQ(h->idem_key, 100u);
  EXPECT_EQ(h->deadline_ns, 123456);
  EXPECT_EQ(h->payload_len, 3u);
}

TEST(LightweightRpc, CodecRejectsBadMagicAndTruncated) {
  auto frame = make_frame(1, 1, 1, 1, 0);
  frame[0] = 0xBB;  // 坏 magic
  EXPECT_FALSE(drpc::decode_header(frame.data(), frame.size()).has_value());
  EXPECT_FALSE(drpc::decode_header(frame.data(), 10).has_value());  // 截断
}

TEST(LightweightRpc, CorrelationMatchesAndClears) {
  drpc::PendingCalls calls;
  calls.register_call(1, 1000);
  drpc::Header reply;
  reply.sequence = 1; reply.status = static_cast<uint8_t>(drpc::Status::OK);
  EXPECT_EQ(calls.match(reply, 500), drpc::Status::OK);
  EXPECT_EQ(calls.size(), 0u);          // 配对后移除
  EXPECT_EQ(calls.match(reply, 600), drpc::Status::TIMEOUT);  // 二次到达=迟到
}

TEST(LightweightRpc, DeadlineExpirySweepsExpired) {
  drpc::PendingCalls calls;
  calls.register_call(1, 100);
  calls.register_call(2, 500);
  auto expired = calls.sweep_expired(300);   // seq1 过期，seq2 未
  ASSERT_EQ(expired.size(), 1u);
  EXPECT_EQ(expired[0], 1u);
  EXPECT_EQ(calls.size(), 1u);
}

TEST(LightweightRpc, RetryPolicyBounds) {
  drpc::RetryPolicy p;  // max_retries=2
  EXPECT_TRUE(p.should_retry(1));
  EXPECT_TRUE(p.should_retry(2));
  EXPECT_FALSE(p.should_retry(3));
}

TEST(LightweightRpc, DedupExecutesOnceAndReplaysCache) {
  drpc::DedupCache cache;
  EXPECT_TRUE(cache.first_seen(77));    // 首见：执行
  cache.cache_result(77, {0xAA});
  EXPECT_FALSE(cache.first_seen(77));   // 重试到达：不执行
  auto *cached = cache.cached(77);      // 回放缓存结果
  ASSERT_NE(cached, nullptr);
  EXPECT_EQ((*cached)[0], 0xAA);
}

TEST(LightweightRpc, DedupEvictsBounded) {
  drpc::DedupCache cache(4);
  for (uint64_t k = 1; k <= 8; ++k) EXPECT_TRUE(cache.first_seen(k));
  EXPECT_EQ(cache.size(), 4u);          // 有界：只留最近 4 个
  EXPECT_TRUE(cache.first_seen(1));     // 最早的 1 已被逐出 → 视为首见
}
