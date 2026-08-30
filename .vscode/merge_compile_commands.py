#!/usr/bin/env python3
"""合并 colcon 各包的 compile_commands.json 到工作区根，供 clangd 使用。

用法: python3 merge_compile_commands.py [工作区根目录]
配合 colcon build --cmake-args -DCMAKE_EXPORT_COMPILE_COMMANDS=ON 使用（见 tasks.json）。
"""
import glob
import json
import os
import sys

ws = os.path.expanduser(sys.argv[1] if len(sys.argv) > 1 else '~/code/ros2_ws')
entries = []
for f in sorted(glob.glob(os.path.join(ws, 'build', '*', 'compile_commands.json'))):
    entries += json.load(open(f))
out = os.path.join(ws, 'compile_commands.json')
json.dump(entries, open(out, 'w'), indent=1)
print(f'merged {len(entries)} compile units -> {out}')
