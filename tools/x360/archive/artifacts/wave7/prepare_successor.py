"""Prepare the three mandatory fixes only after exact frozen FAIL is persisted."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
WORK = Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
FROZEN = '765d9ce2f097194da48d7180cfdf329476ae6210'
FIXES = ['a6c87c23418c1e9de55b6022738c6af5d60211a5',
         'a254d6af2bf84cc393b2dbb5e96f707c4add992d',
         '4de7fcc913891487150d7b1acc9d8b6b25a0655c']


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def git(*args):
    return subprocess.check_output(['git', '--no-optional-locks', *args], cwd=WORK).decode().strip()


assert git('rev-parse', 'HEAD') == FROZEN and not git('status', '--porcelain=v1')
meta_path = HERE / 'native-parallel-wave7-full-stage1-patch.json'
meta = read(meta_path)
audit_path = HERE / 'full-stage1-terminal-audit.json'
audit = read(audit_path)
assert sha(audit_path) == 'c5cc40080d41f5e10ca7f31843a22073455b9a2446bda731186ac364cbce7921'
assert audit['head'] == FROZEN and audit['metadata_sha256'] == sha(meta_path)
assert sha(audit['report']) == audit['report_sha256']
review_path = HERE / 'native-parallel-wave7-full-stage1-review-result.json'
review = read(review_path)
assert review['verdict'] == 'fail' and review['head'] == FROZEN and review['sha256'] == meta['sha256']
assert sha(review['report']) == review['report_sha256']
receipt_path = HERE / 'wave7-receipts.json'
assert sha(receipt_path) == sha(HERE / 'frozen-stage1-31-receipts.json') == meta['receipt_sha256']
assert len(read(receipt_path)['receipts']) == 31
rows = []
paths = set()
for commit in FIXES:
    assert git('rev-parse', commit + '^') == FROZEN
    owned = git('diff-tree', '--no-commit-id', '--name-only', '-r', commit).splitlines()
    assert all(p.startswith(('src/', 'vendor/')) and 'pc' not in Path(p).parts for p in owned)
    rows.append({'commit': commit, 'paths': owned, 'message': git('show', '-s', '--format=%B', commit),
                 'source_blobs': {p: git('rev-parse', commit + ':' + p) for p in owned}})
    paths.update(owned)
assert len(paths) == 8 and [len(row['paths']) for row in rows] == [2, 5, 1]
audio_path = HERE / 'iir2_fma_repair_luna/final/receipt.json'
assert sha(audio_path) == 'eb1f8984b7c4d37666ce67284b12d9223576e23c5c40ff1e15cee3f532c393b2'
audio = read(audio_path)
for key, digest in [('report', 'report_sha256'), ('object', 'object_sha256'),
                    ('assembly', 'assembly_sha256'), ('log', 'log_sha256')]:
    binding = audio['native_whole_cpp_compile']
    assert sha(binding[key]) == binding[digest]
evidence = [HERE.parent / 'wave7-gui-caller-repair-1/HANDOFF.json',
            Path('D:/Reverse/octree-layout-terra-build/LAYOUT_EVIDENCE.md'), audio_path]
old = read(HERE / 'intake-plan-03.json')
plan = {'base': meta['base'], 'frozen_full_head': FROZEN,
    'collect_now': old['collect_now'] + FIXES, 'new_commits': FIXES, 'new_nodes': rows,
    'successor_paths': sorted(paths), 'expected_total_receipts': 34,
    'frozen31_receipts_sha256': sha(HERE / 'frozen-stage1-31-receipts.json'),
    'previous_metadata_sha256': sha(meta_path), 'previous_terminal_audit_sha256': sha(audit_path),
    'previous_failed_review': str(review_path), 'previous_failed_review_sha256': sha(review_path),
    'evidence': {str(p): sha(p) for p in evidence},
    'scope': 'Only mandatory GUI2CPP, genuine Scene5path prefix/provider and Iir2 oneCPP repair; no optional375/late work.',
    'inferences_and_limits': [
        'Scene effective128 member from two distinct native contexts; JobData type64/800 unchanged; original new16 request preserved and real allocator alignment unproved.',
        'Iir2 root-authorized TU-local pure noinline innerfmadds primitive, onecall/sample; original arithmetic boundaries/output/state match; non-byte identity and performance unverified.',
        'Controlled GUI queue/callback/ID and Scene allocation/job services do not establish full frame/query/thread/hardware behavior.'],
    'required_gate': 'Actual selected then NEW complete native inventory sweep for changed headers, expected3615 with new real result-provider CPP; same sole exact successor PASS and original audit retain all3374 published passes.',
    'created_utc': datetime.now(timezone.utc).isoformat()}
with (HERE / 'intake-plan-04.json').open('x', encoding='utf-8') as stream:
    json.dump(plan, stream, indent=2)
source = (HERE / 'integrate_wave7.py').read_text()
assert source.count('intake-plan-03.json') == 1
with (HERE / 'integrate_successor.py').open('x', encoding='utf-8') as stream:
    stream.write(source.replace('intake-plan-03.json', 'intake-plan-04.json'))
print('Prepared exact mandatory three-commit/eight-input successor; no source changed.')
