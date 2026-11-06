from pathlib import Path
import hashlib, json, random, struct, sys

HERE = Path(__file__).resolve().parent
PARENT = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(PARENT / 'scratch/x360_sdk'))
mkdir = Path.mkdir
Path.mkdir = lambda *a, **k: None
try:
    import native_atomic_fixture as F
finally:
    Path.mkdir = mkdir

def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

assert F.M.V.x360rd.u32(0x82815570) == 0x7C8A2378
assert F.M.V.x360rd.u32(0x82F264A8) == 4
report = json.loads((HERE / 'native/compile-results.json').read_text())
paths = {Path(row['source']).name: Path(row['object']) for row in report['results'] if row['passed']}
ids = paths['job_thread_id.cpp']
rng = random.Random(0x829158F8)
values = [0, 1, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 0xF8000088,
          0xFFFFFFFF00000000, 0x12345678F8000088] + [rng.getrandbits(64) for _ in range(504)]
groups, errors = {}, []
for name, original, prefix in (
    ('convert', 0x82915920, '?ToJobThreadId@Jobs@EA@@'),
    ('getter', 0x829158F8, '?GetJobThreadId@Jobs@EA@@')):
    obj = F.Coff(ids, prefix)
    for index, value in enumerate(values):
        pair = []
        for compiled in (False, True):
            cpu = F.cpu(obj)
            cpu.setg(3, value)
            calls = []
            def get_thread(c):
                calls.append('GetThreadId')
                c.setg(3, value)
            cpu.hooks[0x82B42560] = get_thread
            for addr, symbol in obj.unresolved.items():
                if symbol.startswith('?GetThreadId@Thread@EA@@'):
                    cpu.hooks[addr] = get_thread
            cpu.run(obj.entry if compiled else original)
            pair.append((cpu.g(3), calls))
        expected = (value & 0xFFFFFFFF, ['GetThreadId'] if name == 'getter' else [])
        if pair[0] != expected or pair[1] != expected:
            errors.append({'kind': name, 'case': index, 'input': hex(value),
                           'original': pair[0], 'compiled': pair[1], 'expected': expected})
    groups[name] = len(values)

new = F.Coff(paths['PolygonSoupTesterJob.cpp'], '?AllocateMemory@PolygonSoupTesterJob@@')
old_report = HERE.parent / 'wave7/full-successor1/compile-results.json'
old_row = next(row for row in json.loads(old_report.read_text())['results']
               if row['source'].endswith('/PolygonSoupTesterJob.cpp'))
assert old_row['passed']
old = F.Coff(Path(old_row['object']), '?AllocateMemory@PolygonSoupTesterJob@@')
SELF = 0x45000000
old_failures = []
for index in range(512):
    alignment = 1 << (index % 8)
    size = rng.randrange(0, 4097)
    cursor = rng.randrange(0, 0x17000)
    initial = bytearray(rng.randbytes(0x19100))
    struct.pack_into('>i', initial, 0x19020, cursor)
    # Seed the old incorrect cursor only to isolate its arena/field-address defect.
    # Both the native extent and guard region are compared without normalizing writes.
    struct.pack_into('>i', initial, 0x19080, cursor)
    pair = []
    for version in ('original', 'new', 'old'):
        obj = old if version == 'old' else new
        cpu = F.cpu(obj)
        cpu.mem.add(SELF, len(initial), initial)
        cpu.setg(3, SELF)
        cpu.setg(4, size)
        cpu.setg(5, alignment)
        cpu.run(0x82916B98 if version == 'original' else obj.entry)
        pair.append((cpu.g(3) & 0xFFFFFFFF, cpu.mem.rd(SELF, len(initial))))
    if pair[0] != pair[1]:
        errors.append({'kind': 'arena', 'case': index, 'cursor': cursor,
                       'size': size, 'alignment': alignment,
                       'returns': [hex(x[0]) for x in pair[:2]],
                       'different_bytes': [i for i, (a, b) in enumerate(zip(pair[0][1], pair[1][1])) if a != b]})
    if pair[0] != pair[2]:
        old_failures.append(index)
groups['arena_in_bound'] = 512
assert len(old_failures) == 512
raw = {'0x829158F8': F.M.V.x360rd.rd(0x829158F8, 36).hex(),
       '0x82915920': F.M.V.x360rd.rd(0x82915920, 16).hex()}
result = {'cases': sum(groups.values()), 'groups': groups, 'mismatches': errors,
          'objects': {str(ids): sha(ids), str(new.path): sha(new.path)},
          'old_negative': {'object': str(old.path), 'sha256': sha(old.path),
                           'failed_cases': old_failures, 'total': 512},
          'original_export_hole_code': raw,
          'original_allocator_export': 'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82916B98.json',
          'original_allocator_export_sha256': sha('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82916B98.json'),
          'verifier_sha256': sha(__file__),
          'limits': 'Actual original versus compiled PPC. Thread identity is a controlled service; conversion checks include arbitrary upper GPR bits and all low32 output bits. Arena tests stay below the existing overflow assertion, compare every native context and guard byte, and retain the old wrong-member-offset object as a negative. Global array storage uses the independently observed64-byte property; original type/member alignment remains unknown. No whole collision job, real threading, hardware, full link or launch claim.'}
with (HERE / 'job-id-arena-comparison.json').open('x', encoding='utf-8') as f:
    json.dump(result, f, indent=2)
print(json.dumps({'cases': result['cases'], 'groups': groups,
                  'mismatches': len(errors), 'old_arena_negative': len(old_failures)}, indent=2))
assert not errors
