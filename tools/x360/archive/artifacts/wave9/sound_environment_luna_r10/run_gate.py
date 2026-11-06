from pathlib import Path
import hashlib, json, os, shutil, struct, subprocess, sys

ROOT = Path('D:/Reverse/BP-X360-Experimental-Worktrees/sound_environment_luna_r10')
CANON = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp')
HERE = Path(__file__).resolve().parent
BASELINE = '1598404ea16b7e16cbe00396bbd051e38b0d59f4'
BRANCH = 'codex/experimental-x360-sound-environment-luna'
SOURCES = [
    'src/GameSource/Sound/Vehicles/Environment/BrnAmbienceControl.cpp',
    'src/GameSource/Sound/Vehicles/Traffic/BrnTrafficHorn.cpp',
    'src/GameSource/Sound/Vehicles/Traffic/BrnTrafficStateManager.cpp',
    'src/GameSource/Sound/Vehicles/Wheels/BrnAISkidEffect.cpp',
    'src/GameSource/Sound/Vehicles/Wheels/BrnWheelControl.cpp',
]
sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()

def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT, text=True, encoding='utf-8').strip()

head = git('rev-parse', 'HEAD')
branch = git('branch', '--show-current')
assert head == BASELINE, (head, BASELINE)
assert branch == BRANCH, (branch, BRANCH)
changed = git('diff', '--name-only').splitlines()
assert set(changed) == set(SOURCES), changed
assert not git('ls-files', '--others', '--exclude-standard'), 'unexpected untracked source files'

stage = HERE / 'gate-five-source-pass'
stage.mkdir(exist_ok=False)
tmp = HERE / 'tmp'
tmp.mkdir(exist_ok=True)
sdk = CANON / 'build/x360/sdk-6534.4/portable/XDK'
assert (sdk / 'bin/win32/cl.exe').is_file(), sdk

before = {s: sha(ROOT / s) for s in SOURCES}
command = [
    sys.executable, '-X', 'utf8', '-B', str(ROOT / 'tools/build_x360.py'),
    '--sdk', str(sdk), '--out', str(stage), '--jobs', '1',
    '--compiler-arg=/showIncludes',
]
for source in SOURCES:
    command.extend(['--source', source])
# The fresh worktree has its pinned EA submodule gitlinks but no submodule checkout.
# Use the same canonical vendor checkout, whose submodule gitlinks match this baseline.
for rel in (
    'EABase/include/Common', 'EASTL/include', 'EAThread/include',
    'renderware/include', 'PPMalloc/include', 'coreallocator/include',
    'dirtysdk/include', 'zlib/include', 'lua/src',
):
    inc = CANON / 'vendor' / rel
    assert inc.is_dir(), inc
    command.append('--compiler-arg=/I' + str(inc))

(stage / 'command.json').write_text(json.dumps(command, indent=2) + '\n', encoding='utf-8')
env = dict(os.environ)
env['TEMP'] = env['TMP'] = str(tmp)
run = subprocess.run(
    command, cwd=ROOT, env=env, stdin=subprocess.DEVNULL, capture_output=True,
    text=True, encoding='utf-8', errors='replace', timeout=1800,
)
(stage / 'driver.log').write_text(run.stdout + run.stderr, encoding='utf-8')
assert before == {s: sha(ROOT / s) for s in SOURCES}, 'source changed during compile'
report_path = stage / 'compile-results.json'
report = json.loads(report_path.read_text(encoding='utf-8'))
rows = report['results']
assert [row['source'] for row in rows] == SOURCES, [row['source'] for row in rows]

binding = {
    'baseline_head': head,
    'branch': branch,
    'sources': before,
    'compile_results_sha256': sha(report_path),
    'driver_sha256': sha(ROOT / 'tools/build_x360.py'),
    'native_compiler': {},
    'actual_headers': {},
    'objects': {},
}
for row in rows:
    log_path = Path(row['log'])
    log = log_path.read_text(encoding='utf-8', errors='replace')
    errors = [line for line in log.splitlines() if 'error C' in line or 'fatal error' in line]
    print(json.dumps({'source': row['source'], 'passed': row['passed'], 'returncode': row['returncode'], 'machine': row.get('machine'), 'errors': errors}, ensure_ascii=False))
    for line in log.splitlines():
        prefix = 'Note: including file:'
        if line.startswith(prefix):
            inc = Path(line[len(prefix):].strip())
            if not inc.is_absolute():
                inc = ROOT / inc
            inc = inc.resolve()
            assert inc.is_file(), inc
            binding['actual_headers'][str(inc)] = sha(inc)
    command_row = row.get('command', [])
    if command_row:
        compiler = Path(command_row[0])
        assert compiler.is_file(), compiler
        binding['native_compiler'] = {'path': str(compiler.resolve()), 'sha256': sha(compiler)}
    obj_value = row.get('object') or row.get('obj') or row.get('object_path')
    if obj_value:
        obj = Path(obj_value)
        if not obj.is_absolute():
            obj = ROOT / obj
        obj = obj.resolve()
        assert obj.is_file(), obj
        machine = struct.unpack('<H', obj.read_bytes()[:2])[0]
        binding['objects'][row['source']] = {
            'path': str(obj), 'sha256': sha(obj),
            'coff_machine': machine, 'coff_machine_hex': '0x%04X' % machine,
            'bytes': obj.stat().st_size,
        }

binding['actual_header_count'] = len(binding['actual_headers'])
(stage / 'source-object-binding.json').write_text(json.dumps(binding, indent=2) + '\n', encoding='utf-8')

post = HERE / 'source-post'
post_manifest = {}
for source in SOURCES:
    src = ROOT / source
    snapshot = post / source
    snapshot.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(src, snapshot)
    post_manifest[source] = {'sha256': sha(src), 'bytes': src.stat().st_size}
(HERE / 'source-post.json').write_text(json.dumps({'compiled_head': head, 'files': post_manifest}, indent=2) + '\n', encoding='utf-8')

passed = sum(bool(row['passed']) for row in rows)
summary = {
    'baseline_head': head, 'branch': branch, 'requested': len(SOURCES),
    'passed': passed, 'failed': len(SOURCES) - passed,
    'driver_returncode': run.returncode,
    'actual_header_count': len(binding['actual_headers']),
    'all_objects_xbox360_ppc_be': all(binding['objects'].get(s, {}).get('coff_machine') == 0x01F2 for s in SOURCES),
    'sources': [{'source': row['source'], 'passed': row['passed'], 'returncode': row['returncode']} for row in rows],
}
(stage / 'gate-summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
print(json.dumps(summary, indent=2))
if passed != len(SOURCES) or run.returncode != 0 or not summary['all_objects_xbox360_ppc_be']:
    sys.exit(1)