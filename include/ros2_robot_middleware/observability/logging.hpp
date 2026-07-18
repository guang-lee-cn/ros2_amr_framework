#pragma once
/// @file   logging.hpp
/// @brief  Structured logging integration — ring buffer + background worker.
///
/// Usage:
///   amr::observability::Logging::init();  // in main()
///   LOG_OBS(INFO, "fusion", "cycle complete", lat_us, obj_count);
///   // ... program exit → LogWorker drains remaining events
///
/// Hot path: ~10ns (one try_push). All serialization in background thread.

#include "log_event.hpp"
#include "log_worker.hpp"
#include "ring_buffer.hpp"
#include "tracer.hpp"

#include <memory>

namespace amr::observability {

class Logging {
public:
    static constexpr size_t kBufferSize = 4096; // power of 2

    static void init() {
        if (instance_) return; // already initialized
        instance_ = std::make_unique<Logging>();
        instance_->worker_.start();
    }

    static void shutdown() {
        if (instance_) {
            instance_->worker_.stop(); // drains remaining events
            instance_.reset();
        }
    }

    /// Push a log event from hot path. Returns false if buffer full.
    static bool push(LogEvent ev) {
        return instance_ ? instance_->buffer_.try_push(ev) : false;
    }

    Logging(const Logging &) = delete;
    Logging &operator=(const Logging &) = delete;

    // Constructor public for make_unique, but init()/shutdown() are the only
    // intended entry points. Do not construct directly.
    Logging() : worker_(buffer_) {}

private:

    static inline std::unique_ptr<Logging> instance_{nullptr};
    RingBuffer<LogEvent, kBufferSize> buffer_;
    LogWorker<kBufferSize> worker_;
};

} // namespace amr::observability

/// Convenience macro — trace_id/span_id auto-populated from TRACE_SCOPE context.
/// Usage: LOG_OBS(INFO, "fusion", "cycle done", lat_us, obj_count);
#define LOG_OBS(lvl, mod, msg, lat_us, extra)                                 \
    do {                                                                       \
        auto _ev = amr::observability::LogEvent::make(                         \
            amr::observability::LogLevel::lvl, mod, msg, lat_us, extra);       \
        auto &_ctx = amr::observability::current_trace();                      \
        _ev.trace_id = _ctx.trace_id;                                          \
        _ev.span_id  = _ctx.span_id;                                           \
        amr::observability::Logging::push(_ev);                                \
    } while (0)
