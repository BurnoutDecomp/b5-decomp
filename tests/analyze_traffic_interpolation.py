"""Measure visible traffic holds from the opt-in BRN_TRAFFIC_INTERP_DIAG witness.

Only compare consecutive renders inside a moving simulation interval. Long
stationary/paused groups and intervals spanning invisible frames are excluded.
This measures submitted poses, not GPU performance or simulation correctness.
"""
import argparse
from collections import defaultdict
import json
import math
from pathlib import Path
import re

ROW = re.compile(
    r'^\[traffic-pose\] frame=(\d+) id=(\d+) physical=(\d+) crash=(\d+) '
    r'alpha=([\d.]+) ms=([\d.]+) raw=\(([^)]+)\) render=\(([^)]+)\) basis=\(([^)]+)\)')


def analyze(path):
    by_vehicle = defaultdict(list)
    text = path.read_text(encoding='utf-8', errors='replace')
    for line in text.splitlines():
        match = ROW.match(line)
        if not match:
            continue
        frame, vehicle, physical, crash, alpha, ms, raw, display, basis = match.groups()
        by_vehicle[int(vehicle)].append(dict(
            frame=int(frame), physical=int(physical), crash=int(crash),
            alpha=float(alpha), ms=float(ms), raw=tuple(map(float, raw.split(','))),
            display=tuple(map(float, display.split(','))), basis=tuple(map(float, basis.split(',')))))
    eligible = held = 0
    for rows in by_vehicle.values():
        groups = []
        for row in rows:
            if (groups and row['frame'] == groups[-1][-1]['frame'] + 1
                    and math.dist(row['raw'], groups[-1][-1]['raw']) < 1e-6):
                groups[-1].append(row)
            else:
                groups.append([row])
        for index, group in enumerate(groups):
            if (index == 0 or len(group) > 8
                    or groups[index-1][-1]['frame'] + 1 != group[0]['frame']
                    or math.dist(groups[index-1][-1]['raw'], group[0]['raw']) < 0.001):
                continue
            for previous, current in zip(group, group[1:]):
                if current['alpha'] <= previous['alpha'] or not current['physical']:
                    continue
                eligible += 1
                held += (math.dist(previous['display'], current['display']) < 1e-6
                         and math.dist(previous['basis'], current['basis']) < 1e-6)
    rows = [row for vehicle in by_vehicle.values() for row in vehicle]
    return dict(samples=len(rows), physical_samples=sum(row['physical'] for row in rows),
                fatal_samples=sum(row['crash'] for row in rows), vehicles=len(by_vehicle),
                moving_physical_between_ticks=eligible, held_render_pose=held,
                advanced_render_pose=eligible-held,
                held_percent=(100.0*held/eligible if eligible else None),
                asserts=len(re.findall(r'\[ASSERT \d+\]', text)),
                exceptions=text.count('[EXCEPTION]'))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--check', action='store_true', help='require moving samples and fewer than 1%% held poses')
    args = parser.parse_args()
    result = analyze(args.log)
    output = json.dumps(result, indent=2)
    print(output)
    if args.output:
        args.output.write_text(output + '\n', encoding='utf-8')
    if args.check:
        raise SystemExit(0 if (result['moving_physical_between_ticks'] >= 30
                              and result['held_percent'] < 1.0
                              and not result['asserts'] and not result['exceptions']) else 1)
