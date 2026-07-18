#pragma once
/// @file   log_event.hpp
/// @brief  Structured log event for ring buffer transport.
///
/// Fixed-size POD — no heap allocation on hot path.
/// Background thread serializes to JSON for stdout/file export.

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <string_view>

namespace amr::observability {

enum class LogLevel : uint8_t { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3, FATAL = 4 };

struct LogEvent {
    using clock = std::chrono::steady_clock;

    // Fixed-size fields — no dynamic allocation
    uint64_t timestamp_ns;
    uint64_t trace_id;
    uint64_t span_id;
    std::array<char, 32> module;   // e.g. "fusion", "decision"
    std::array<char, 64> message;  // fixed message text
    int64_t latency_us;            // optional: attached metric value
    int32_t extra_value;           // optional: additional context (e.g. object count)
    LogLevel level;
    uint8_t padding_[3];

    /// Convenience factory — called from real-time callback.
    /// All params must be compile-time known or already stack-allocated
    /// to avoid heap allocation inside this function.
    static LogEvent make(LogLevel lvl, std::string_view mod, std::string_view msg,
                         int64_t lat_us = 0, int32_t extra = 0) {
        LogEvent ev{};
        ev.timestamp_ns = clock::now().time_since_epoch().count();
        ev.trace_id = 0; // populated by tracer when LTTng session active
        ev.span_id = 0;
        ev.level = lvl;
        ev.latency_us = lat_us;
        ev.extra_value = extra;

        const size_t mod_len = (mod.size() < ev.module.size()) ? mod.size() : ev.module.size() - 1;
        std::copy_n(mod.data(), mod_len, ev.module.begin());
        ev.module[mod_len] = '\0';

        const size_t msg_len = (msg.size() < ev.message.size()) ? msg.size() : ev.message.size() - 1;
        std::copy_n(msg.data(), msg_len, ev.message.begin());
        ev.message[msg_len] = '\0';

        return ev;
    }
};

static_assert(sizeof(LogEvent) <= 192, "LogEvent must fit in a few cache lines");

} // namespace amr::observability
