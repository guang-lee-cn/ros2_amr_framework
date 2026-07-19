# ── Convenience targets ──────────────────────────────────────────────
# CI uses quality/quality.sh directly, not these targets.

.PHONY: build test quality clean

build:
	cd .. && colcon build --packages-select ros2_robot_middleware

test:
	../quality/quality.sh

quality: test

clean:
	cd .. && rm -rf build/ros2_robot_middleware install/ros2_robot_middleware
