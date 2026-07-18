# Per-node health snapshot
string node_name
string status        # OK | WARN | ERROR | STALE
float64 last_seen_s  # seconds since last activity
float64 timeout_s    # configured timeout for this node
