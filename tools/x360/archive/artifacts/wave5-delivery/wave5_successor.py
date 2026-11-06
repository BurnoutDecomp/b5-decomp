"""Bind immutable Wave5 native artifacts and its exact eight-CPP repair overlay."""
import argparse
from collections import Counter, defaultdict, deque
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess

PARENT = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
SCRATCH = PARENT / 'scratch/x360_sdk'
CANONICAL = PARENT / 'b5-decomp'
WORK = Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
HERE = Path(__file__).resolve().parent
BASE = '8d8881e97a97809b57736dd7a6f39c076fe228ae'
FROZEN = '9769ee661135f95461f2ccff0e870ce1ade2a3c1'
RAW = HERE.parent / 'wave5-full/compile-results.json'
RAW_HASH = '9fdac46dc1823a1330ebfc3122f4d5be19d91ba6e68678463d185557a6c3d4ac'
BASELINE = CANONICAL / 'build/x360/native-parallel-wave4-final3-effective.json'
STAGE_META = SCRATCH / 'native-parallel-wave5-full-stage1-patch.json'
TERMINAL = HERE / 'stage1-terminal-audit.json'
NAME = 'native-parallel-wave5-final'
META = HERE / (NAME + '-patch.json')
REPAIR = HERE.parent / 'wave5-repair/compile-results.json'
EXPECTED_REPAIRS = {
    'src/GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.cpp',
    'src/GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager_Remove.cpp',
    'src/GameShared/GameClasses/Gui/CgsGuiModuleBridges.cpp',
    'src/GameShared/GameClasses/Gui/CgsGuiModuleIO_OutputBuffer.cpp',
    'src/GameShared/GameClasses/Gui/Model/CgsModelModuleIO_InputBuffer.cpp',
    'src/GameShared/GameClasses/Geometric/Primitives/CgsBox.cpp',
    'src/GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoup.cpp',
    'src/GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupPoly.cpp',
}
COMMON_PATH = SCRATCH / 'parallel-native/wave4_integrator_sol/wave4_evidence.py'
spec = importlib.util.spec_from_file_location('readonly_native_evidence', COMMON_PATH)
common = importlib.util.module_from_spec(spec)
spec.loader.exec_module(common)


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def git(*args, cwd=WORK):
    return subprocess.check_output(['git', '--no-optional-locks', *args], cwd=cwd)


def now():
    return datetime.now(timezone.utc).isoformat()


def save_new(path, data):
    with Path(path).open('x', encoding='utf-8', newline='\n') as stream:
        stream.write(json.dumps(data, indent=2) + '\n')


def check_current(meta):
    assert git('rev-parse', 'HEAD').decode().strip() == meta['head']
    assert not git('status', '--porcelain=v1').strip()
    assert hashlib.sha256(git('diff', '--binary', meta['base'])).hexdigest() == meta['sha256']
    assert common.source_snapshot() == meta['source_snapshot']
    for path, expected in meta['source_sha256'].items():
        assert sha(WORK / path) == expected, path


def external_inputs(report):
    directories = set()
    expected_dirs = None
    for row in report['results']:
        roots = tuple(a[2:] for a in row['command'] if a.startswith('/I'))
        if expected_dirs is None:
            expected_dirs = roots
        assert roots == expected_dirs, row['source']
        for value in roots:
            path = Path(value).resolve()
            if path.is_relative_to(CANONICAL):
                directories.add(path)
    trees = {}
    for directory in sorted(directories):
        files = {}
        aggregate = hashlib.sha256()
        for path in sorted((p for p in directory.rglob('*') if p.is_file()),
                           key=lambda p: p.relative_to(directory).as_posix().lower()):
            relative = path.relative_to(directory).as_posix()
            digest = sha(path)
            files[relative] = digest
            aggregate.update((relative + '\0' + digest + '\n').encode())
        assert files, str(directory)
        trees[str(directory)] = {'files': files, 'aggregate_path_sha256': aggregate.hexdigest()}
    modules = {}
    for name in ('EABase', 'EASTL', 'EAThread'):
        directory = CANONICAL / 'vendor' / name
        head = git('rev-parse', 'HEAD', cwd=directory).decode().strip()
        pin = git('ls-tree', BASE, 'vendor/' + name).decode().split()[2]
        status = git('status', '--porcelain=v1', '--untracked-files=all', cwd=directory).decode()
        assert head == pin and not status, (name, head, pin, status)
        modules[name] = {'head': head, 'gitlink': pin, 'status': status}
    sdk = Path(report['sdk'])
    tools = {}
    for name in ('cl.exe', 'c1.dll', 'c1xx.dll', 'c2.dll', 'cl.exe.manifest',
                 'msvcr80.dll', 'msvcp80.dll'):
        path = sdk / 'bin/win32' / name
        assert path.is_file(), str(path)
        tools[str(path)] = sha(path)
    return {'include_trees': trees, 'submodules': modules, 'compiler_and_runtime': tools,
            'scope': 'Conservative superset of all actual canonical fallback include directories, '
                     'EA submodule identities, and C/C++ compiler tools; observed at terminal. '
                     'This is not a preprocessor include trace.'}


def closure(previous, current):
    changed = {p for p in set(previous['hashes']) | set(current['hashes'])
               if previous['hashes'].get(p) != current['hashes'].get(p)}
    reverse = defaultdict(set)
    for graph in (previous['dependencies'], current['dependencies']):
        for parent, children in graph.items():
            for child in children:
                reverse[child].add(parent)
    affected = set(changed)
    pending = deque(changed)
    while pending:
        for parent in reverse[pending.popleft()]:
            if parent not in affected:
                affected.add(parent)
                pending.append(parent)
    return changed, affected


def verify_manifest(report, manifest):
    for row in report['results']:
        expected = manifest[row['source']]
        assert sha(row['log']) == expected['log_sha256'], row['source']
        if row['passed']:
            assert sha(row['object']) == expected['object_sha256'], row['source']
            assert sha(row['assembly']) == expected['assembly_sha256'], row['source']


def compare(report, prior):
    index = {r['source']: r for r in report['results']}
    old = {r['source']: r for r in prior['results']}
    return {
        'requested': len(index), 'passed': report['passed'], 'failed': report['failed'],
        'prior_passes_retained': sum(index[s]['passed'] for s in old if old[s]['passed']),
        'new_passes': sorted(s for s in index if index[s]['passed'] and (s not in old or not old[s]['passed'])),
        'regressions': sorted(s for s in old if old[s]['passed'] and not index[s]['passed']),
        'timeouts': sorted(s for s in index if index[s]['returncode'] == -1),
        'source_requested': sum(s.startswith('src/') for s in index),
        'source_passed': sum(r['passed'] for s, r in index.items() if s.startswith('src/')),
        'new_sources': sorted(set(index) - set(old)), 'removed_sources': sorted(set(old) - set(index)),
    }


def terminal():
    meta = read(STAGE_META)
    assert meta['head'] == FROZEN and meta['base'] == BASE
    check_current(meta)
    assert sha(RAW) == RAW_HASH
    exit_path = SCRATCH / 'parallel-native/wave5_integrator_sol/full-stage1-exit.json'
    assert read(exit_path)['head'] == FROZEN and read(exit_path)['exit_code'] == 1
    report, index, artifacts = common.report_check(RAW)
    assert set(index) == set(meta['native_inventory']) == set(meta['selected_sources'])
    assert len(index) == 3610 and report['passed'] == 3335 and report['failed'] == 275
    assert git('ls-files', '*.cpp', '*.c').decode().splitlines()
    expected = {p for p in common.inventory() if Path(p).suffix.lower() in ('.cpp', '.c')}
    assert set(index) == expected
    snapshot = meta['source_snapshot']
    assert len(snapshot['hashes']) == 6274
    assert snapshot['nonliteral_includes'] == [{'path': 'vendor/lua/src/lua.h', 'directive': 'LUA_USER_H'}]
    for row in report['results']:
        assert not any(re.match(r'/[DU]LUA_USER_H(?:[= ]|$)', a, re.I) for a in row['command'])
    assert not any(re.search(r'^\s*#\s*define\s+LUA_USER_H\b',
                            (WORK / p).read_text(encoding='utf-8-sig', errors='replace'), re.M)
                   for p in snapshot['hashes'])
    prior = read(BASELINE)
    prior_manifest_path = SCRATCH / 'parallel-native/wave4_integrator_sol/native-parallel-wave4-final3-artifact-manifest.json'
    prior_manifest = read(prior_manifest_path)
    assert sha(BASELINE) == prior_manifest['effective_report_sha256']
    verify_manifest(prior, prior_manifest['artifact_manifest'])
    summary = compare(report, prior)
    assert not summary['regressions'] and not summary['timeouts']
    assert summary['prior_passes_retained'] == 3233
    assert summary['new_sources'] == ['src/GameSource/Main/BrnMainX360.cpp'] and not summary['removed_sources']
    result = {'head': FROZEN, 'base': BASE, 'patch_sha256': meta['sha256'],
              'metadata': str(STAGE_META), 'metadata_sha256': sha(STAGE_META),
              'report': str(RAW), 'report_sha256': sha(RAW),
              'exit_receipt': str(exit_path), 'exit_receipt_sha256': sha(exit_path),
              'baseline_report': str(BASELINE), 'baseline_report_sha256': sha(BASELINE),
              'baseline_artifact_manifest_sha256': sha(prior_manifest_path),
              'summary': summary, 'artifact_manifest': artifacts,
              'source_snapshot': snapshot, 'external_inputs': external_inputs(report),
              'verifier': str(Path(__file__)), 'verifier_sha256': sha(__file__),
              'common_verifier_sha256': sha(COMMON_PATH), 'created_utc': now()}
    save_new(TERMINAL, result)
    print(json.dumps({k: v for k, v in result.items() if k not in ('artifact_manifest', 'source_snapshot', 'external_inputs')}, indent=2))
    print('TERMINAL AUDIT', sha(TERMINAL), 'objects', report['passed'], flush=True)


def freeze():
    terminal = read(TERMINAL)
    assert sha(RAW) == terminal['report_sha256']
    assert not git('status', '--porcelain=v1').strip()
    snapshot = common.source_snapshot()
    changed, affected = closure(terminal['source_snapshot'], snapshot)
    assert changed == EXPECTED_REPAIRS, changed
    native = set(read(STAGE_META)['native_inventory'])
    assert affected & native == EXPECTED_REPAIRS, affected & native
    patch = git('diff', '--binary', BASE)
    paths = git('diff', '--name-only', BASE).decode().splitlines()
    result = {'base': BASE, 'head': git('rev-parse', 'HEAD').decode().strip(),
              'workspace': str(WORK), 'sha256': hashlib.sha256(patch).hexdigest(),
              'paths': paths, 'source_sha256': {p: sha(WORK / p) for p in paths},
              'source_snapshot': snapshot, 'native_inventory': sorted(native),
              'repair_paths': sorted(changed), 'affected_sources': sorted(affected & native),
              'full_sweep_head': FROZEN, 'raw_report': str(RAW), 'raw_report_sha256': sha(RAW),
              'terminal_audit': str(TERMINAL), 'terminal_audit_sha256': sha(TERMINAL),
              'external_inputs': terminal['external_inputs'], 'created_utc': now()}
    assert external_inputs(read(RAW)) == terminal['external_inputs']
    save_new(META, result)
    with (HERE / (NAME + '.diff')).open('xb') as stream:
        stream.write(patch)
    print(json.dumps({k: v for k, v in result.items() if k not in ('paths', 'source_sha256', 'source_snapshot', 'external_inputs', 'native_inventory')}, indent=2))


def overlay():
    meta = read(META)
    check_current(meta)
    terminal = read(TERMINAL)
    assert sha(TERMINAL) == meta['terminal_audit_sha256']
    assert sha(RAW) == meta['raw_report_sha256'] == terminal['report_sha256']
    raw = read(RAW)
    verify_manifest(raw, terminal['artifact_manifest'])
    report, repairs, artifacts = common.report_check(REPAIR)
    assert set(repairs) == set(meta['affected_sources']) == EXPECTED_REPAIRS
    assert report['passed'] == 7 and report['failed'] == 1
    assert external_inputs(report) == terminal['external_inputs']
    changed, affected = closure(terminal['source_snapshot'], meta['source_snapshot'])
    assert affected & set(meta['native_inventory']) == set(repairs)
    repair_audit = {'head': meta['head'], 'patch_sha256': meta['sha256'],
                    'report': str(REPAIR), 'report_sha256': sha(REPAIR),
                    'requested': report['requested'], 'passed': report['passed'], 'failed': report['failed'],
                    'artifact_manifest': artifacts, 'source_snapshot': meta['source_snapshot'],
                    'external_inputs': terminal['external_inputs'], 'created_utc': now()}
    repair_audit_path = HERE / 'repair-terminal-audit.json'
    save_new(repair_audit_path, repair_audit)
    rows = [dict(repairs.get(r['source'], r), build_head=meta['head'] if r['source'] in repairs else FROZEN)
            for r in raw['results']]
    effective = dict(raw, results=rows, passed=sum(r['passed'] for r in rows))
    effective['failed'] = len(rows) - effective['passed']
    layers = [{'head': FROZEN, 'report': str(RAW), 'report_sha256': sha(RAW),
               'terminal_audit': str(TERMINAL), 'terminal_audit_sha256': sha(TERMINAL)},
              {'head': meta['head'], 'report': str(REPAIR), 'report_sha256': sha(REPAIR),
               'terminal_audit': str(repair_audit_path), 'terminal_audit_sha256': sha(repair_audit_path)}]
    effective['provenance'] = {'kind': 'One complete 3610-source native sweep plus one eight-CPP actual repair overlay',
                               'source_head': meta['head'], 'patch_sha256': meta['sha256'], 'layers': layers,
                               'input_invariance_verified': True, 'not_a_new_full_sweep': True,
                               'closure_method': meta['source_snapshot']['method']}
    effective_path = HERE / (NAME + '-effective.json')
    save_new(effective_path, effective)
    merged_artifacts = dict(terminal['artifact_manifest'], **artifacts)
    manifest = {'head': meta['head'], 'patch_sha256': meta['sha256'],
                'effective_report': str(effective_path), 'effective_report_sha256': sha(effective_path),
                'artifact_manifest': merged_artifacts, 'layers': layers,
                'input_invariance_verified': True, 'external_inputs': terminal['external_inputs']}
    save_new(HERE / (NAME + '-artifact-manifest.json'), manifest)
    summary = compare(effective, read(BASELINE))
    assert summary['prior_passes_retained'] == 3233 and not summary['regressions'] and not summary['timeouts']
    assert summary['passed'] == 3340 and summary['failed'] == 270
    save_new(HERE / 'final-compile-comparison.json', summary)
    print(json.dumps({k: v for k, v in summary.items() if k != 'new_passes'}, indent=2))
    print('NEW PASSES', len(summary['new_passes']), 'EFFECTIVE SHA256', sha(effective_path), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('phase', choices=('freeze', 'overlay'))
    args = parser.parse_args()
    {'terminal': terminal, 'freeze': freeze, 'overlay': overlay}[args.phase]()
