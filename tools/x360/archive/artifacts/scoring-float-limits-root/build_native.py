"""Compile the three exact ARTIST float-limit spellings and two real callers."""
from pathlib import Path
import hashlib, importlib.util, json, os, re, struct, subprocess, sys

ROOT = Path('D:/Reverse/BP-X360-Experimental-Worktrees/drive-thru-native-root')
HERE = Path(__file__).resolve().parent
WAVE = HERE.parent / 'wave6'
PARENT = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
OWNED = [
    'src/GameSource/GameState/ModeManager/Scoring/BrnScoringSystem_UpdateA.cpp',
    'src/GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring_Queries.cpp',
    'src/GameSource/GameState/TakedownManager/BrnTakedownManagerRaceCarData.cpp',
]
CALLERS = ['src/GameSource/GameState/ModeManager/Scoring/BrnScoringSystem_UpdateB.cpp',
           'src/GameSource/GameState/TakedownManager/BrnTakedownManager.cpp']
DRIVE = 'src/GameSource/GameState/Offences/BrnDriveThruManager.cpp'
BASE = '22758494dfdb2ba0d8f46c013e7c36d136fa6921'

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def save(name, value):
    with (HERE / name).open('x', encoding='utf-8') as stream:
        json.dump(value, stream, indent=2)

def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])

assert git('rev-parse', 'HEAD').decode().strip() == BASE
meta = json.loads((WAVE / 'native-parallel-wave6-full-stage1-patch.json').read_text())
before = {p: sha(ROOT / p) for p in meta['source_snapshot']['hashes']}
changes = sorted(p for p, digest in before.items() if digest != meta['source_snapshot']['hashes'][p])
assert set(changes) == {*OWNED, DRIVE}
assert sha(ROOT / DRIVE) == json.loads((HERE.parent / 'drive-thru-native-root/DELIVERY.json').read_text())['source_sha256']
save('source-before.json', {'base': BASE, 'frozen526': meta['head'], 'hashes': before, 'changed': changes})
sys.path.insert(0, str(PARENT / 'tools/re'))
import x360rd
assert x360rd.u32(0x82815570) == 0x7c8a2378 and x360rd.u32(0x82f264a8) == 4
constants = []
for address, filename in [(0x82cdb7d0, '0x8232A668.json'),
                          (0x82020afc, '0x823211E8.json'),
                          (0x82029b70, '0x82355CE8.json')]:
    assert x360rd.u32(address) == 0x7f7fffff
    original = Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX') / filename
    data = json.loads(original.read_text())
    constants.append({'address': hex(address), 'bits': hex(x360rd.u32(address)),
                      'value': x360rd.f32(address), 'function': data['name'],
                      'export': str(original), 'export_sha256': sha(original),
                      'loads': [line for line in data['assembly'].splitlines()
                                if format(address, '08X') in line]})
save('original-constants.json', {'reader': x360rd.__file__, 'reader_sha256': sha(x360rd.__file__),
                               'image': x360rd.RAW, 'image_sha256': sha(x360rd.RAW),
                               'code_and_data_calibration_passed': True, 'constants': constants})
full = json.loads((WAVE / 'full-stage1/compile-results.json').read_text())
prefix = '/i' + (str(PARENT / 'b5-decomp/vendor').replace('\\', '/').lower()) + '/'
extra = [a for a in full['results'][0]['command'] if a.replace('\\', '/').lower().startswith(prefix)]
temp = HERE / 'temp'
temp.mkdir(exist_ok=False)
os.environ['TEMP'] = os.environ['TMP'] = str(temp)
output = HERE / 'native'
assert not output.exists()
argv = [str(ROOT / 'tools/build_x360.py'), '--sdk', full['sdk'], '--out', str(output),
        '--jobs', '1', '--timeout', '180', '--quiet', '--compiler-arg=/showIncludes']
argv += ['--compiler-arg=' + flag for flag in extra]
for source in OWNED + CALLERS:
    argv += ['--source', source]
save('command.json', {'argv': argv, 'cwd': str(ROOT), 'temp': str(temp),
                      'runner_sha256': sha(__file__), 'driver_sha256': sha(ROOT / 'tools/build_x360.py')})
sys.argv = argv
spec = importlib.util.spec_from_file_location('float_limit_native_driver', ROOT / 'tools/build_x360.py')
driver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(driver)
code = driver.main()
after = {p: sha(ROOT / p) for p in before}
assert after == before
save('source-after.json', {'hashes': after, 'unchanged_during_compile': True})
report = json.loads((output / 'compile-results.json').read_text())
assert report['state'] == 'complete' and report['requested'] == 5
headers, artifacts, emitted = {}, {}, {}
for row in report['results']:
    log = Path(row['log']).read_text(encoding='utf-8')
    for line in log.splitlines():
        if 'including file:' in line:
            path = Path(line.split('including file:', 1)[1].strip()).resolve()
            headers[str(path)] = sha(path)
    for kind in ('log', 'object', 'assembly'):
        path = Path(row[kind])
        if path.is_file():
            artifacts[str(path)] = sha(path)
    if row['passed']:
        assert row['machine'] == 0x1f2 and struct.unpack_from('<H', Path(row['object']).read_bytes())[0] == 0x1f2
        if row['source'] in OWNED:
            asm = Path(row['assembly']).read_text(encoding='utf-8', errors='replace')
            lines = [line for line in asm.splitlines() if re.search('7[fF]7[fF]{5}', line)]
            assert lines, row['source']
            emitted[row['source']] = lines
    else:
        print('\n'.join(line for line in log.splitlines() if 'error ' in line)[:3000])
save('actual-inputs.json', {'headers': headers, 'header_count': len(headers), 'artifacts': artifacts,
                          'emitted_exact_float_limit': emitted})
print(json.dumps({'passed': report['passed'], 'failed': report['failed'],
                  'headers': len(headers), 'source_hashes_unchanged': len(before)}), flush=True)
raise SystemExit(code)
