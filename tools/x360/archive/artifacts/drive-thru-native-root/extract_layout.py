"""Read the existing native probe; no compiler invocation or artifact replacement."""
from pathlib import Path
import hashlib, json, struct, sys

stage = Path(__file__).resolve().parent / sys.argv[1]
report = json.loads((stage / 'native/compile-results.json').read_text())
row = next(r for r in report['results'] if r['source'].endswith('x360_drive_thru_layout_probe.cpp'))
assert row['passed'] and row['machine'] == 0x1f2
data = Path(row['object']).read_bytes()
machine, sections, _, symbols_at, count, optional, _ = struct.unpack_from('<HHIIIHH', data)
assert machine == 0x1f2
at = symbols_at + count * 18
strings = data[at:at + struct.unpack_from('<I', data, at)[0]]
index = 0
found = []
while index < count:
    raw, value, section, _, _, aux = struct.unpack_from('<8sIhHBB', data, symbols_at + index * 18)
    name = (strings[struct.unpack_from('<I', raw, 4)[0]:].split(b'\0', 1)[0]
            if raw[:4] == b'\0' * 4 else raw.split(b'\0', 1)[0]).decode()
    if 'gNativeDriveThruLayout' in name:
        assert section > 0
        size, pointer = struct.unpack_from('<II', data, 20 + optional + (section - 1) * 40 + 16)
        assert value + 80 <= size
        found.append((name, list(struct.unpack_from('>20I', data, pointer + value))))
    index += 1 + aux
assert len(found) == 1, found
names = ['size', 'alignment', 'trigger_size', 'trigger_alignment', 'trigger_position',
         'trigger_timer', 'closed_bits', 'palette', 'box', 'drive_cache', 'id_cache',
         'car_select', 'repaired_id', 'turn_off_id', 'last_bool', 'progression',
         'action_size', 'action_junction_id', 'action_mode', 'action_auto_unlock']
values = dict(zip(names, found[0][1]))
result = {'symbol': found[0][0], 'values': values,
          'hex': {name: hex(value) for name, value in values.items()},
          'object': row['object'], 'object_sha256': hashlib.sha256(data).hexdigest(),
          'extractor_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
          'note': 'Original runner compiled successfully then failed its unmangled-symbol lookup. '
                  'This extractor reads the same immutable object using its actual C++ symbol.'}
with (stage / 'layout-extracted.json').open('x', encoding='utf-8') as stream:
    json.dump(result, stream, indent=2)
print(json.dumps(result['hex'], indent=2))
