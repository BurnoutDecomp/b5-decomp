"""Measure/fix native DriveThru using actual headers, with immutable stage outputs."""
from pathlib import Path
import hashlib, importlib.util, json, os, re, struct, subprocess, sys

ROOT = Path('D:/Reverse/BP-X360-Experimental-Worktrees/drive-thru-native-root')
HERE = Path(__file__).resolve().parent
WAVE = Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave6')
SOURCE = 'src/GameSource/GameState/Offences/BrnDriveThruManager.cpp'
PROBE = 'tests/x360_drive_thru_layout_probe.cpp'
stage = sys.argv[1]
out = HERE / stage
out.mkdir(exist_ok=False)
temp = out / 'temp'
temp.mkdir()
os.environ['TEMP'] = os.environ['TMP'] = str(temp)

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def save(name, value):
    (out / name).write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')

meta = json.loads((WAVE / 'native-parallel-wave6-full-stage1-patch.json').read_text())
head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
assert head == meta['head']
before = {p: sha(ROOT / p) for p in meta['source_snapshot']['hashes']}
changes = sorted(p for p, value in before.items() if value != meta['source_snapshot']['hashes'][p])
assert set(changes) <= {SOURCE}, changes
save('input-before.json', {'head': head, 'hashes': before, 'changed': changes,
                           'probe': PROBE, 'probe_sha256': sha(ROOT / PROBE)})
full = json.loads((WAVE / 'full-stage1/compile-results.json').read_text())
prefix = '/ic:/users/adri1/.codex/worktrees/fbaf/bp-decomp_workflow/b5-decomp/vendor/'
extra = [a for a in full['results'][0]['command'] if a.replace('\\', '/').lower().startswith(prefix)]
sources = [PROBE] if stage.startswith('layout') else [SOURCE, PROBE,
    'src/GameSource/GameState/BrnGameStateModule.cpp']
argv = [str(ROOT / 'tools/build_x360.py'), '--sdk', full['sdk'], '--out', str(out / 'native'),
        '--jobs', '1', '--timeout', '180', '--quiet', '--compiler-arg=/showIncludes']
argv += ['--compiler-arg=' + a for a in extra]
for source in sources:
    argv += ['--source', source]
save('command.json', {'argv': argv, 'cwd': str(ROOT), 'temp': str(temp),
                      'runner_sha256': sha(__file__), 'driver_sha256': sha(ROOT / 'tools/build_x360.py')})
sys.argv = argv
spec = importlib.util.spec_from_file_location('drive_native_driver', ROOT / 'tools/build_x360.py')
driver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(driver)
code = driver.main()
after = {p: sha(ROOT / p) for p in before}
assert after == before
save('input-after.json', {'head': head, 'hashes': after, 'unchanged_during_compile': True})
report = json.loads((out / 'native/compile-results.json').read_text())
headers = {}
for row in report['results']:
    log = Path(row['log']).read_text(encoding='utf-8')
    for text in log.splitlines():
        if 'including file:' in text:
            path = Path(text.split('including file:', 1)[1].strip())
            assert path.is_file(), str(path)
            headers[str(path.resolve())] = sha(path)
    if not row['passed']:
        print('\n'.join(line for line in log.splitlines() if 'error ' in line)[:5000], flush=True)
save('actual-headers.json', {'count': len(headers), 'hashes': headers})
probe = next(row for row in report['results'] if row['source'] == PROBE)
if probe['passed']:
    data = Path(probe['object']).read_bytes()
    machine, sections, _, symbols_at, symbol_count, optional_size, _ = struct.unpack_from('<HHIIIHH', data)
    assert machine == 0x1f2
    strings = symbols_at + symbol_count * 18
    table = data[strings:strings + struct.unpack_from('<I', data, strings)[0]]
    values = None
    index = 0
    while index < symbol_count:
        at = symbols_at + index * 18
        raw, value, section, _, _, auxiliary = struct.unpack_from('<8sIhHBB', data, at)
        if raw[:4] == b'\0' * 4:
            name_at = struct.unpack_from('<I', raw, 4)[0]
            name = table[name_at:].split(b'\0', 1)[0].decode()
        else:
            name = raw.split(b'\0', 1)[0].decode()
        if name.lstrip('_') == 'gNativeDriveThruLayout':
            assert section > 0
            section_at = 20 + optional_size + (section - 1) * 40
            size, pointer = struct.unpack_from('<II', data, section_at + 16)
            assert value + 80 <= size
            values = list(struct.unpack_from('>20I', data, pointer + value))
            break
        index += 1 + auxiliary
    assert values is not None
    names = ['size', 'alignment', 'trigger_size', 'trigger_alignment', 'trigger_position',
             'trigger_timer', 'closed_bits', 'palette', 'box', 'drive_cache', 'id_cache',
             'car_select', 'repaired_id', 'turn_off_id', 'last_bool', 'progression',
             'action_size', 'action_junction_id', 'action_mode', 'action_auto_unlock']
    result = dict(zip(names, values))
    save('layout.json', {'values': result, 'hex': {k: hex(v) for k, v in result.items()},
                         'object': probe['object'], 'object_sha256': sha(probe['object'])})
    print(json.dumps({k: hex(v) for k, v in result.items()}), flush=True)
save('artifact-manifest.json', {str(path.relative_to(out)): sha(path)
                               for path in out.rglob('*') if path.is_file() and 'temp' not in path.parts})
print('Tracked sources unchanged:', len(before), '; changed from frozen:', changes, flush=True)
raise SystemExit(code)
