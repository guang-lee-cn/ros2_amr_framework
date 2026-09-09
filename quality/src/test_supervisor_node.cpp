/// @file test_supervisor_node.cpp — SupervisorNode 的 infra 层回归（此前为零）
///
/// 两组契约：
///   A. v2 心跳门（F2）：配了 health_nodes 的子进程「存活 ≠ 就位」，健康报告
///      ERROR 触发进程级 kill + 退避重生（域层由 test_supervisor_policy 锁定）
///   B. 进程组清场（B1）：spawn 的子进程必须自成进程组，否则 kill(-pid) 组杀
///      ESRCH，孙进程（ros2 launch 的子进程们）泄漏——2026-09-09 实测命中
///   C. 聚合语义（W5）：一个子进程映射多个 health 节点时，全部 OK 才算活；
///      WARN 不误杀——整栈 launch 子进程（NAV2 形态）的判定基础
///
/// 子进程用 `/bin/sh -c '… >> $file; sleep 60'`：每次 spawn 追加一行，行数即
/// 重生次数。单线程 executor 与 amr_supervisor 生产形态一致（回调不并发）。
#include "ros2_robot_middleware/infrastructure/qos_profiles.hpp"
#include "ros2_robot_middleware/infrastructure/supervisor_node.hpp"

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

using SteadyClock = std::chrono::steady_clock;
using namespace std::chrono_literals;

constexpr const char *kHealthTopic = "/health/report";

int count_lines(const std::string &path) {
  std::ifstream f(path);
  int n = 0;
  std::string line;
  while (std::getline(f, line)) ++n;
  return n;
}

/// 读文件里的单个 pid（文件未生成/为空 → 0）。
pid_t read_pid(const std::string &path) {
  std::ifstream f(path);
  long v = 0;
  f >> v;
  return static_cast<pid_t>(v);
}

/// 进程是否已不再运行：ESRCH（已被回收）或 /proc 状态 Z（已死、但无人 reap）。
///
/// 不能只认 ESRCH：僵尸对 kill(pid,0) 同样返回成功，而孤儿僵尸是否被回收取决于
/// PID 1 是否 wait()。CI 的 container job 用 `--entrypoint tail -f /dev/null` 起
/// 容器（runner 行为，2026-09-09 CI 日志亲验），PID 1 = tail 从不 wait → 组杀后
/// 孙进程永远是 Z。同形态容器里复现：同一段代码 WSL 1s 后 GONE、容器里 1s/3s
/// 后仍 Z（`ps` 亲验 PPID=1 STAT=Z）——CI 红是这条环境差异，不是组杀失效。
///
/// Z 恰是「组杀生效」的证据：没被杀中的话状态是 S。断言本体是「不再运行」，
/// 不是「已被回收」——回收者是谁不归 supervisor 管。
bool not_running(pid_t pid) {
  if (::kill(pid, 0) == -1 && errno == ESRCH) {
    return true;
  }
  std::ifstream f("/proc/" + std::to_string(pid) + "/stat");
  std::string line;
  if (!std::getline(f, line)) {
    return true;  // /proc 项已消失
  }
  // comm 字段可含空格：从最后一个 ')' 之后取状态字符
  std::size_t rparen = line.rfind(')');
  return rparen != std::string::npos && rparen + 2 < line.size() &&
         line[rparen + 2] == 'Z';
}

bool wait_not_running(pid_t pid, std::chrono::milliseconds timeout) {
  const auto deadline = SteadyClock::now() + timeout;
  while (SteadyClock::now() < deadline) {
    if (not_running(pid)) return true;
    std::this_thread::sleep_for(50ms);
  }
  return not_running(pid);
}

class SupervisorNodeTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() { rclcpp::init(0, nullptr); }
  static void TearDownTestSuite() { rclcpp::shutdown(); }

  void SetUp() override {
    stamp_ = std::to_string(::getpid()) + "_" +
             ::testing::UnitTest::GetInstance()->current_test_info()->name();
    pid_file_ = "/tmp/amr_sup_node_" + stamp_ + ".log";
    std::remove(pid_file_.c_str());

    harness_ = std::make_shared<rclcpp::Node>("supervisor_node_harness");
    pub_ = harness_->create_publisher<ros2_robot_middleware::msg::HealthReport>(
        kHealthTopic, amr::qos::reliable_stream());
    sub_ = harness_->create_subscription<ros2_robot_middleware::msg::HealthReport>(
        "/supervisor/report", amr::qos::latched_state(),
        [this](ros2_robot_middleware::msg::HealthReport::SharedPtr msg) {
          ++reports_;  // 报文计数：用于「重生之后的新报文」这类断言
          for (const auto &s : msg->nodes) {
            if (s.node_name == "svc") last_status_ = s.status;
          }
        });
  }

  void TearDown() override {
    if (sup_) {
      sup_->deactivate();  // teardown_children：测试绝不泄漏子进程
      sup_->cleanup();
    }
    std::remove(pid_file_.c_str());
  }

  /// 默认子进程 argv：启动时往 pid_file_ 追加一行，然后长睡。
  /// posix_spawn 不查 PATH（生产用 install 绝对路径，见 supervised_scene.launch.py）
  std::vector<std::string> default_cmd() const {
    return {"/bin/sh", "-c", "date +%s%N >> " + pid_file_ + "; sleep 60"};
  }

  /// 建 supervisor（心跳门开）+ 单个子进程 svc，配置即生效。
  void start_supervisor(const std::vector<std::string> &cmd,
                        const std::vector<std::string> &health_nodes = {"hb_node"}) {
    rclcpp::NodeOptions opts;
    opts.parameter_overrides({
        {"supervisor.children", std::vector<std::string>{"svc"}},
        {"supervisor.svc.cmd", cmd},
        {"supervisor.svc.health_nodes", health_nodes},
        {"supervisor.svc.startup_timeout_s", 10.0},
        {"supervisor.svc.backoff_base_ms", 200},
        {"supervisor.svc.backoff_max_ms", 400},
    });
    sup_ = std::make_shared<amr::infrastructure::SupervisorNode>(opts);
    sup_->configure();
    sup_->activate();

    exec_.add_node(harness_);
    exec_.add_node(sup_->get_node_base_interface());
  }

  void start_supervisor_default() { start_supervisor(default_cmd()); }

  void publish_status(const std::string &node, const std::string &status) {
    ros2_robot_middleware::msg::HealthReport msg;
    msg.header.stamp = harness_->now();
    ros2_robot_middleware::msg::HealthStatus s;
    s.node_name = node;
    s.status = status;
    s.last_seen_s = 0.0;
    s.timeout_s = 2.0;
    msg.nodes.push_back(s);
    pub_->publish(msg);
  }

  /// 以 20Hz 重发给定节点状态，直到条件成立或超时。
  bool spin_with_statuses(
      const std::vector<std::pair<std::string, std::string>> &statuses,
      const std::function<bool()> &done,
      std::chrono::milliseconds timeout = 10s) {
    // 发布节奏贴近生产（/health/report 是 1Hz），排空必须用 spin_all：
    // Executor::spin_some() 每次只执行一个回调（实测），一轮发 2 条却只取 1 条
    // → KEEP_LAST 队列饱和后新样本挤掉未取的旧样本，表现为「hb_b 永远收不到」
    // 的假红。同一窗口独立 rclpy 订阅者 50/50 收全，产品代码无辜。
    // 踩过：先误判成 DDS 排队伪影，差点放宽断言了事。
    const auto deadline = SteadyClock::now() + timeout;
    while (SteadyClock::now() < deadline) {
      for (const auto &[node, status] : statuses) publish_status(node, status);
      exec_.spin_all(20ms);
      if (done()) return true;
      std::this_thread::sleep_for(50ms);
    }
    exec_.spin_all(20ms);
    return done();
  }

  /// 单节点 hb_node 的便捷形态（既有用例沿用）。
  bool spin_with_health(const std::string &status,
                        const std::function<bool()> &done,
                        std::chrono::milliseconds timeout = 10s) {
    return spin_with_statuses({{"hb_node", status}}, done, timeout);
  }

  /// 只 spin（不发健康报告）——观察「无信号」时的默认行为。
  bool spin_until(const std::function<bool()> &done,
                  std::chrono::milliseconds timeout = 2s) {
    const auto deadline = SteadyClock::now() + timeout;
    while (SteadyClock::now() < deadline) {
      exec_.spin_once(20ms);
      if (done()) return true;
    }
    return done();
  }

  rclcpp::executors::SingleThreadedExecutor exec_;
  std::shared_ptr<rclcpp::Node> harness_;
  std::shared_ptr<amr::infrastructure::SupervisorNode> sup_;
  rclcpp::Publisher<ros2_robot_middleware::msg::HealthReport>::SharedPtr pub_;
  rclcpp::Subscription<ros2_robot_middleware::msg::HealthReport>::SharedPtr sub_;
  std::string last_status_;
  int reports_ = 0;
  std::string pid_file_;
  std::string stamp_;
};

TEST_F(SupervisorNodeTest, Given_HealthGatedChild_Then_NotRunningUntilHealthOk) {
  start_supervisor_default();
  // 进程已拉起（pid 文件有行），但健康报告未 OK → 必须停在 STARTING（WARN）
  ASSERT_TRUE(spin_until([this] { return count_lines(pid_file_) >= 1; }, 3s))
      << "子进程未被拉起";
  spin_until([] { return false; }, 1500ms);
  EXPECT_EQ(last_status_, "WARN")
      << "存活即确认（v1 行为）——心跳门未生效，挂死节点会被当成就位";

  // 健康报告 OK → 就位
  EXPECT_TRUE(spin_with_health("OK", [this] { return last_status_ == "OK"; }))
      << "健康 OK 后仍未就位（实际 " << last_status_ << "）";
}

TEST_F(SupervisorNodeTest, Given_HealthLost_Then_ChildKilledAndRespawned) {
  start_supervisor_default();
  ASSERT_TRUE(spin_with_health("OK", [this] { return last_status_ == "OK"; }))
      << "前置条件不成立：未就位，本用例无法归因";
  ASSERT_EQ(count_lines(pid_file_), 1);

  // 健康报告转 ERROR（进程仍活）→ 进程级重启：kill + 退避 + 重生
  EXPECT_TRUE(spin_with_health(
      "ERROR", [this] { return count_lines(pid_file_) >= 2; }, 15s))
      << "健康报告 ERROR 未触发重生（行数 " << count_lines(pid_file_) << "）";

  // 重生后仍需健康 OK 才回到就位（不是「活着就算好」）。必须等「重生之后
  // 发出的新报文」——last_status_ 可能是 ERROR 前的旧值，只比状态会假绿。
  const int reports_before = reports_;
  EXPECT_TRUE(spin_with_health("OK", [this, reports_before] {
    return reports_ > reports_before && last_status_ == "OK";
  })) << "重生后未能恢复就位（实际 " << last_status_ << "）";
}

TEST_F(SupervisorNodeTest, Given_SpawnedChild_Then_OwnProcessGroupAndNoGrandchildLeak) {
  // 孙进程 pid 写文件——组杀失效时它是唯一的存活证据
  const std::string gc_file = pid_file_ + ".gc";
  const std::string sh_file = pid_file_ + ".sh";
  std::remove(gc_file.c_str());
  std::remove(sh_file.c_str());
  start_supervisor({"/bin/sh", "-c",
                    "echo $$ > " + sh_file + "; sleep 60 & echo $! > " + gc_file +
                        "; wait"});

  ASSERT_TRUE(spin_until(
      [&] { return read_pid(sh_file) > 0 && read_pid(gc_file) > 0; }, 5s))
      << "子/孙进程未就绪";
  const pid_t sh_pid = read_pid(sh_file);
  const pid_t gc_pid = read_pid(gc_file);

  // 契约 1：子进程自成进程组（kill(-pid) 才有作用域）
  EXPECT_EQ(getpgid(sh_pid), sh_pid)
      << "posix_spawn 未建独立进程组——kill(-pid) 组杀会 ESRCH，孙进程泄漏";
  // 契约 1b：孙进程落在同一个组里——组杀覆盖它的前提（与 PID 1 行为无关）
  EXPECT_EQ(getpgid(gc_pid), sh_pid) << "孙进程不在子进程组内，组杀覆盖不到它";

  // 契约 2：清场后孙进程必须一起走（组杀生效的直接证据）
  sup_->deactivate();
  EXPECT_TRUE(wait_not_running(gc_pid, 3s))
      << "孙进程 " << gc_pid << " 仍在运行（组杀未覆盖 → 真泄漏）";
  std::remove(gc_file.c_str());
  std::remove(sh_file.c_str());
}

// ── 聚合语义（W5）：一个子进程映射多个 health 节点（整栈 launch 形态）─────

TEST_F(SupervisorNodeTest, Given_MultiNodeChild_Then_AliveOnlyAfterAllNodesOk) {
  start_supervisor(default_cmd(), {"hb_a", "hb_b"});
  ASSERT_TRUE(spin_until([this] { return count_lines(pid_file_) >= 1; }, 3s))
      << "子进程未被拉起";

  // 只有一个节点 OK → 不得判就位。「任一 OK 即活」是整栈形态的半死栈漏洞：
  // map_server 先 ACTIVE 就把整栈判成就位，之后没起来的节点报 STALE 被忽略。
  EXPECT_FALSE(spin_with_statuses({{"hb_a", "OK"}},
                                  [this] { return last_status_ == "OK"; }, 2s))
      << "单节点 OK 就判就位——聚合语义失效";

  EXPECT_TRUE(spin_with_statuses({{"hb_a", "OK"}, {"hb_b", "OK"}},
                                 [this] { return last_status_ == "OK"; }))
      << "全节点 OK 后仍未就位（实际 " << last_status_ << "）";
}

TEST_F(SupervisorNodeTest, Given_MultiNodeChildAndOneNodeError_Then_KilledAndRespawned) {
  start_supervisor(default_cmd(), {"hb_a", "hb_b"});
  ASSERT_TRUE(spin_with_statuses({{"hb_a", "OK"}, {"hb_b", "OK"}},
                                 [this] { return last_status_ == "OK"; }))
      << "前置条件不成立：未就位";
  ASSERT_EQ(count_lines(pid_file_), 1);

  EXPECT_TRUE(spin_with_statuses({{"hb_a", "OK"}, {"hb_b", "ERROR"}},
                                 [this] { return count_lines(pid_file_) >= 2; }, 15s))
      << "单节点 ERROR 未触发整栈重启（行数 " << count_lines(pid_file_) << "）";
}

TEST_F(SupervisorNodeTest, Given_WarnStatus_Then_NoFalseKill) {
  start_supervisor_default();
  ASSERT_TRUE(spin_with_health("OK", [this] { return last_status_ == "OK"; }));
  ASSERT_EQ(count_lines(pid_file_), 1);

  // WARN = 心跳仍在到（只是逼近超时）→ 不得按挂死处置（误杀风暴防线）
  EXPECT_FALSE(spin_with_health("WARN",
                                [this] { return count_lines(pid_file_) >= 2; }, 3s))
      << "WARN 被当挂死误杀";
  EXPECT_EQ(last_status_, "OK");
}

}  // namespace
