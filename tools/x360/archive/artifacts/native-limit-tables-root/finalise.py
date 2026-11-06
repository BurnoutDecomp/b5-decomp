"""Bind the existing six native CPP results and three exact original data tables."""
from pathlib import Path
import hashlib, json, subprocess, struct
ROOT = Path('D:/Reverse/BP-X360-Experimental-Worktrees/drive-thru-native-root')
HERE = Path(__file__).resolve().parent
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p): return json.loads(Path(p).read_text(encoding='utf-8'))
def git(*a): return subprocess.check_output(['git', '-C', str(ROOT), *a])
before, after = read(HERE / 'source-before.json'), read(HERE / 'source-after.json')
assert before['hashes'] == after['hashes'] and len(before['hashes']) == 6277
assert all(sha(ROOT / p) == value for p, value in before['hashes'].items())
report_path = HERE / 'native/compile-results.json'
report = read(report_path)
assert report['state'] == 'complete' and report['passed'] == 6 and report['failed'] == 0
inputs = read(HERE / 'actual-inputs.json')
assert all(sha(p) == value for p, value in inputs['headers'].items())
assert all(sha(p) == value for p, value in inputs['artifacts'].items())
assert not any('wave4-intake' in p.lower() for p in inputs['headers'])
for row in report['results']:
    assert row['passed'] and row['machine'] == 0x1f2
    assert struct.unpack_from('<H', Path(row['object']).read_bytes())[0] == 0x1f2
assert len(inputs['exact_original_tables']) == 3
assert sum(t['bytes'] for t in inputs['exact_original_tables']) == 200
compiler = read(HERE.parent / 'wave6/full-stage1-terminal-audit.json')['external_inputs']['compiler_and_runtime']
assert len(compiler) == 7 and all(sha(p) == value for p, value in compiler.items())
assert git('rev-parse', 'HEAD^').decode().strip() == before['base']
changed = git('diff-tree', '--no-commit-id', '--name-only', '-r', 'HEAD').decode().splitlines()
assert len(changed) == 4 and not git('diff', '--name-only').strip()
head = git('rev-parse', 'HEAD').decode().strip()
(HERE / 'owned.patch').write_bytes(git('diff', '--binary', 'HEAD^', 'HEAD'))
result = {'base': before['base'], 'head': head, 'sources': changed,
          'source_hashes': {p: sha(ROOT / p) for p in changed},
          'git_blobs': {p: git('rev-parse', 'HEAD:' + p).decode().strip() for p in changed},
          'patch_sha256': sha(HERE / 'owned.patch'), 'report': str(report_path),
          'report_sha256': sha(report_path), 'passed': 6, 'failed': 0,
          'exact_original_tables': inputs['exact_original_tables'], 'original_table_bytes': 200,
          'original_data': str(HERE / 'original-data.json'), 'original_data_sha256': sha(HERE / 'original-data.json'),
          'artifact_manifest': str(HERE / 'actual-inputs.json'), 'artifact_manifest_sha256': sha(HERE / 'actual-inputs.json'),
          'actual_headers': inputs['header_count'], 'compiler_and_runtime': compiler,
          'tracked_inputs_stable_during_compile': 6277, 'changes_from_frozen526': before['changed'],
          'scope': 'Only four CPPs: exact XDK float limits and fourteen recovered HUD thresholds. '
                   'No header, ABI, layout, provider or control-flow changes.',
          'limits': 'Three data tables match original bytes. Full method parity, global math ABI, '
                    'link/image and runtime behavior are not established by this compile checkpoint.',
          'verifier_sha256': sha(__file__)}
with (HERE / 'DELIVERY.json').open('x', encoding='utf-8') as stream:
    json.dump(result, stream, indent=2)
print(json.dumps({'head': head, 'passed': 6, 'exact_original_table_bytes': 200,
                  'headers': inputs['header_count'], 'delivery_sha256': sha(HERE / 'DELIVERY.json')}, indent=2))
