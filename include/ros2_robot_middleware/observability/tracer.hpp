#pragma once
/// @file   tracer.hpp
/// @brief  Lightweight trace context — generates trace_id, feeds LOG_OBS.
///
/// Hot path: TracerContext stores trace_id/span_id in thread_local storage.
/// TRACE_SCOPE pushes a span on entry, pops on exit.
/// LOG_OBS reads current trace_id/span_id from TracerContext.
///
/// No LTTng dependency for trace_id generation — uses atomic counter
/// for trace_id and per-thread span counter. LTTng tracepoints can be
/// added independently for kernel-level flame graphs.

#include <atomic>
#include <cstdint>

namespace amr::observability {

/// Thread-local trace context — zero allocation, zero lock
struct TraceContext {
    uint64_t trace_id = 0;
    uint64_t span_id  = 0;
    uint64_t parent_span_id = 0;
};

inline TraceContext &current_trace() {
    thread_local TraceContext ctx;
    return ctx;
}

/// Global trace_id counter — atomic, monotonic
inline uint64_t next_trace_id() {
    static std::atomic<uint64_t> counter{1};
    return counter.fetch_add(1, std::memory_order_relaxed);
}

/// RAII span — pushes trace context on entry, restores on exit.
class ScopedSpan {
public:
    explicit ScopedSpan(const char *name);

    ~ScopedSpan();

    ScopedSpan(const ScopedSpan &) = delete;
    ScopedSpan &operator=(const ScopedSpan &) = delete;

private:
    TraceContext saved_;
};

inline ScopedSpan::ScopedSpan(const char * /*name*/) : saved_(current_trace()) {
    auto &ctx = current_trace();
    if (ctx.trace_id == 0) {
        ctx.trace_id = next_trace_id();       // root span — start new trace
        ctx.span_id  = 1;
    } else {
        // Child span — increment span counter
        static thread_local uint64_t span_counter = 1;
        ctx.parent_span_id = ctx.span_id;
        ctx.span_id = ++span_counter;
    }
}

inline ScopedSpan::~ScopedSpan() {
    current_trace() = saved_;   // restore parent context
}

/// Emit a named event within the current trace. Trace context is auto-injected.
inline void trace_event(const char * /*name*/) {
    // LTTng tracepoint placeholder — trace_id available from current_trace()
}

} // namespace amr::observability

// ── Convenience macros ───────────────────────────────────────────────

#ifdef AMR_TRACING_ENABLED
#define TRACE_SCOPE(name) amr::observability::ScopedSpan _trace_span(name)
#define TRACE_EVENT(name) amr::observability::trace_event(name)
#else
#define TRACE_SCOPE(name) ((void)0)
#define TRACE_EVENT(name) ((void)0)
#endif
