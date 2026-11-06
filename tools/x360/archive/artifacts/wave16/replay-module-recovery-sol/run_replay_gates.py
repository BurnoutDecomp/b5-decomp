from concurrent.futures import ThreadPoolExecutor
import ctypes
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys

ARTIFACT = Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave16/replay-module-recovery-sol')
OWN = Path('D:/Reverse/BP-X360-Experimental-Worktrees/device_storage_terra')
ICE = Path('D:/Reverse/BP-X360-Experimental-Worktrees/cxx03_five_luna_04ed')
CANONICAL = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp')
SDK = CANONICAL / 'build/x360/sdk-6534.4/portable/XDK'
ICE_HEAD = 'ba05880df217c6c75ec63b7a7534b6c2f9c31121'
HEADER = 'src/GameSource/Replays/BrnReplayModule.h'
OVERLAY = ARTIFACT / 'typed-game-overlay-final'
VENDORS = ['vendor/EABase/include/Common', 'vendor/EASTL/include', 'vendor/EAThread/include',
           'vendor/renderware/include', 'vendor/PPMalloc/include', 'vendor/coreallocator/include',
           'vendor/dirtysdk/include', 'vendor/zlib/include', 'vendor/lua/src']
TEXT = {'.h', '.hpp', '.hxx', '.inl', '.inc', '.c', '.cpp', '.cxx', '.cc', '.ipp'}


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args], text=True).strip()


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def snapshot(root):
    files = {root / p for p in git(root, 'ls-files').splitlines()
             if Path(p).suffix.lower() in TEXT and (root / p).is_file()}
    for directory in [CANONICAL / p for p in VENDORS] + [SDK / 'include/xbox']:
        files.update(p for p in directory.rglob('*') if p.is_file())
    files.add(root / 'tools/build_x360.py')
    files.add(SDK / 'bin/win32/cl.exe')
    for name in ['c1xx.dll', 'c1.dll', 'c2.dll']:
        p = SDK / 'bin/win32' / name
        if p.is_file(): files.add(p)
    if root == ICE:
        files.add(OVERLAY / 'GameSource/Replays/BrnReplayModule.h')
    return {str(p.resolve()): sha(p) for p in sorted(files)}


def run(root, name, sources, overlay=False):
    output = ARTIFACT / name
    output.mkdir(parents=True, exist_ok=True)
    before = snapshot(root)
    (output / 'context-before.json').write_text(json.dumps(before, indent=2), encoding='utf-8')
    print(name, 'frozen files', len(before), flush=True)
    spec = importlib.util.spec_from_file_location('native_builder_' + name, root / 'tools/build_x360.py')
    builder = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(builder)
    incs = ([OVERLAY] if overlay else []) + [root / p for p in builder.INCLUDES if (root / p).is_dir()]
    incs += [SDK / 'include/xbox']
    flags = builder.FLAGS + ['/I' + str(p) for p in incs] + ['/showIncludes', '/d1reportSingleClassLayoutReplayModule']
    if overlay:
        flags += ['/d1reportSingleClassLayoutBrnGameModule']
    flags += ['/I' + str(CANONICAL / p) for p in VENDORS]
    env = dict(os.environ)
    env.update({'XEDK': str(SDK), 'PATH': str(SDK / 'bin/win32') + os.pathsep + str(Path(os.environ['SystemRoot']) / 'System32'),
                'INCLUDE': str(SDK / 'include/xbox'), 'LIB': str(SDK / 'lib/xbox'),
                'TEMP': str(ARTIFACT), 'TMP': str(ARTIFACT)})
    for key in ['CL', '_CL_', 'LINK', '_LINK_']:
        env.pop(key, None)
    report = {'stage': 'compile-only', 'context': name, 'source_root': str(root),
              'source_root_head': git(root, 'rev-parse', 'HEAD'), 'jobs': 1,
              'overlay_only': HEADER if overlay else None, 'results': []}
    bindings = []
    for source in sources:
        stem = Path(source).stem + '-' + hashlib.sha256(source.lower().encode()).hexdigest()[:8]
        obj, asm, log = [output / (stem + suffix) for suffix in ['.obj', '.asm', '.log']]
        assert not any(p.exists() for p in [obj, asm, log]), 'Preserve old diagnostics; choose a new gate name'
        command = [str(SDK / 'bin/win32/cl.exe')] + flags + ['/Fo' + str(obj), '/Fa' + str(asm), source]
        result = subprocess.run(command, cwd=root, env=env, stdin=subprocess.DEVNULL,
                                capture_output=True, text=True, errors='replace', timeout=180,
                                creationflags=subprocess.CREATE_NO_WINDOW)
        text = result.stdout + result.stderr
        log.write_text(text, encoding='utf-8')
        machine = struct.unpack('<H', obj.read_bytes()[:2])[0] if obj.is_file() else None
        passed = result.returncode == 0 and machine == 0x1f2 and asm.is_file()
        report['results'].append({'source': source, 'source_sha256': sha(root / source),
                                  'passed': passed, 'returncode': result.returncode, 'machine': machine,
                                  'object': str(obj), 'assembly': str(asm), 'log': str(log), 'command': command})
        seen = []
        for line in text.splitlines():
            if 'including file:' in line.lower():
                p = Path(re.split('including file:', line, maxsplit=1, flags=re.I)[1].strip()).resolve()
                digest = sha(p)
                assert before.get(str(p)) == digest, ('include outside frozen context', str(p))
                seen.append({'path': str(p), 'sha256': digest})
        bindings.append({'source': source, 'includes': seen})
        print(('PASS ' if passed else 'FAIL ') + name + ' ' + source, flush=True)
        if not passed:
            print('\n'.join(x for x in text.splitlines() if re.search(r'\b(?:warning|error) [A-Z]\d+', x))[:8000], flush=True)
    after = snapshot(root)
    changed = {p: {'before': before.get(p), 'after': after.get(p)}
               for p in set(before) | set(after) if before.get(p) != after.get(p)}
    assert not changed, changed
    (output / 'context-after.json').write_text(json.dumps(after, indent=2), encoding='utf-8')
    (output / 'context-check.json').write_text(json.dumps({'files': len(before), 'changed': changed}, indent=2), encoding='utf-8')
    (output / 'showIncludes-bindings.json').write_text(json.dumps(bindings, indent=2), encoding='utf-8')
    report.update({'passed': sum(r['passed'] for r in report['results']),
                   'failed': sum(not r['passed'] for r in report['results']), 'state': 'complete'})
    (output / 'compile-results.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    if overlay:
        expected = str((OVERLAY / 'GameSource/Replays/BrnReplayModule.h').resolve())
        assert sha(OWN / HEADER) == sha(expected)
        used = [entry for row in bindings for entry in row['includes'] if entry['path'] == expected]
        stale = [entry for row in bindings for entry in row['includes'] if entry['path'] == str((ICE / HEADER).resolve())]
        assert used and not stale, ('overlay binding not established', used, stale)
        (output / 'overlay-binding.json').write_text(json.dumps({'used': used, 'stale_original_header_bindings': stale,
                                                                'source_header_sha256': sha(OWN / HEADER)}, indent=2), encoding='utf-8')
    return report


assert git(ICE, 'rev-parse', 'HEAD') == ICE_HEAD and not git(ICE, 'status', '--porcelain')
target = OVERLAY / 'GameSource/Replays/BrnReplayModule.h'
target.parent.mkdir(parents=True, exist_ok=True)
shutil.copyfile(OWN / HEADER, target)
assert sha(target) == sha(OWN / HEADER)
ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)
baseline = run(OWN, 'attempt03-replay-native-final', [
    'src/GameSource/Replays/BrnReplayModule.cpp',
    'src/GameSource/Replays/BrnReplayModule_Prepare.cpp',
    'src/GameSource/Replays/BrnReplayDebugComponent.cpp',
])
combined = run(ICE, 'attempt04-typed-game-overlay-final', ['src/GameSource/Game/BrnGameModule.cpp'], overlay=True)
assert git(ICE, 'rev-parse', 'HEAD') == ICE_HEAD and not git(ICE, 'status', '--porcelain')
print('Replay baseline', baseline['passed'], '/', len(baseline['results']),
      'typed overlay', combined['passed'], '/', len(combined['results']), flush=True)
sys.exit(0 if baseline['failed'] == 0 and combined['failed'] == 0 else 1)
