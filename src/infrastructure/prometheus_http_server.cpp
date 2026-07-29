#include "ros2_robot_middleware/infrastructure/prometheus_http_server.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>

bool PrometheusHttpServer::start() {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return false;

  int opt = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port_);

  if (bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
    close(fd);
    return false;
  }

  if (listen(fd, 5) < 0) {
    close(fd);
    return false;
  }

  socket_fd_ = fd;
  running_.store(true, std::memory_order_release);
  thread_ = std::thread([this]() { accept_loop(); });
  return true;
}

void PrometheusHttpServer::stop() {
  running_.store(false, std::memory_order_release);

  if (socket_fd_ >= 0) {
    ::shutdown(socket_fd_, SHUT_RDWR);
    close(socket_fd_);
    socket_fd_ = -1;
  }
  if (thread_.joinable()) {
    thread_.join();
  }
}

void PrometheusHttpServer::accept_loop() {
  while (running_.load(std::memory_order_acquire)) {
    sockaddr_in client{};
    socklen_t len = sizeof(client);
    int conn = accept(socket_fd_, reinterpret_cast<sockaddr *>(&client), &len);
    if (conn < 0) continue;

    char buf[1024]{};
    ssize_t n = recv(conn, buf, sizeof(buf) - 1, 0);
    std::string body;

    if (n > 0) {
      std::string request(buf, n);
      if (request.find("GET /metrics") != std::string::npos) {
        std::string metrics = provider_();
        body = "HTTP/1.1 200 OK\r\n"
               "Content-Type: text/plain; version=0.0.4\r\n"
               "Content-Length: " + std::to_string(metrics.size()) + "\r\n"
               "Connection: close\r\n"
               "\r\n" + metrics;
      } else {
        body = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
      }
    }

    send(conn, body.data(), body.size(), MSG_NOSIGNAL);
    close(conn);
  }
}
