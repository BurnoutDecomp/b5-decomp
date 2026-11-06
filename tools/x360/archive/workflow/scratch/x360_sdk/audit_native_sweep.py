"""Inspect completed native output and compare actual rows against the prior snapshot."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

DEFAULT_ROOT = Path(__file__).resolve().parents[2] / 'b5-decomp'
parser = argparse.ArgumentParser()
parser.add_argument('--workspace', type=Path, default=DEFAULT_ROOT,
                    help='Frozen source checkout to verify')
parser.add_argument('--artifact-root', type=Path, default=DEFAULT_ROOT,
                    help='Root for relative raw/effective compile report paths')
parser.add_argument('--report', required=True)
parser.add_argument('--baseline', required=True)
parser.add_argument('--batch', required=True)
parser.add_argument('--allow-new-source', action='append', default=[])
parser.add_argument('--allow-removed-source', action='append', default=[])
args = parser.parse_args()
ROOT = args.workspace.resolve()
ARTIFACT_ROOT = args.artifact_root.resolve()
scratch = Path(__file__).parent
report = json.loads((ARTIFACT_ROOT / args.report).read_text(encoding='utf-8'))
prior = json.loads((ARTIFACT_ROOT / args.baseline).read_text(encoding='utf-8'))
assert report['state'] == 'complete', report['state']
rows = report['results']
index = {r['source']: r for r in rows}
old = {r['source']: r for r in prior['results']}
assert len(rows) == len(index) == report['requested']
added = set(index) - set(old)
assert added == set(args.allow_new_source), added
removed = set(old) - set(index)
assert removed == set(args.allow_removed_source), removed
tracked = subprocess.check_output(['git','ls-files','*.cpp','*.c'], cwd=ROOT, text=True).splitlines()
tracked = {p for p in tracked if p.startswith(('src/','vendor/'))
           and not any(part.lower() == 'pc' for part in Path(p).parts)}
assert set(index) == tracked, (set(index)-tracked, tracked-set(index))
passed = sum(bool(r['passed']) for r in rows)
assert report['passed'] == passed and report['failed'] == len(rows) - passed
assert prior['passed'] == sum(bool(r['passed']) for r in prior['results'])
artifacts = []
for row in rows:
    assert 'pc' not in [p.lower() for p in Path(row['source']).parts]
    assert not any('/src/pc' in arg.replace('\\', '/').lower() for arg in row['command'])
    if row['passed']:
        obj, asm = Path(row['object']), Path(row['assembly'])
        machine = None
        if obj.is_file():
            with obj.open('rb') as stream:
                machine = struct.unpack('<H', stream.read(2))[0]
        if not (row['returncode'] == 0 and row['machine'] == 0x1F2
                and obj.is_file() and asm.is_file()
                and machine == 0x1F2):
            artifacts.append(row['source'])
assert not artifacts, artifacts
metadata = json.loads((scratch / (args.batch + '-patch.json')).read_text())
if 'workspace' in metadata:
    assert str(ROOT).lower() == str(Path(metadata['workspace']).resolve()).lower()
assert added <= set(metadata['paths'])
assert removed <= set(metadata['paths'])
for source in removed:
    evidence = metadata['removed_source_evidence'][source]
    assert evidence['providers']
    assert all(p in index and index[p]['passed'] for p in evidence['providers'])
    proof_path = scratch / evidence['proof']
    proof = json.loads(proof_path.read_text())
    assert hashlib.sha256(proof_path.read_bytes()).hexdigest() == evidence['proof_sha256']
    assert proof['removed_source'] == source
    if evidence['kind'] == 'duplicate-native-definitions':
        assert not proof['missing_external_definitions']
    elif evidence['kind'] == 'pc-only-adapter':
        # This one source was a synchronous x64 host adapter, not the native
        # asynchronous loader. Verify both its provenance and final caller closure.
        assert source == 'src/GameShared/GameClasses/System/Resource/CgsResourceBundleLoader.cpp'
        assert proof['base'] == metadata['base']
        assert proof['source_head'] == metadata['head']
        assert proof['replacement_sources'] == evidence['providers']
        assert proof['native_call_references'] == []
        assert proof['residual_pc_forward_friend_declarations'] == []
        assert not (ROOT / proof['removed_header']).exists()
        original = subprocess.check_output(
            ['git', 'show', metadata['base'] + ':' + source], cwd=ROOT)
        original_header = subprocess.check_output(
            ['git', 'show', metadata['base'] + ':' + proof['removed_header']], cwd=ROOT)
        assert hashlib.sha256(original).hexdigest() == proof['removed_source_sha256']
        assert hashlib.sha256(original_header).hexdigest() == proof['removed_header_sha256']
        assert b'PC bundle loader' in original and b'little-endian x64' in original
        lexical = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*.*?\*/', re.S)
        live = []
        for path in subprocess.check_output(['git', 'ls-files', 'src'], cwd=ROOT, text=True).splitlines():
            if 'pc' in [p.lower() for p in Path(path).parts]:
                continue
            if Path(path).suffix.lower() not in ('.cpp', '.c', '.h', '.hpp', '.inl', '.inc', '.ipp'):
                continue
            content = (ROOT / path).read_text(encoding='utf-8-sig', errors='replace')
            if re.search(r'^\s*#\s*include\s*[<"][^>"\n]*\bCgsResourceBundleLoader\.h[>"]', content, re.M):
                live.append(path)
            if not re.search(r'\bBundleLoader\b', content):
                continue
            if re.search(r'\bBundleLoader\b', lexical.sub('', content)):
                live.append(path)
        assert not live, live
    else:
        raise AssertionError(('unsupported removed-source proof', evidence['kind']))

for path, expected in metadata.get('source_sha256', {}).items():
    assert hashlib.sha256((ROOT / path).read_bytes()).hexdigest() == expected, path
actual_head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
assert actual_head == metadata['head'], (actual_head, metadata['head'])
diff = subprocess.check_output(['git', 'diff', '--binary', metadata['base']], cwd=ROOT)
actual_hash = hashlib.sha256(diff).hexdigest()
assert actual_hash == metadata['sha256'], (actual_hash, metadata['sha256'])
review = json.loads((scratch / (args.batch + '-review-result.json')).read_text())
assert review['verdict'] == 'pass' and review['sha256'] == actual_hash
if 'head' in review:
    assert review['head'] == actual_head
gains = sorted(s for s in index if index[s]['passed'] and (s not in old or not old[s]['passed']))
regressions = sorted(s for s in old if old[s]['passed'] and s in index and not index[s]['passed'])
summary = {'report': args.report, 'baseline': args.baseline,
           'workspace': str(ROOT), 'artifact_root': str(ARTIFACT_ROOT),
           'requested': len(rows),
           'passed': passed, 'failed': len(rows)-passed,
           'source_passed': sum(r['passed'] for r in rows if r['source'].startswith('src/')),
           'source_requested': sum(r['source'].startswith('src/') for r in rows),
           'prior_passes_retained': sum(index[s]['passed'] for s in old if old[s]['passed'] and s in index),
           'new_passes': gains, 'regressions': regressions, 'patch_sha256': actual_hash,
           'new_sources': sorted(added),
           'removed_sources': sorted(removed),
           'timeouts': [r['source'] for r in rows if r['returncode'] == -1]}
(scratch / (args.batch + '-sweep-comparison.json')).write_text(json.dumps(summary, indent=2)+'\n')
groups = Counter()
failures = []
for row in rows:
    if row['passed']:
        continue
    log = Path(row['log']).read_text(encoding='utf-8', errors='replace')
    match = re.search(r'^.*(?:fatal error|error C)\d+.*$', log, re.M)
    first = match.group(0).strip() if match else (log.splitlines() or ['no diagnostics'])[0]
    key = re.sub(r'^.*?b5-decomp[\\/]', '', first)
    key = re.sub(r'\(\d+\)', '', key)
    groups[key] += 1
    failures.append({'source': row['source'], 'first': first, 'log': row['log']})
(scratch / ('remaining-' + args.batch + '-errors.json')).write_text(json.dumps(
    {'groups': groups.most_common(), 'failures': failures}, indent=2)+'\n')
print(json.dumps({k:v for k,v in summary.items() if k not in ('new_passes','regressions')}, indent=2))
print('New passes:', len(gains), 'regressions:', regressions)
print('Leading blockers:', json.dumps(groups.most_common(12), indent=2))
raise SystemExit(bool(regressions))
