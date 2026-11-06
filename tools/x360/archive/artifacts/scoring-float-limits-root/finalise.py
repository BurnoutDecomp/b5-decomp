"""Verify the completed source-only float-limit checkpoint without recompiling."""
from pathlib import Path
import hashlib, json, subprocess

ROOT = Path('D:/Reverse/BP-X360-Experimental-Worktrees/drive-thru-native-root')
HERE = Path(__file__).resolve().parent
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p): return json.loads(Path(p).read_text(encoding='utf-8'))
def git(*a): return subprocess.check_output(['git', '-C', str(ROOT), *a])
before, after = read(HERE / 'source-before.json'), read(HERE / 'source-after.json')
assert before['hashes'] == after['hashes'] and len(before['hashes']) == 6277
assert all(sha(ROOT / p) == value for p, value in before['hashes'].items())
report = read(HERE / 'native/compile-results.json')
assert report['state'] == 'complete' and report['passed'] == 5 and report['failed'] == 0
inputs = read(HERE / 'actual-inputs.json')
assert all(sha(p) == digest for p, digest in inputs['headers'].items())
assert all(sha(p) == digest for p, digest in inputs['artifacts'].items())
assert not any('wave4-intake' in p.lower() for p in inputs['headers'])
audit = read(HERE.parent / 'wave6/full-stage1-terminal-audit.json')
tools = audit['external_inputs']['compiler_and_runtime']
assert all(sha(p) == digest for p, digest in tools.items()) and len(tools) == 7
assert git('rev-parse', 'HEAD^').decode().strip() == before['base']
changed = git('diff-tree', '--no-commit-id', '--name-only', '-r', 'HEAD').decode().splitlines()
assert set(changed) == set(inputs['emitted_exact_float_limit']) and len(changed) == 3
assert not git('diff', '--name-only').strip()
head = git('rev-parse', 'HEAD').decode().strip()
patch = git('diff', '--binary', 'HEAD^', 'HEAD')
(HERE / 'owned.patch').write_bytes(patch)
result = {'base': before['base'], 'head': head, 'sources': changed,
          'source_sha256': {p: sha(ROOT / p) for p in changed},
          'git_blobs': {p: git('rev-parse', 'HEAD:' + p).decode().strip() for p in changed},
          'patch_sha256': sha(HERE / 'owned.patch'),
          'report': str(HERE / 'native/compile-results.json'),
          'report_sha256': sha(HERE / 'native/compile-results.json'),
          'passed': 5, 'failed': 0, 'machine': '0x01F2', 'actual_headers': inputs['header_count'],
          'artifact_manifest': str(HERE / 'actual-inputs.json'),
          'artifact_manifest_sha256': sha(HERE / 'actual-inputs.json'),
          'original_constants': read(HERE / 'original-constants.json'),
          'compiler_and_runtime': tools, 'tracked_inputs_unchanged_during_compile': 6277,
          'changes_from_frozen526': before['changed'],
          'source_scope': 'Only three CPP float-limit spellings and their cfloat includes; '
                          'previous Root DriveThru commit is an existing private prerequisite.',
          'limits': 'Actual original constants and emitted native values are verified; '
                    'no full-function semantic/byte parity, global vector ABI, image or launch claim.',
          'verifier_sha256': sha(__file__)}
with (HERE / 'DELIVERY.json').open('x', encoding='utf-8') as stream:
    json.dump(result, stream, indent=2)
print(json.dumps({'head': head, 'passed': 5, 'headers': inputs['header_count'],
                  'delivery_sha256': sha(HERE / 'DELIVERY.json')}, indent=2))
