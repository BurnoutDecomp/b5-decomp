"""Read-only native artifact/row inspection; does not grant a source review pass."""
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2] / 'b5-decomp'
REPORT = ROOT / 'build/x360/full-native-parallel-wave1/compile-results.json'
BASE = ROOT / 'build/x360/native-xgraphics-arena-effective.json'
report = json.loads(REPORT.read_text(encoding='utf-8'))
base = json.loads(BASE.read_text(encoding='utf-8'))
metadata = json.loads((HERE.parent / 'native-parallel-wave1-patch.json').read_text())
assert report['state'] == 'complete'
rows = report['results']
index = {r['source']: r for r in rows}
old = {r['source']: r for r in base['results']}
assert len(rows) == len(index) == report['requested']
tracked = subprocess.check_output(['git', 'ls-files', '*.cpp', '*.c'], cwd=ROOT, text=True).splitlines()
tracked = {p for p in tracked if p.startswith(('src/', 'vendor/'))
           and not any(part.lower() == 'pc' for part in Path(p).parts)}
assert set(index) == tracked, (set(index) - tracked, tracked - set(index))
assert set(index) - set(old) == set(metadata['new_sources'])
assert not (set(old) - set(index))
patch = subprocess.check_output(['git', 'diff', '--binary', metadata['base']], cwd=ROOT)
assert hashlib.sha256(patch).hexdigest() == metadata['sha256']
passed = sum(bool(r['passed']) for r in rows)
assert passed == report['passed']
assert len(rows) - passed == report['failed']
assert base['passed'] == sum(bool(r['passed']) for r in base['results'])
for row in rows:
    assert not any(part.lower() == 'pc' for part in Path(row['source']).parts)
    assert not any('/src/pc' in arg.replace('\\', '/').lower() for arg in row['command'])
    if row['passed']:
        obj = Path(row['object'])
        with obj.open('rb') as stream:
            machine = struct.unpack('<H', stream.read(2))[0]
        assert row['returncode'] == 0 and row['machine'] == machine == 0x1f2
        assert Path(row['assembly']).is_file()
gains = sorted(s for s in index if index[s]['passed'] and (s not in old or not old[s]['passed']))
regressions = sorted(s for s in old if old[s]['passed'] and not index[s]['passed'])
summary = {
    'stage': 'native-artifact-inspection', 'source_review': 'pending',
    'head': metadata['head'], 'patch_sha256': metadata['sha256'],
    'report': str(REPORT), 'baseline': str(BASE),
    'requested': len(rows), 'passed': passed, 'failed': len(rows) - passed,
    'source_requested': sum(s.startswith('src/') for s in index),
    'source_passed': sum(r['passed'] for s, r in index.items() if s.startswith('src/')),
    'prior_passes_retained': sum(index[s]['passed'] for s in old if old[s]['passed']),
    'new_passes': gains, 'regressions': regressions,
    'new_sources': sorted(set(index) - set(old)),
    'timeouts': [r['source'] for r in rows if r['returncode'] == -1],
}
(HERE / 'wave1-artifact-inspection.json').write_text(json.dumps(summary, indent=2) + '\n')
groups = Counter()
failures = []
for row in rows:
    if row['passed']:
        continue
    log = Path(row['log']).read_text(encoding='utf-8', errors='replace')
    errors = re.findall(r'^.*(?:fatal error|error C)\d+.*$', log, re.M)
    first = errors[0].strip() if errors else (log.splitlines() or ['no diagnostics'])[0]
    key = re.sub(r'^.*?b5-decomp[\\/]', '', first)
    key = re.sub(r'\(\d+\)', '', key)
    groups[key] += 1
    failures.append({'source': row['source'], 'first': first, 'errors': errors, 'log': row['log']})
(HERE.parent / 'remaining-native-parallel-wave1-errors.json').write_text(json.dumps({
    'groups': groups.most_common(), 'failures': failures}, indent=2) + '\n')
print(json.dumps({k: v for k, v in summary.items() if k not in ('new_passes',)}, indent=2), flush=True)
print('New passes:', len(gains), flush=True)
print('Leading blockers:', json.dumps(groups.most_common(25), indent=2), flush=True)
