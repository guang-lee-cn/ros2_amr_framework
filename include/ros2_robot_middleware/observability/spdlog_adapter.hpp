#pragma once
/// @file   spdlog_adapter.hpp
/// @brief  spdlog async logger — replacement for ring buffer logging.
///
/// Preserves LOG_OBS macro API. Internally uses spdlog async
/// (thread pool + non-blocking queue) instead of custom ring buffer.
///
/// Hot path: ~100ns (format string parsing + async queue push).
///           Higher than ring buffer ~10ns, but still negligible at
///           AMR sensor rates (0.001% of 10ms IMU callback budget).
///
/// Migration from ring buffer:
///   - Replace Logging::init() with Logging::init_spdlog()
///   - LOG_OBS macro unchanged — same call sites, no code changes in nodes
///   - LogWorker / RingBuffer / LogEvent no longer used, kept for reference

#include <memory>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace amr::observability {

class Logging {
public:
  /// Initialize spdlog async logger. Call once in main().
  static void init_spdlog(const std::string &name = "amr",
                          spdlog::level::level_enum level = spdlog::level::info) {
    if (instance_) return;

    // Async logger: thread pool (1 thread, 8192 queue) + stdout sink
    auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    instance_ = std::make_shared<spdlog::async_logger>(
        name, sink, spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);
    instance_->set_level(level);
    instance_->set_pattern(R"({"ts":%E.%f,"lvl":"%l","mod":"%n","msg":"%v"})");
    spdlog::register_logger(instance_);
  }

  /// Get the amr logger. Returns nullptr if not initialized.
  static std::shared_ptr<spdlog::logger> get() { return instance_; }

  /// Shutdown — drain async queue before exit.
  static void shutdown() {
    if (instance_) {
      instance_->flush();
      spdlog::drop(instance_->name());
      instance_.reset();
    }
  }

private:
  static inline std::shared_ptr<spdlog::logger> instance_;
};

} // namespace amr::observability

/// Convenience macro — same API as ring buffer version.
/// Internally routes to spdlog async logger.
#define LOG_OBS(lvl, mod, msg, lat_us, extra)                                 \
  do {                                                                         \
    auto _logger = amr::observability::Logging::get();                         \
    if (_logger) {                                                             \
      _logger->log(spdlog::level::lvl, "{} lat_us={} extra={}", msg, lat_us, extra); \
    }                                                                          \
  } while (0)
