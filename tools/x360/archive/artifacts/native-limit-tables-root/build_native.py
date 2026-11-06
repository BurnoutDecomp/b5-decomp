"""Compile exact ARTIST float limits and constant tables; retain immutable outputs."""
from pathlib import Path
import hashlib, importlib.util, json, os, struct, subprocess, sys

ROOT = Path('D:/Reverse/BP-X360-Experimental-Worktrees/drive-thru-native-root')
HERE = Path(__file__).resolve().parent
PARENT = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
WAVE = HERE.parent / 'wave6'
OWNED = [
    'src/GameSource/Gui/BrnGuiBurnoutSkillsManager.cpp',
    'src/GameSource/Gui/Flow/HUD/Components/BrnPlayerPositionSingle.cpp',
    'vendor/renderware/src/rw/audio/core/CpuLoadBalancer.cpp',
    'src/SDKs/XGraphics/XGraphicsCompilerToSSM.cpp',
]
CALLERS = ['src/GameSource/Gui/BrnGuiModule.cpp',
           'vendor/renderware/src/rw/audio/core/Profiler.cpp']
PRIOR = ['src/GameSource/GameState/Offences/BrnDriveThruManager.cpp',
         'src/GameSource/GameState/ModeManager/Scoring/BrnScoringSystem_UpdateA.cpp',
         'src/GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring_Queries.cpp',
         'src/GameSource/GameState/TakedownManager/BrnTakedownManagerRaceCarData.cpp']
BASE = '89ff029c24c991d199dd3e5392b7663676c31ceb'

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(name, value):
    with (HERE / name).open('x', encoding='utf-8') as stream:
        json.dump(value, stream, indent=2)
def git(*args): return subprocess.check_output(['git', '-C', str(ROOT), *args])

assert git('rev-parse', 'HEAD').decode().strip() == BASE
meta = json.loads((WAVE / 'native-parallel-wave6-full-stage1-patch.json').read_text())
before = {p: sha(ROOT / p) for p in meta['source_snapshot']['hashes']}
changes = sorted(p for p, digest in before.items() if digest != meta['source_snapshot']['hashes'][p])
assert set(changes) == {*OWNED, *PRIOR}, changes
save('source-before.json', {'base': BASE, 'frozen526': meta['head'], 'hashes': before, 'changed': changes})
sys.path.insert(0, str(PARENT / 'tools/re'))
import x360rd
assert x360rd.u32(0x82815570) == 0x7c8a2378 and x360rd.u32(0x82f264a8) == 4
TABLES = [
    (OWNED[0], 'KAF_MINIMUM_HUD_MESSAGE_SKILLS_LEVEL', 0x8206f7b0, 14),
    (OWNED[1], 'KAF_CHALLENGE_WOW_LIMITS', 0x82f25680, 24),
    (OWNED[1], 'KAF_TODAYS_BEST_WOW_LIMITS', 0x82f2564c, 12),
]
original_tables = []
for source, name, address, count in TABLES:
    data = x360rd.rd(address, count * 4)
    original_tables.append({'source': source, 'name': name, 'address': hex(address), 'count': count,
                            'bytes': data.hex(), 'words': [hex(v) for v in struct.unpack('>'+str(count)+'I', data)]})
for address in (0x8206f11c, 0x8204f664): assert x360rd.u32(address) == 0x7f7fffff
exports = {}
original_dir = Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')
for filename in ('0x825118F0.json', '0x82421F78.json', '0x82B67658.json', '0x82C253F0.json'):
    path = original_dir / filename
    data = json.loads(path.read_text())
    exports[str(path)] = {'sha256': sha(path), 'name': data['name']}
fog = json.loads((original_dir / '0x82C253F0.json').read_text())['assembly'].splitlines()
assert any('0x82C25488' in line and '0x7F7F' in line for line in fog)
assert any('0x82C2548C' in line and '0xFFFF' in line for line in fog)
save('original-data.json', {'reader': x360rd.__file__, 'reader_sha256': sha(x360rd.__file__),
                           'image': x360rd.RAW, 'image_sha256': sha(x360rd.RAW),
                           'calibration_code_and_data': True, 'tables': original_tables,
                           'float_limits': {'0x8206f11c': '0x7f7fffff', '0x8204f664': '0x7f7fffff'},
                           'fog_immediate': [line for line in fog if any(a in line for a in
                                             ('0x82C25488', '0x82C2548C', '0x82C25490'))],
                           'exports': exports})
full = json.loads((WAVE / 'full-stage1/compile-results.json').read_text())
prefix = '/i' + str(PARENT / 'b5-decomp/vendor').replace('\\','/').lower() + '/'
extra = [a for a in full['results'][0]['command'] if a.replace('\\','/').lower().startswith(prefix)]
temp = HERE / 'temp'; temp.mkdir(exist_ok=False)
os.environ['TEMP'] = os.environ['TMP'] = str(temp)
output = HERE / 'native'; assert not output.exists()
argv = [str(ROOT / 'tools/build_x360.py'), '--sdk', full['sdk'], '--out', str(output),
        '--jobs', '1', '--timeout', '180', '--quiet', '--compiler-arg=/showIncludes']
argv += ['--compiler-arg=' + a for a in extra]
for source in OWNED + CALLERS: argv += ['--source', source]
save('command.json', {'argv': argv, 'cwd': str(ROOT), 'temp': str(temp),
                      'runner_sha256': sha(__file__), 'driver_sha256': sha(ROOT / 'tools/build_x360.py')})
sys.argv = argv
spec = importlib.util.spec_from_file_location('native_limit_table_driver', ROOT / 'tools/build_x360.py')
driver = importlib.util.module_from_spec(spec); spec.loader.exec_module(driver)
code = driver.main()
after = {p: sha(ROOT / p) for p in before}; assert after == before
save('source-after.json', {'hashes': after, 'unchanged_during_compile': True})
report = json.loads((output / 'compile-results.json').read_text())
headers, artifacts, matches = {}, {}, []
for row in report['results']:
    log = Path(row['log']).read_text(encoding='utf-8')
    for line in log.splitlines():
        if 'including file:' in line:
            path = Path(line.split('including file:', 1)[1].strip()).resolve()
            headers[str(path)] = sha(path)
    for key in ('object', 'assembly', 'log'):
        if Path(row[key]).is_file(): artifacts[row[key]] = sha(row[key])
    if not row['passed']:
        print('\n'.join(line for line in log.splitlines() if 'error ' in line)[:3000])
        continue
    data = Path(row['object']).read_bytes()
    machine, nsec, _, sympos, nsym, optional, _ = struct.unpack_from('<HHIIIHH', data)
    assert machine == 0x1f2
    strpos = sympos + nsym * 18
    strings = data[strpos:strpos + struct.unpack_from('<I', data, strpos)[0]]
    i = 0
    while i < nsym:
        raw, value, sec, _, _, aux = struct.unpack_from('<8sIhHBB', data, sympos + i * 18)
        name = (strings[struct.unpack_from('<I',raw,4)[0]:].split(b'\0',1)[0]
                if raw[:4] == b'\0'*4 else raw.split(b'\0',1)[0]).decode()
        for target in original_tables:
            if row['source'] == target['source'] and target['name'] in name:
                assert sec > 0
                size, pointer = struct.unpack_from('<II', data, 20 + optional + (sec-1)*40 + 16)
                length = target['count']*4; assert value + length <= size
                actual = data[pointer+value:pointer+value+length]
                assert actual.hex() == target['bytes'], (name, actual.hex(), target['bytes'])
                matches.append({'name': target['name'], 'symbol': name, 'source': row['source'],
                                'bytes': length, 'sha256': hashlib.sha256(actual).hexdigest()})
        i += 1 + aux
save('actual-inputs.json', {'headers': headers, 'header_count': len(headers), 'artifacts': artifacts,
                          'exact_original_tables': matches})
assert len(matches) == 3
print(json.dumps({'passed': report['passed'], 'failed': report['failed'], 'tables': 3,
                  'exact_original_table_bytes': sum(t['bytes'] for t in matches),
                  'headers': len(headers), 'unchanged_source_hashes': len(before)}), flush=True)
raise SystemExit(code)
