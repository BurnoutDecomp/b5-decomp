"""Read-only source/artifact inspection; writes only new, explicit evidence files."""
import argparse
from collections import defaultdict, deque
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

PARENT = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
SCRATCH = PARENT / 'scratch/x360_sdk'
HERE = Path(__file__).resolve().parent
WORK = Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
ARTIFACTS = PARENT / 'b5-decomp'
BASE = '551cd95c0184f7c29de7ea1203a8d6d57bb07c0a'
FROZEN = 'e1026b2324dd376839bc0484e82e5fe0d4b005cc'
RAW = ARTIFACTS / 'build/x360/full-native-parallel-wave4-stage1/compile-results.json'
PRIOR = ARTIFACTS / 'build/x360/native-parallel-wave3-final-effective.json'
TEXT_EXT = {'.cpp', '.c', '.h', '.hpp', '.inl', '.inc', '.ipp'}


def git(*args):
    return subprocess.check_output(['git', '-C', str(WORK), *args])


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write_new(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('x', encoding='utf-8', newline='\n') as stream:
        stream.write(json.dumps(value, indent=2) + '\n')


def inventory():
    return [p for p in git('ls-files', 'src', 'vendor').decode().splitlines()
            if p.startswith(('src/', 'vendor/')) and 'pc' not in [x.lower() for x in Path(p).parts]]


def source_snapshot():
    files = [p for p in inventory() if Path(p).suffix.lower() in TEXT_EXT]
    hashes = {p: sha(WORK / p) for p in files}
    lookup = {p.lower(): p for p in files}
    byname = defaultdict(list)
    for p in files:
        byname[Path(p).name.lower()].append(p)
    dependencies = {}
    dynamic = []
    for p in files:
        content = (WORK / p).read_text(encoding='utf-8-sig', errors='replace')
        includes = re.findall(r'^\s*#\s*include\s*([^\r\n]+)', content, re.M)
        edges = set()
        for directive in includes:
            match = re.match(r'[<"]([^>"\r\n]+)[>"]', directive)
            if not match:
                dynamic.append({'path': p, 'directive': directive})
                continue
            include = match.group(1).replace('\\', '/')
            # Every basename candidate is retained. This intentionally includes
            # inactive conditional branches and overestimates ambiguous search paths.
            edges.update(byname.get(Path(include).name.lower(), []))
            local = (WORK / Path(p).parent / include).resolve()
            try:
                relative = local.relative_to(WORK).as_posix().lower()
            except ValueError:
                relative = ''
            if relative in lookup:
                edges.add(lookup[relative])
        dependencies[p] = sorted(edges)
    return {'hashes': hashes, 'dependencies': dependencies,
            'nonliteral_includes': dynamic,
            'method': 'Conservative literal-include graph; all basename matches and inactive branches included. Nonliteral sites listed separately.'}


def report_check(path):
    report = json.loads(path.read_text(encoding='utf-8'))
    assert report['state'] == 'complete', (str(path), report['state'])
    rows = report['results']
    index = {r['source']: r for r in rows}
    assert len(rows) == len(index) == report['requested']
    passed = sum(bool(r['passed']) for r in rows)
    assert report['passed'] == passed and report['failed'] == len(rows) - passed
    artifacts = {}
    for row in rows:
        assert 'pc' not in [p.lower() for p in Path(row['source']).parts]
        assert not any('/src/pc' in arg.replace('\\', '/').lower() for arg in row['command'])
        assert row['command'][-1].replace('\\', '/') == row['source']
        assert '/Fo' + row['object'] in row['command']
        assert '/Fa' + row['assembly'] in row['command']
        assert Path(row['log']).is_file(), row['source']
        record = {'log_sha256': sha(row['log'])}
        if row['passed']:
            obj, listing = Path(row['object']), Path(row['assembly'])
            assert row['returncode'] == 0 and row['machine'] == 0x1F2
            assert obj.is_file() and listing.is_file() and listing.stat().st_size > 0
            with obj.open('rb') as stream:
                assert struct.unpack('<H', stream.read(2))[0] == 0x1F2, row['source']
            record.update(object_sha256=sha(obj), assembly_sha256=sha(listing))
        artifacts[row['source']] = record
    return report, index, artifacts


def raw():
    assert git('rev-parse', 'HEAD').decode().strip() == FROZEN
    assert not git('status', '--porcelain=v1').strip()
    report, index, artifacts = report_check(RAW)
    expected = {p for p in inventory() if Path(p).suffix.lower() in ('.cpp', '.c')}
    assert set(index) == expected, (set(index) - expected, expected - set(index))
    prior = json.loads(PRIOR.read_text())
    old = {r['source']: r for r in prior['results']}
    meta = json.loads((SCRATCH / 'native-parallel-wave4-stage1-patch.json').read_text())
    assert hashlib.sha256(git('diff', '--binary', BASE)).hexdigest() == meta['sha256']
    snapshot = source_snapshot()
    assert snapshot['nonliteral_includes'] == [{'path':'vendor/lua/src/lua.h', 'directive':'LUA_USER_H'}]
    for row in json.loads(RAW.read_text())['results']:
        assert not any(re.match(r'/[DU]LUA_USER_H(?:[= ]|$)', a, re.I) for a in row['command'])
    assert not any(re.search(r'^\s*#\s*define\s+LUA_USER_H\b', (WORK / p).read_text(encoding='utf-8-sig',errors='replace'), re.M)
                   for p in snapshot['hashes'])
    result = {'head': FROZEN, 'report': str(RAW), 'report_sha256': sha(RAW),
              'requested': report['requested'], 'passed': report['passed'], 'failed': report['failed'],
              'artifact_manifest': artifacts, 'source_snapshot': snapshot,
              'added': sorted(set(index) - set(old)), 'removed': sorted(set(old) - set(index)),
              'new_passes': sorted(s for s in index if index[s]['passed'] and (s not in old or not old[s]['passed'])),
              'regressions': sorted(s for s in old if old[s]['passed'] and s in index and not index[s]['passed']),
              'timeouts': sorted(s for s in index if index[s]['returncode'] == -1),
              'created_utc': datetime.now(timezone.utc).isoformat()}
    write_new(HERE / 'stage1-terminal-audit.json', result)
    print(json.dumps({k: v for k, v in result.items() if k not in ('source_snapshot', 'artifact_manifest', 'new_passes')}, indent=2))
    print('New passes:', len(result['new_passes']))


def freeze():
    assert not git('status', '--porcelain=v1').strip()
    old = json.loads((HERE / 'stage1-terminal-audit.json').read_text())
    snapshot = source_snapshot()
    changed = {p for p in set(old['source_snapshot']['hashes']) | set(snapshot['hashes'])
               if old['source_snapshot']['hashes'].get(p) != snapshot['hashes'].get(p)}
    reverse = defaultdict(set)
    for graph in (old['source_snapshot']['dependencies'], snapshot['dependencies']):
        for parent, children in graph.items():
            for child in children:
                reverse[child].add(parent)
    affected = set(changed)
    queue = deque(changed)
    while queue:
        for parent in reverse[queue.popleft()]:
            if parent not in affected:
                affected.add(parent)
                queue.append(parent)
    requested = {r['source'] for r in json.loads(RAW.read_text())['results']}
    sources = sorted(affected & requested)
    patch = git('diff', '--binary', BASE)
    head = git('rev-parse', 'HEAD').decode().strip()
    paths = git('diff', '--name-only', BASE).decode().splitlines()
    metadata = {'base': BASE, 'head': head, 'workspace': str(WORK),
                'sha256': hashlib.sha256(patch).hexdigest(), 'paths': paths,
                'source_sha256': {p: sha(WORK / p) for p in paths if (WORK / p).is_file()},
                'stage1_head': FROZEN, 'stage1_raw_report_sha256': sha(RAW),
                'repair_paths': sorted(changed), 'affected_sources': sources,
                'source_snapshot': snapshot, 'created_utc': datetime.now(timezone.utc).isoformat()}
    write_new(SCRATCH / 'native-parallel-wave4-final-patch.json', metadata)
    with (SCRATCH / 'native-parallel-wave4-final.diff').open('xb') as stream:
        stream.write(patch)
    write_new(HERE / 'affected-sources.json', sources)
    print(json.dumps({k:v for k,v in metadata.items() if k not in ('paths','source_sha256','source_snapshot','affected_sources')},indent=2))
    print('Final patch paths:',len(paths),'affected source count:',len(sources))


def overlay(target):
    meta = json.loads((SCRATCH / 'native-parallel-wave4-final-patch.json').read_text())
    assert git('rev-parse', 'HEAD').decode().strip() == meta['head']
    assert not git('status', '--porcelain=v1').strip()
    assert hashlib.sha256(git('diff', '--binary', BASE)).hexdigest() == meta['sha256']
    for p, h in meta['source_snapshot']['hashes'].items():
        assert sha(WORK / p) == h, p
    original, raw_index, raw_artifacts = report_check(RAW)
    rebuilt, new_index, new_artifacts = report_check(target)
    assert set(new_index) == set(meta['affected_sources']), (set(new_index) ^ set(meta['affected_sources']))
    assert sha(RAW) == meta['stage1_raw_report_sha256']
    previous_audit = json.loads((HERE / 'stage1-terminal-audit.json').read_text())
    assert raw_artifacts == previous_audit['artifact_manifest'], 'Raw artifacts changed'
    result = dict(original)
    result['results'] = [dict(new_index.get(r['source'], r),
                             build_head=meta['head'] if r['source'] in new_index else FROZEN)
                         for r in original['results']]
    result['passed'] = sum(r['passed'] for r in result['results'])
    result['failed'] = result['requested'] - result['passed']
    result['provenance'] = {'kind': 'full sweep with affected-source repair overlay',
                            'source_head': meta['head'], 'patch_sha256': meta['sha256'],
                            'full_sweep_head': FROZEN, 'full_sweep_report': str(RAW),
                            'full_sweep_report_sha256': sha(RAW),
                            'repair_report': str(target), 'repair_report_sha256': sha(target),
                            'rebuilt_sources': meta['affected_sources'],
                            'closure_method': meta['source_snapshot']['method'],
                            'not_a_new_full_sweep': True}
    effective = ARTIFACTS / 'build/x360/native-parallel-wave4-final-effective.json'
    write_new(effective, result)
    write_new(HERE / 'final-artifact-manifest.json',
              {'head': meta['head'], 'patch_sha256': meta['sha256'],
               'effective_report': str(effective), 'effective_report_sha256': sha(effective),
               'artifact_manifest': dict(raw_artifacts, **new_artifacts)})
    print(json.dumps({'effective_report': str(effective), 'requested':result['requested'],
                      'passed':result['passed'],'failed':result['failed'],
                      'rebuilt':len(new_index),'repair_passed':rebuilt['passed'],
                      'repair_failed':rebuilt['failed'],'effective_sha256':sha(effective)},indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stage', choices=('raw', 'freeze', 'overlay'))
    parser.add_argument('--target', type=Path)
    args = parser.parse_args()
    {'raw':raw,'freeze':freeze,'overlay':lambda:overlay(args.target)}[args.stage]()
