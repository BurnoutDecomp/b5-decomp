"""Check actual checksum-validated saves for score and progression retention.

This reads artifacts only. It never creates a score, medal, profile, or fixture.
Native cases must separately witness the producer, street plate, and Driver Details.
"""
from pathlib import Path
import argparse
import json
import struct
import sys

sys.dont_write_bytecode = True
workflow = next((p for p in Path(__file__).resolve().parents
                 if (p / 'tools/saves/convert_xenia_save.py').is_file()), None)
if workflow is None:
    raise SystemExit('Run from a b5 checkout within BP-Decomp_Workflow.')
sys.path.insert(0, str(workflow / 'tools/saves'))
from convert_xenia_save import read_container


def read(path):
    image, _ = read_container(Path(path).read_bytes())
    if len(image) < 118064 or struct.unpack_from('<I', image)[0] != 28:
        raise ValueError('Expected a version28 progression image')
    scores = [struct.unpack_from('<QQiiQQ', image, 100040 + i * 40) for i in range(64)]
    return image, scores


def retained(before, after):
    old, old_scores = read(before)
    new, new_scores = read(after)
    lost, changed = [], []
    for slot, (a, b) in enumerate(zip(old_scores, new_scores)):
        for bit, field, value_index, car_index in ((1, 'time', 2, 4), (2, 'showtime', 3, 5)):
            was, now = bool(a[1] & bit), bool(b[1] & bit)
            if was and (not now or (b[value_index] > a[value_index] if bit == 1 else b[value_index] < a[value_index])):
                lost.append({'slot': slot, 'type': field, 'before': a[value_index], 'after': b[value_index] if now else None})
            if was and now and b[value_index] == a[value_index] and b[car_index] != a[car_index]:
                lost.append({'slot': slot, 'type': field, 'reason': 'unchanged score lost its car id'})
            if now and (not was or b[value_index] != a[value_index]):
                changed.append({'slot': slot, 'type': field, 'score': b[value_index], 'car': f'{b[car_index]:016X}'})

    progression = []
    for name, count_at, data_at, stride, cap in (
        ('cars', 604, 624, 24, 512), ('liveries', 608, 12912, 24, 512),
        ('rivals', 612, 25200, 56, 64), ('events', 616, 28784, 8, 175)):
        old_count = struct.unpack_from('<i', old, count_at)[0]
        new_count = struct.unpack_from('<i', new, count_at)[0]
        if not (0 <= old_count <= cap and 0 <= new_count <= cap):
            raise ValueError(f'Invalid {name} record count')
        fmt = '<I' if name == 'events' else '<Q'
        old_ids = {struct.unpack_from(fmt, old, data_at + i * stride)[0] for i in range(old_count)}
        new_ids = {struct.unpack_from(fmt, new, data_at + i * stride)[0] for i in range(new_count)}
        missing = old_ids - new_ids
        progression.append({'kind': name, 'before': old_count, 'after': new_count, 'missing_ids': len(missing)})
        if missing:
            lost.append({'progression': name, 'missing_ids': sorted(missing)})
    for name, start, length in (('hit props', 58920, 37504), ('seen training', 117936, 32)):
        dropped = sum((a & b) != a for a, b in zip(old[start:start + length], new[start:start + length]))
        progression.append({'kind': name, 'bytes_with_lost_bits': dropped})
        if dropped:
            lost.append({'progression': name, 'bytes_with_lost_bits': dropped})
    for name, offset in (('medals', 42496), ('takedowns', 392)):
        a = struct.unpack_from('<I', old, offset)[0]
        b = struct.unpack_from('<I', new, offset)[0]
        progression.append({'kind': name, 'before': a, 'after': b})
        if b < a:
            lost.append({'progression': name, 'before': a, 'after': b})
    return {'pass': not lost, 'before_scores': sum(bool(a[1] & bit) for a in old_scores for bit in (1, 2)),
            'after_scores': sum(bool(a[1] & bit) for a in new_scores for bit in (1, 2)),
            'reset_epoch_before': struct.unpack_from('<I', old, 102600)[0],
            'reset_epoch_after': struct.unpack_from('<I', new, 102600)[0],
            'changed': changed, 'lost': lost, 'progression': progression}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('before')
    parser.add_argument('after')
    parser.add_argument('--require-new-time-win', type=int, metavar='SLOT')
    parser.add_argument('--time-par-ms', type=int, default=4699)
    parser.add_argument('--keep-reset-epoch', action='store_true',
                        help='Require missing-service-config sessions to preserve the stored epoch.')
    args = parser.parse_args()
    result = retained(args.before, args.after)
    if args.keep_reset_epoch and result['reset_epoch_before'] != result['reset_epoch_after']:
        result['lost'].append({'reason': 'stored reset epoch changed without reset authority'})
        result['pass'] = False
    if args.require_new_time_win is not None:
        won = any(row['slot'] == args.require_new_time_win and row['type'] == 'time'
                  and 0 < row['score'] < args.time_par_ms for row in result['changed'])
        result['new_time_win'] = won
        result['pass'] &= won
    print(json.dumps(result, indent=2))
    return 0 if result['pass'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
