#!/usr/bin/env python3
"""menus_pause_frames.py -- measure the WORLD behind the Driver Details pause screen in one harness run.

    py -3 b5-decomp/tests/menus_pause_frames.py <run dir> [--settle 2.0] [--tail 0.5]

Selects the dumped frames (BRN_FRAME_DUMP bb_*.bmp) written between `--settle` seconds after the harness's
PAUSE tap and `--tail` seconds before its UNPAUSE tap (both read from flow_run.console.log, anchored to marks.txt's
RUNSTART and the DRIVING mark), then over the world regions that the pause UI leaves uncovered reports:
    sat_median   median over the paused frames of the regions' mean HSV saturation (0 = greyscale)
    motion_mean  mean over consecutive paused-frame pairs of the regions' mean |luminance delta| (0 = frozen camera)
and the same two numbers for the frames written in the 10 s BEFORE the pause tap (the running game, for scale).
Prints one JSON object. Exit 2 when the run has no usable pause window.

The regions are fractions of the frame (1280x720 measured 2026-09-27): the right edge beside the stat rows and the
left edge under the tab list -- both plain world on the Driver Details screen.
"""
import argparse
import datetime
import glob
import json
import os
import re
import sys

import numpy as np
from PIL import Image

REGIONS = ((0.845, 0.25, 0.99, 0.46), (0.0, 0.46, 0.07, 0.78))


def read_text(path):
    raw = open(path, "rb").read()
    for enc in ("utf-16", "utf-8-sig", "latin-1"):
        try:
            text = raw.decode(enc)
            if "[flow]" in text or "RUNSTART" in text:
                return text
        except UnicodeDecodeError:
            continue
    return raw.decode("latin-1")


def seconds(text):
    return float(text.replace(",", "."))


def region_pixels(image, region):
    w, h = image.size
    x0, y0, x1, y1 = int(region[0] * w), int(region[1] * h), int(region[2] * w), int(region[3] * h)
    return np.asarray(image.crop((x0, y0, x1, y1)).convert("RGB")).astype(np.float64)


def frame_stats(path):
    image = Image.open(path)
    sats, lums = [], []
    for region in REGIONS:
        px = region_pixels(image, region)
        mx = px.max(axis=2)
        mn = px.min(axis=2)
        sats.append(float(np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-9), 0.0).mean()))
        lums.append(0.299 * px[..., 0] + 0.587 * px[..., 1] + 0.114 * px[..., 2])
    return float(np.mean(sats)), lums


def window(frames):
    if not frames:
        return None
    stats = [frame_stats(f) for f in frames]
    sat = float(np.median([s for s, _ in stats]))
    motions = []
    for (_, a), (_, b) in zip(stats, stats[1:]):
        motions.append(float(np.mean([np.abs(x - y).mean() for x, y in zip(a, b)])))
    return {"frames": len(frames), "first": os.path.basename(frames[0]), "last": os.path.basename(frames[-1]),
            "sat_median": sat, "motion_mean": float(np.mean(motions)) if motions else 0.0}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("run_dir")
    parser.add_argument("--settle", type=float, default=2.0)
    parser.add_argument("--tail", type=float, default=0.5)
    args = parser.parse_args()

    marks = read_text(os.path.join(args.run_dir, "flow", "marks.txt"))
    console = read_text(os.path.join(args.run_dir, "flow_run.console.log"))
    run_start = re.search(r"RUNSTART (\S+)", marks)
    driving = re.search(r"^strfin\s+([\d.,]+)s", marks, re.M) or re.search(r"^ingame\s+([\d.,]+)s", marks, re.M)
    pause = re.search(r"PAUSE tap #1 .*?at DRIVING\+([\d.,]+)s", console)
    unpause = re.search(r"UNPAUSE tap #1 .*?at DRIVING\+([\d.,]+)s", console)
    if not (run_start and driving and pause):
        print(json.dumps({"error": "no RUNSTART / DRIVING mark / PAUSE tap in this run"}))
        return 2
    stamp = run_start.group(1)
    stamp = re.sub(r"(\.\d{6})\d+", r"\1", stamp)
    t0 = datetime.datetime.fromisoformat(stamp).timestamp() + seconds(driving.group(1))
    t_pause = t0 + seconds(pause.group(1))
    t_unpause = t0 + seconds(unpause.group(1)) if unpause else float("inf")

    frames = sorted(glob.glob(os.path.join(args.run_dir, "frames", "bb_*.bmp")))
    timed = [(os.path.getmtime(f), f) for f in frames]
    paused = [f for t, f in timed if t_pause + args.settle <= t <= t_unpause - args.tail]
    before = [f for t, f in timed if t_pause - 10.0 <= t < t_pause]
    result = {"pause_window": window(paused), "before_window": window(before)}
    print(json.dumps(result))
    return 0 if result["pause_window"] else 2


if __name__ == "__main__":
    sys.exit(main())
