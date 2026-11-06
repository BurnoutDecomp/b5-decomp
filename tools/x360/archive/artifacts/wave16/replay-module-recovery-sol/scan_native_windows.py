import bisect
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0, 'D:/Reverse/BP-Decomp_Workflow/tools/re')
import findinit
import x360rd

assert x360rd.u32(0x82815570) == 0x7c8a2378
assert x360rd.u32(0x82F264A8) == 4
OUT = Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave16/replay-module-recovery-sol')
EXPORTS = Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')
functions = []
for path in EXPORTS.glob('*.json'):
    data = json.loads(path.read_text(encoding='utf-8'))
    assembly = data.get('assembly') or ''
    rows = assembly.splitlines()
    start = int(data['address'], 16)
    end = int(rows[-1].split()[0], 16) + 4 if rows and rows[-1].startswith('0x') else start + 4
    functions.append((start, end, data['name']))
functions.sort()
starts = [f[0] for f in functions]


def owner(address):
    i = bisect.bisect_right(starts, address) - 1
    return functions[i][2] if i >= 0 and functions[i][0] <= address < functions[i][1] else None


ranges = [(0x4930, 0x4940), (0x5b80, 0x5c30), (0x5c33, 0x5c38)]
hits = []
outer_hits = []
for start, end, cumulative in findinit._segments():
    blob = findinit._bulk(start, end, cumulative)
    words = struct.unpack('>' + 'I' * (len(blob) // 4), blob[:len(blob) & ~3])
    for i, word in enumerate(words):
        op = word >> 26
        if op not in findinit._DFORM and op not in findinit._DSFORM:
            continue
        immediate = word & (0xfffc if op in findinit._DSFORM else 0xffff)
        if any(low <= immediate < high for low, high in ranges):
            address = start + i * 4
            hits.append({'address': hex(address), 'word': hex(word), 'op': op,
                         'immediate': hex(immediate), 'base_register': (word >> 16) & 31,
                         'register': (word >> 21) & 31, 'owner': owner(address)})
        # Game Replay starts at +8BD300. The three native module intervals
        # therefore also have candidate address forms based on Game+8C0000.
        if any((0x8bd300 + low) & 0xffff <= immediate < (0x8bd300 + high) & 0xffff
               for low, high in ranges):
            address = start + i * 4
            outer_hits.append({'address': hex(address), 'word': hex(word), 'op': op,
                               'immediate': hex(immediate), 'base_register': (word >> 16) & 31,
                               'register': (word >> 21) & 31, 'owner': owner(address)})
result = {'sentinels_passed': True, 'native_windows': [[hex(a), hex(b)] for a, b in ranges],
          'direct_immediate_candidates': hits, 'outer_game_low_immediate_candidates': outer_hits,
          'limits': 'Candidate filter only; register provenance and context must be classified. No field role inferred from an immediate.'}
(OUT / 'RAW_WINDOW_CANDIDATES.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print('Direct candidates:', len(hits), 'outer Game candidates:', len(outer_hits))
for row in hits:
    print(row)
