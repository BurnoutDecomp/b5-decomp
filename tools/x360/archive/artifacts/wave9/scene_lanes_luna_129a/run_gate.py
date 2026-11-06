from pathlib import Path
import hashlib
import json
import os
import subprocess
import sys

ROOT = Path(r'D:\Reverse\BP-X360-Experimental-Worktrees\scene_lanes_luna_129a')
CANON = Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp')
HERE = Path(__file__).resolve().parent
EXPECTED_BASE = '129afef95856e383366a55fb8f30a66ceb74de42'
SOURCES = [
    'src/GameShared/GameClasses/SceneManager/CgsEntityManager.cpp',
    'src/GameShared/GameClasses/SceneManager/CgsSceneManagerBridgeFunctions.cpp',
    'src/GameShared/GameClasses/SceneManager/CgsSceneManagerContact.cpp',
    'src/GameShared/GameClasses/Network/Debug Components/CgsNetworkPlayerManagerDebugComponent.cpp',
    'src/GameShared/GameClasses/Graphics/CgsMaterialAssembly.cpp',
]
VENDOR_FALLBACKS = (
    'EABase/include/Common', 'EASTL/include', 'EAThread/include', 'renderware/include',
    'PPMalloc/include', 'coreallocator/include', 'dirtysdk/include', 'zlib/include', 'lua/src',
)
STAGE = HERE / sys.argv[1]
STAGE.mkdir(exist_ok=False)

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT, text=True).strip()

head = git('rev-parse', 'HEAD')
assert head == EXPECTED_BASE, f'unexpected compile base: {head}'
changed = set(filter(None, git('diff', '--name-only').splitlines()))
assert changed == set(SOURCES), f'unexpected modified paths before gate: {sorted(changed)}'
assert not git('diff', '--cached', '--name-only'), 'staged paths present before gate'
inputs = {s: sha(ROOT / s) for s in SOURCES}
command = [sys.executable, '-X', 'utf8', '-B', str(ROOT / 'tools/build_x360.py'),
           '--sdk', str(CANON / 'build/x360/sdk-6534.4/portable/XDK'),
           '--out', str(STAGE), '--jobs', '1', '--compiler-arg=/showIncludes']
for source in SOURCES:
    command.extend(['--source', source])
for include in VENDOR_FALLBACKS:
    command.append('--compiler-arg=/I' + str(CANON / 'vendor' / include))
env = dict(os.environ)
tmp = HERE / 'tmp'
tmp.mkdir(exist_ok=True)
env['TEMP'] = env['TMP'] = str(tmp)
(STAGE / 'command.json').write_text(json.dumps(command, indent=2) + '\n', encoding='utf-8')
result = subprocess.run(command, cwd=ROOT, env=env, stdin=subprocess.DEVNULL,
                        capture_output=True, text=True, encoding='utf-8', errors='replace')
(STAGE / 'driver.log').write_text(result.stdout + result.stderr, encoding='utf-8')
post = {s: sha(ROOT / s) for s in SOURCES}
assert inputs == post, 'source changed during compile'
report_path = STAGE / 'compile-results.json'
report = json.loads(report_path.read_text(encoding='utf-8'))
binding = {
    'head': head,
    'branch': git('branch', '--show-current'),
    'sources_before_sha256': inputs,
    'sources_after_sha256': post,
    'compile_report_sha256': sha(report_path),
    'runner_sha256': sha(__file__),
    'actual_headers': {},
    'objects': {},
    'rows': [],
}
for row in report['results']:
    log_path = Path(row['log'])
    log = log_path.read_text(encoding='utf-8', errors='replace')
    errors = [line for line in log.splitlines() if 'error ' in line]
    print(row['passed'], row['source'], 'machine', row.get('machine'), 'diagnostics', len(errors))
    for line in errors:
        print(line)
    binding['rows'].append({
        'source': row['source'], 'passed': row['passed'], 'returncode': row.get('returncode'),
        'machine': row.get('machine'), 'log': str(log_path), 'log_sha256': sha(log_path),
        'diagnostics': errors,
    })
    for line in log.splitlines():
        if line.startswith('Note: including file:'):
            header = Path(line.split('Note: including file:', 1)[1].strip())
            assert header.is_file(), header
            binding['actual_headers'][str(header.resolve())] = sha(header)
    obj = row.get('object') or row.get('obj') or row.get('object_path')
    if obj:
        object_path = Path(obj)
        assert object_path.is_file(), object_path
        binding['objects'][row['source']] = {
            'path': str(object_path), 'sha256': sha(object_path),
            'machine': row.get('machine'), 'size': object_path.stat().st_size,
        }
assert {row['source'] for row in report['results']} == set(SOURCES), 'compile gate returned unexpected source set'
assert report['passed'] == len(SOURCES) and report['failed'] == 0, 'one or more owned CPPs failed native XDK compile'
assert all(row.get('machine') == 0x1F2 for row in report['results']), 'object machine is not native PPC32 X360'
assert len(binding['objects']) == len(SOURCES), 'native objects missing'
(STAGE / 'source-object-binding.json').write_text(json.dumps(binding, indent=2) + '\n', encoding='utf-8')
print('headers', len(binding['actual_headers']), 'passed', report['passed'], 'failed', report['failed'])
sys.exit(result.returncode)
