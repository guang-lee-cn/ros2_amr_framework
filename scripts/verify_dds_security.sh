#!/bin/bash
# verify_dds_security.sh — DDS-Security 真启用验证（§8.3 头名，2026-08-30 实证版）
#
# 四段断言（设环境变量不算启用，认证真的在挡人才算）：
#   A 认证+互通  ：合法 enclave 的 talker/listener 加密互通（收到 ≥1 条）
#   B 野节点被拒 ：无凭证进程收不到任何数据
#   C 真实栈     ：本包 scene_simulator（真节点）Enforce 下正常发布 /odom
#   D 授权拒绝   ：签了 DENY permissions 的参与者被拒之于域外（连 join 都不行）
#
# 机制要点（2026-08-30 排障结论，均为实证）：
#   1. env 三件套（KEYSTORE/ENABLE/STRATEGY=Enforce）只是开关；
#      per-node 身份必须经 ros args：`--ros-args --enclave /<name>`
#      （ROS_SECURITY_NODE_NAME 在本栈不生效——strace 证实 fqn 未拼入路径）
#   2. create_enclave 自带已签 governance + 全通行 permissions（rt/*，
#      仅授权 domain 0）——所以本脚本跑在 domain 0
#   3. create_permission 的 schema 校验在本环境解析失败（连自家模板都过
#      不了）；DENY 用 openssl CMS 手工签名等价替代（FastDDS 实测接受）
#
# 自包含：临时 keystore 建在 /tmp（用后即焚）。用法: ./scripts/verify_dds_security.sh
set -o pipefail

source /opt/ros/jazzy/setup.bash
source "$(dirname "$(readlink -f "$0")")/../../../../install/setup.bash" 2>/dev/null || \
    source ~/code/ros2_ws/install/setup.bash

KS=$(mktemp -d /tmp/amr_sec_verify.XXXX)
DOM=0   # 默认 permissions 只授权 domain 0（见头注 2）
PASS=0; FAIL=0
note() { echo "[sec-verify] $*"; }
SECENV="ROS_SECURITY_KEYSTORE=$KS ROS_SECURITY_ENABLE=true ROS_SECURITY_STRATEGY=Enforce ROS_DOMAIN_ID=$DOM"

# ── keystore + enclaves（认证材料开箱即得）─────────────────────────────
ros2 security create_keystore "$KS" >/dev/null 2>&1
for n in sec_talker sec_listener scene_simulator sec_denied; do
  ros2 security create_enclave "$KS" "/$n" >/dev/null 2>&1 || { note "⛔ enclave /$n 创建失败"; exit 1; }
done

# /sec_denied: DENY permissions（模板改写 + openssl CMS 签名，见头注 3）
python3 - "$KS" <<'EOF'
import re, sys
ks = sys.argv[1]
tpl = open(f"{ks}/enclaves/sec_denied/permissions.xml").read()
deny = re.sub(r'<publish>.*?</subscribe>', '''<publish>
            <topics>
              <topic>ros_discovery_info</topic>
            </topics>
          </publish>
          <subscribe>
            <topics>
              <topic>ros_discovery_info</topic>
            </topics>
          </subscribe>''', tpl, flags=re.S, count=1)
open(f"{ks}/perm_deny.xml", "w").write(deny)
EOF
openssl smime -sign -in "$KS/perm_deny.xml" -out "$KS/enclaves/sec_denied/permissions.p7s" \
  -signer "$KS/public/permissions_ca.cert.pem" -inkey "$KS/private/permissions_ca.key.pem" \
  -outform DER >/dev/null 2>&1

# ── 内联节点 ──────────────────────────────────────────────────────────
LISTENER=$(mktemp /tmp/sec_l.XXXX.py); TALKER=$(mktemp /tmp/sec_t.XXXX.py)
cat > "$LISTENER" <<'EOF'
import sys, time
import rclpy
from rclpy.node import Node
from std_msgs.msg import String
from nav_msgs.msg import Odometry
topic, wait_s, encl = sys.argv[1], float(sys.argv[2]), sys.argv[3]
MsgT = Odometry if topic == "/odom" else String
if encl != "-":  # "-" = 野节点（无安全环境，由调用方控制）
    rclpy.init(args=['--ros-args', '--enclave', encl])
else:
    rclpy.init()
n = Node("probe_" + str(int(time.time())))
got = [0]
n.create_subscription(MsgT, topic, lambda m: got.__setitem__(0, got[0] + 1), 10)
end = time.time() + wait_s
while time.time() < end and got[0] < 5:
    rclpy.spin_once(n, timeout_sec=0.2)
print("GOT", got[0])
rclpy.shutdown()
EOF
cat > "$TALKER" <<'EOF'
import time
import rclpy
from rclpy.node import Node
from std_msgs.msg import String
rclpy.init(args=['--ros-args', '--enclave', '/sec_talker'])
n = Node("sec_talker")
p = n.create_publisher(String, "sec_check", 10)
end = time.time() + 40
while time.time() < end:
    p.publish(String(data="authenticated"))
    time.sleep(0.1)
EOF

note "talker 起动（Enforce + enclave /sec_talker）"
env $SECENV python3 "$TALKER" >/dev/null 2>&1 &
TPID=$!
sleep 6  # 认证握手

note "A: 合法 listener 应收到 ≥1"
A=$(timeout 15 env $SECENV python3 "$LISTENER" sec_check 10 /sec_listener 2>/dev/null | grep -oE '[0-9]+$')
[ "${A:-0}" -ge 1 ] && { note "✅ A 认证互通（$A 条）"; PASS=$((PASS+1)); } || { note "❌ A 失败"; FAIL=$((FAIL+1)); }

note "B: 无凭证 listener 应收到 0"
B=$(ROS_DOMAIN_ID=$DOM timeout 15 python3 "$LISTENER" sec_check 10 - 2>/dev/null | grep -oE '[0-9]+$')
[ "${B:-0}" -eq 0 ] && { note "✅ B 野节点被拒（0 条）"; PASS=$((PASS+1)); } || { note "❌ B 失败（收到 $B！）"; FAIL=$((FAIL+1)); }

note "D: DENY permissions 参与者应无法入域"
DLOG=$(timeout 15 env $SECENV python3 "$LISTENER" sec_check 8 /sec_denied 2>&1)
echo "$DLOG" | grep -q "not allowed" && { note "✅ D 授权拒绝（participant not allowed）"; PASS=$((PASS+1)); } \
  || { echo "$DLOG" | grep -q "^GOT" && { note "❌ D 失败（DENY 方收到了数据！）"; FAIL=$((FAIL+1)); } \
       || { note "❌ D 异常（未见拒绝标识也未见数据）"; FAIL=$((FAIL+1)); }; }

kill $TPID 2>/dev/null

# ── C: 真实节点在 Enforce 下发布 ──────────────────────────────────────
note "C: scene_simulator（真实节点）Enforce 下发布 /odom"
env $SECENV ros2 run ros2_robot_middleware scene_simulator \
    --ros-args --enclave /scene_simulator >/dev/null 2>&1 &
SPID=$!
sleep 8
C=$(timeout 15 env $SECENV python3 "$LISTENER" /odom 10 /sec_listener 2>/dev/null | grep -oE '[0-9]+$')
[ "${C:-0}" -ge 1 ] && { note "✅ C 真实栈互通（/odom $C 条）"; PASS=$((PASS+1)); } || { note "❌ C 失败"; FAIL=$((FAIL+1)); }
kill $SPID 2>/dev/null; sleep 2; kill -9 $SPID 2>/dev/null

rm -rf "$KS" "$LISTENER" "$TALKER"
note "──────── 结果: PASS=$PASS FAIL=$FAIL（A 认证互通 / B 野节点拒 / C 真实栈 / D 授权拒）────────"
[ "$FAIL" -eq 0 ] || exit 1
exit 0
