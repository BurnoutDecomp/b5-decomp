import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys


WORKTREE = Path('D:/Reverse/BP-X360-Experimental-Worktrees/cxx03_vec2_five_luna_04ed')
ARTIFACT = Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave16/vertex-state-recovery-sol')
CANONICAL = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp')
SDK = CANONICAL / 'build/x360/sdk-6534.4/portable/XDK'
BASELINE = '99e26860e1a2e3071ebdeb4273dd629908a489dd'
RUN = sys.argv[1] if len(sys.argv) > 1 else 'gate1'
SUFFIX = '' if RUN == 'gate1' else '_' + RUN
VENDORS = [
    'vendor/EABase/include/Common', 'vendor/EASTL/include', 'vendor/EAThread/include',
    'vendor/renderware/include', 'vendor/PPMalloc/include', 'vendor/coreallocator/include',
    'vendor/dirtysdk/include', 'vendor/zlib/include', 'vendor/lua/src',
]
SOURCES = [
    'src/SDKs/RenderEngineClub/MAIN/components/src/states/vertexprogramstate.cpp',
    'src/GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.cpp',
    'src/GameShared/GameClasses/Graphics/Dispatch/CgsOcclusionCullManager.cpp',
    'src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm3d.cpp',
    'src/GameSource/Graphics/PostFx/BrnPostFxShader.cpp',
    'src/SDKs/RenderEngineClub/MAIN/components/src/indexbuffer.cpp',
    'src/SDKs/RenderEngineClub/MAIN/components/src/device.cpp',
]
TEXT_SUFFIXES = {'.h', '.hpp', '.hxx', '.inl', '.inc', '.c', '.cpp', '.cxx', '.cc', '.ipp'}


def git(*args):
    return subprocess.check_output(['git', '-C', str(WORKTREE), *args], text=True).splitlines()


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def native_inventory(paths):
    return sorted(p for p in paths if p.startswith(('src/', 'vendor/'))
                  and Path(p).suffix.lower() in ('.cpp', '.c')
                  and not any(x.lower() == 'pc' for x in Path(p).parts))


def context():
    files = {WORKTREE / p for p in git('ls-files') if (WORKTREE / p).is_file()
             and Path(p).suffix.lower() in TEXT_SUFFIXES}
    for root in [CANONICAL / x for x in VENDORS] + [SDK / 'include/xbox']:
        files.update(p for p in root.rglob('*') if p.is_file())
    files.add(WORKTREE / 'tools/build_x360.py')
    for name in ['cl.exe', 'c1xx.dll', 'c1.dll', 'c2.dll']:
        p = SDK / 'bin/win32' / name
        if p.is_file():
            files.add(p)
    return {str(p.resolve()): sha(p) for p in sorted(files)}


ARTIFACT.mkdir(parents=True, exist_ok=True)
before = context()
(ARTIFACT / ('FROZEN_CONTEXT_BEFORE' + SUFFIX + '.json')).write_text(json.dumps(before, indent=2), encoding='utf-8')
baseline_inventory = native_inventory(git('ls-tree', '-r', '--name-only', BASELINE))
candidate_inventory = native_inventory(git('ls-files', '*.cpp', '*.c'))
inventory = {
    'collector': 'tools/build_x360.py --all selection logic; no full compilation performed',
    'collector_sha256': sha(WORKTREE / 'tools/build_x360.py'),
    'baseline': BASELINE, 'baseline_count': len(baseline_inventory),
    'candidate_count': len(candidate_inventory),
    'added': sorted(set(candidate_inventory) - set(baseline_inventory)),
    'removed': sorted(set(baseline_inventory) - set(candidate_inventory)),
    'candidate_paths': candidate_inventory,
}
(ARTIFACT / ('NATIVE_INVENTORY_DELTA' + SUFFIX + '.json')).write_text(json.dumps(inventory, indent=2), encoding='utf-8')
command = [sys.executable, '-X', 'utf8', '-B', str(WORKTREE / 'tools/build_x360.py'),
           '--sdk', str(SDK), '--out', str(ARTIFACT / RUN), '--jobs', '1', '--timeout', '180',
           '--compiler-arg=/showIncludes']
command += ['--compiler-arg=/I' + str(CANONICAL / p) for p in VENDORS]
for source in SOURCES:
    command += ['--source', source]
env = dict(os.environ)
env['TEMP'] = str(ARTIFACT)
env['TMP'] = str(ARTIFACT)
(ARTIFACT / ('GATE_COMMAND' + SUFFIX + '.json')).write_text(json.dumps(command, indent=2), encoding='utf-8')
print('Frozen context files:', len(before), flush=True)
print('Native inventory:', len(baseline_inventory), '->', len(candidate_inventory), flush=True)
with (ARTIFACT / (RUN + '-console.log')).open('w', encoding='utf-8') as log:
    process = subprocess.Popen(command, cwd=WORKTREE, env=env, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, text=True, errors='replace',
                               creationflags=subprocess.CREATE_NO_WINDOW)
    for line in process.stdout:
        log.write(line)
        log.flush()
        print(line, end='', flush=True)
    result = process.wait()
after = context()
changed = {p: {'before': before.get(p), 'after': after.get(p)}
           for p in set(before) | set(after) if before.get(p) != after.get(p)}
(ARTIFACT / ('FROZEN_CONTEXT_AFTER' + SUFFIX + '.json')).write_text(json.dumps(after, indent=2), encoding='utf-8')
(ARTIFACT / ('FROZEN_CONTEXT_COMPARISON' + SUFFIX + '.json')).write_text(
    json.dumps({'before_files': len(before), 'after_files': len(after),
                'changed': changed, 'compile_returncode': result}, indent=2), encoding='utf-8')
print('Frozen context changes:', len(changed), flush=True)
sys.exit(result if not changed else 2)
