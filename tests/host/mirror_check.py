#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Ian Guinan
"""GIO-1's screen as a mirror viewer sees it (fm1-base's own decoder, base/tools/fm1_mirror.py) against the screen
itself (the simulator's fbdump): every pixel the same.

  python3 tests/host/mirror_check.py STREAM.syx FB.raw [MIN_FRAMES]
  python3 tests/host/mirror_check.py --none STREAM.syx     (the mirror switched off: not one picture frame)
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "base" / "tools"))
import fm1_mirror  # noqa: E402

if sys.argv[1] == "--none":
    scr = fm1_mirror.Screen()
    for m in fm1_mirror.split(Path(sys.argv[2]).read_bytes()):
        scr.frame(m)
    print(f"mirror off: {scr.frames} picture frames")
    sys.exit(0 if scr.frames == 0 else 1)
stream, fb = Path(sys.argv[1]).read_bytes(), Path(sys.argv[2]).read_bytes()
need = int(sys.argv[3]) if len(sys.argv) > 3 else 1
scr = fm1_mirror.Screen()
for m in fm1_mirror.split(stream):
    scr.frame(m)
screen = [fb[2 * i] << 8 | fb[2 * i + 1] for i in range(240 * 240)]
diff = sum(1 for a, b in zip(scr.px, screen) if a != b)
print(f"{len(stream)} bytes, {scr.frames} frames ({scr.bad} bad); {diff} of {240 * 240} pixels differ")
sys.exit(0 if diff == 0 and scr.bad == 0 and scr.frames >= need else 1)
