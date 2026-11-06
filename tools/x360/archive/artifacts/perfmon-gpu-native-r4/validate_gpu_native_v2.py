"""Bounded original/actual native GPU profiler callback, producer and state comparisons.

The real platform/registry/StrStream objects and extracted SDK __u64tod execute.
QPC/QPF, D3D predication/insertion, CRT memory/string and assert synchronization are
controlled services; callbacks are explicitly drained in selected schedules.
Unused/unwritten frame context ID halves are not compared as live arguments.
Hardware GPU callbacks, scheduling, exception/VSCR and full rendering are untested.
"""
from pathlib import Path
import hashlib
import json
import random
import struct
import time
import numpy as np
from gpu_fixture import OUT, M32, M64, Runner, make_bundle, sha

COUNTERS = [0, 1, 0xFFFFFFF0, 0x100000010, 0x1FFFFFFFF,
            0x7FFFFFFFFFFFFFF0, 0x8000000000000010, 0xFFFFFFFFFFFFFFF0]
TICKS = [0, 1, (1 << 24) - 1, (1 << 24) + 1, (1 << 32) - 1,
         (1 << 32) + 1, (1 << 53) - 1, (1 << 53) + 1,
         (1 << 63) - 1, (1 << 64) - 1]
FACTOR_BITS = [0x360637BD, 0x3380D81B, 0x3F800000, 0x44800000]


def main():
    bundle = make_bundle()
    counts, failures = {}, []
    started = time.monotonic()

    def compare(group, index, action):
        outputs = []
        for compiled in [False, True]:
            runner = Runner(bundle, compiled, index)
            returned = action(runner)
            outputs.append({'state': runner.snapshot(), 'trace': runner.trace,
                            'queued_live_contexts': [(x >> 16, x & 0xFFFF if x >> 16 >= 2 else None) for x in runner.queue],
                            'return': returned})
        if outputs[0] != outputs[1]:
            failures.append({'group': group, 'index': index, 'original': outputs[0], 'compiled': outputs[1]})
            raise AssertionError((group, index))
        counts[group] = counts.get(group, 0) + 1

    with np.errstate(all='ignore'):
        try:
            for mark_type in range(4):
                for index in range(256):
                    def callback(runner, mark_type=mark_type, index=index):
                        runner.counter = COUNTERS[index % len(COUNTERS)]
                        monitor = (index % 33) if mark_type >= 2 else ((index * 1313) & 0xFFFF)
                        runner.run('callback', (mark_type << 16) | monitor)
                    compare('callback_type%d' % mark_type, index, callback)
            for index in range(128):
                mark_type = [4, 31, 0x7FFF, 0xFFFF][index % 4]
                compare('callback_invalid_type', index, lambda r, t=mark_type: r.run('callback', (t << 16) | (r.index & 0xFFFF)))
            for name in ['Construct', 'Swap', 'StartProfiling', 'StopProfiling']:
                for index in range(128):
                    def method(runner, name=name):
                        if name == 'Swap':
                            value = [0, 1, 2, 3, 4, -1, -2, -5, -6, 0x7FFFFFFE][runner.index % 10]
                            runner.write(runner.state['values'][0] + 0xA70, value)
                        runner.run(name)
                        return (runner.cpu.g(3) & M32) if name == 'Construct' else None
                    compare(name, index, method)
            for name in ['StartMonitor', 'StopMonitor']:
                for index in range(256):
                    def monitor(runner, name=name):
                        monitor_id = [-1, 32, 0, 31, runner.index % 32][runner.index % 5]
                        if name == 'StopMonitor' and monitor_id < 0:
                            runner.put('active', 1)  # avoid dereferencing an invalid formatted-name pointer
                        runner.run(name, monitor_id)
                    compare(name, index, monitor)
            for name in ['ComputeValueFromTimer', 'GetLastMonitorValue']:
                for index in range(512):
                    def compute(runner, name=name):
                        mode = [0, 1, 2, 3, 4, 0xFFFFFFFF, 0x7FFFFFFF, 0x80000000][runner.index % 8]
                        runner.put('frequency', mode)
                        runner.put('factor', FACTOR_BITS[(runner.index // 8) % len(FACTOR_BITS)])
                        monitor = runner.index % (32 if name == 'GetLastMonitorValue' else 33)
                        cost = TICKS[runner.index % len(TICKS)] if runner.index % 2 else random.Random(runner.index).getrandbits(64)
                        read_index = runner.index % 5
                        runner.write(runner.state['values'][0] + 0xA70, read_index)
                        runner.write(runner.state['values'][0] + read_index * 0x210 + monitor * 8, cost, 8)
                        runner.run(name, monitor)
                        return '%016x' % runner.cpu.fbits[1]
                    compare(name, index, compute)
            for index in range(64):
                def composed(runner):
                    runner.run('Construct')
                    runner.counter = COUNTERS[runner.index % len(COUNTERS)]
                    for frame in range(7):
                        runner.run('StartProfiling')
                        if runner.index % 2:
                            runner.drain()
                        for offset in range(3):
                            monitor = (frame * 3 + offset + runner.index) % 32
                            runner.run('StartMonitor', monitor)
                            if runner.index % 3 == 0:
                                runner.drain()
                            runner.run('StopMonitor', monitor)
                            if runner.index % 3 == 0:
                                runner.drain()
                        runner.run('StopProfiling')
                        runner.drain()
                        runner.run('Swap')
                        runner.run('GetLastMonitorValue', (frame + runner.index) % 32)
                    return '%016x' % runner.cpu.fbits[1]
                compare('actual_producers_to_actual_callbacks_7frames', index, composed)
        except Exception as exc:
            if not failures:
                failures.append({'exception': repr(exc)})
            result = {'cases_by_group': counts, 'mismatches': failures, 'scope': __doc__, 'script_sha256': sha(__file__)}
            (OUT / 'gpu-native-comparison.json').write_text(json.dumps(result, indent=2) + '\n')
            raise
    result = {
        'cases_by_group': counts, 'total_cases': sum(counts.values()), 'mismatches': failures,
        'elapsed_seconds': time.monotonic() - started, 'scope': __doc__,
        'actual_objects': {str(o.path): sha(o.path) for o in bundle.objects},
        'actual_sdk_integer_conversion': 'libcMT.lib __u64tod object: native code byte-identical to ARTIST82C0C670; both streams execute their actual instructions, not a host cast approximation.',
        'frame_context_ID_half': 'Original and compiled producers leave the unused low16 stack halfword unwritten for marks0/1. Only live high16 is compared; actual callbacks consume full captured values and ignore low16 in those cases.',
        'callback_schedules': '64 sequences of7 frames, three monitors/frame, queued draining after selected producer boundaries; these are controlled schedules and not proof of GPU/hardware ordering.',
        'foreign_getter_limit': 'GetLastMonitorValue comparisons use valid IDs0..31. The separately preserved ID32 negative has identical state/value/assert message but pre-existing different generic-CPP file/line metadata; this unmodified foreign metadata is not claimed repaired.',
        'formatting': 'Actual native StrStream executes for valid non-null buffer capacities64/256 and seeded monitor names. Shared assertion messages, original filename bytes and line numbers are compared.',
        'script_sha256': sha(__file__), 'fixture_sha256': sha(OUT / 'gpu_fixture.py'),
    }
    (OUT / 'gpu-native-comparison.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'cases_by_group': counts, 'total': result['total_cases'], 'mismatches': len(failures), 'seconds': result['elapsed_seconds']}, indent=2), flush=True)

if __name__ == '__main__':
    main()
