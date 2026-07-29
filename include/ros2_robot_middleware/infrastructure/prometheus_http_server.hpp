#ifndef ROS2_ROBOT_MIDDLEWARE_INFRA_PROMETHEUS_HTTP_SERVER_HPP_
#define ROS2_ROBOT_MIDDLEWARE_INFRA_PROMETHEUS_HTTP_SERVER_HPP_

#include <atomic>
#include <functional>
#include <string>
#include <thread>

// Standalone Prometheus metrics HTTP server.
// Zero ROS2 dependency — only POSIX sockets + std::thread.
//
// Usage:
//   PrometheusHttpServer server(9090, []{ return "# metric\nvalue 1\n"; });
//   server.start();
//   // ... application runs ...
//   server.stop();

class PrometheusHttpServer {
public:
  using MetricsProvider = std::function<std::string()>;

  PrometheusHttpServer(uint16_t port, MetricsProvider provider)
    : port_(port), provider_(std::move(provider)) {}

  ~PrometheusHttpServer() { stop(); }

  PrometheusHttpServer(const PrometheusHttpServer &) = delete;
  PrometheusHttpServer &operator=(const PrometheusHttpServer &) = delete;

  bool start();
  void stop();

private:
  void accept_loop();

  uint16_t port_;
  MetricsProvider provider_;
  int socket_fd_ = -1;
  std::thread thread_;
  std::atomic<bool> running_{false};
};

#endif
