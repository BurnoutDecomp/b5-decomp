from datetime import datetime, timezone
from pathlib import Path
import hashlib, json, struct, subprocess

ROOT = Path('D:/Reverse/BP-X360-Experimental-Worktrees/sound_environment_luna_r10')
CANON = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp')
ART = Path(__file__).resolve().parent
BASE = '1598404ea16b7e16cbe00396bbd051e38b0d59f4'
BRANCH = 'codex/experimental-x360-sound-environment-luna'
SOURCES = [
    'src/GameSource/Sound/Vehicles/Environment/BrnAmbienceControl.cpp',
    'src/GameSource/Sound/Vehicles/Traffic/BrnTrafficHorn.cpp',
    'src/GameSource/Sound/Vehicles/Traffic/BrnTrafficStateManager.cpp',
    'src/GameSource/Sound/Vehicles/Wheels/BrnAISkidEffect.cpp',
    'src/GameSource/Sound/Vehicles/Wheels/BrnWheelControl.cpp',
]
sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
def git(*args, cwd=ROOT):
    return subprocess.check_output(['git', *args], cwd=cwd, text=True, encoding='utf-8').strip()

def file_ref(path):
    return {'path': str(path), 'sha256': sha(path)}

commit = git('rev-parse', 'HEAD')
parent = git('rev-parse', 'HEAD^')
assert parent == BASE, (parent, BASE)
assert git('branch', '--show-current') == BRANCH
assert not git('status', '--porcelain=v1'), git('status', '--porcelain=v1')
changed = git('diff-tree', '--no-commit-id', '--name-only', '-r', commit).splitlines()
assert changed == SOURCES, changed
body = git('show', '-s', '--format=%B', commit)
assert 'Co-Authored-By: Claude Luna <noreply@anthropic.com>' in body, body

pre_manifest_path = ART/'source-pre.json'
post_manifest_path = ART/'source-post.json'
bind_path = ART/'gate-five-source-pass'/'source-object-binding.json'
report_path = ART/'gate-five-source-pass'/'compile-results.json'
summary_path = ART/'gate-five-source-pass'/'gate-summary.json'
proof_path = ART/'native-coff-proof.json'
verify_path = ART/'precommit-verification.json'
pre_manifest = json.loads(pre_manifest_path.read_text(encoding='utf-8'))
post_manifest = json.loads(post_manifest_path.read_text(encoding='utf-8'))
bind = json.loads(bind_path.read_text(encoding='utf-8'))
report = json.loads(report_path.read_text(encoding='utf-8'))
summary = json.loads(summary_path.read_text(encoding='utf-8'))
proof = json.loads(proof_path.read_text(encoding='utf-8'))
verify = json.loads(verify_path.read_text(encoding='utf-8'))
assert pre_manifest['baseline'] == BASE and post_manifest['compiled_head'] == BASE
assert bind['baseline_head'] == BASE and bind['branch'] == BRANCH
assert summary['requested'] == 5 and summary['passed'] == 5 and summary['failed'] == 0 and summary['driver_returncode'] == 0
assert proof['coff_machine_expected'] == '0x01F2' and proof['all_five_positive']
assert verify['actual_include_files_recorded'] == len(bind['actual_headers'])
assert verify['actual_include_files_still_hash_match'] is True

source_rows = []
for rel in SOURCES:
    pre_path, post_path = ART/'source-pre'/rel, ART/'source-post'/rel
    pre_raw, post_raw = sha(pre_path), sha(post_path)
    assert pre_raw == pre_manifest['files'][rel]['sha256']
    assert post_raw == post_manifest['files'][rel]['sha256'] == bind['sources'][rel]
    assert git('hash-object', '--path='+rel, str(pre_path)) == git('rev-parse', f'{BASE}:{rel}')
    assert git('hash-object', '--path='+rel, str(post_path)) == git('rev-parse', f'{commit}:{rel}')
    assert (ROOT/rel).read_bytes() == post_path.read_bytes()
    source_rows.append({
        'path': rel,
        'pre_snapshot_sha256': pre_raw,
        'pre_baseline_blob': git('rev-parse', f'{BASE}:{rel}'),
        'post_snapshot_sha256': post_raw,
        'post_commit_blob': git('rev-parse', f'{commit}:{rel}'),
    })

# Prove exact include-file identities remained stable after the actual compiler run.
for path, expected in bind['actual_headers'].items():
    assert sha(Path(path)) == expected, path
# Vendor fallback roots use the canonical worktree only for absent EA submodule checkouts.
fallbacks = {}
for name in ('EABase', 'EASTL', 'EAThread'):
    entry = git('ls-tree', BASE, f'vendor/{name}')
    commit_id = entry.split()[2]
    checked_out = git('rev-parse', 'HEAD', cwd=CANON/f'vendor/{name}')
    assert commit_id == checked_out, (name, commit_id, checked_out)
    fallbacks[name] = {'baseline_gitlink': commit_id, 'canonical_checkout': checked_out}

object_rows = []
for rel in SOURCES:
    item = proof['objects'][rel]
    obj = Path(item['path'])
    raw = obj.read_bytes()[:2]
    machine = struct.unpack('<H', raw)[0]
    assert machine == 0x01F2 and raw.hex().upper() == item['coff_header_first_two_bytes_hex']
    assert sha(obj) == item['sha256']
    object_rows.append({
        'source': rel,
        'object': str(obj),
        'sha256': item['sha256'],
        'bytes': item['bytes'],
        'coff_machine_bytes': item['coff_header_first_two_bytes_hex'],
        'coff_machine': item['coff_machine_hex'],
    })

stage = ART/'gate-five-source-pass'
command_path = stage/'command.json'
binding_summary = {
    'requested': 5, 'passed': 5, 'failed': 0,
    'compile_results': file_ref(report_path),
    'gate_summary': file_ref(summary_path),
    'command': file_ref(command_path),
    'source_object_binding': file_ref(bind_path),
    'actual_include_files': len(bind['actual_headers']),
    'include_hashes_currently_match': True,
    'xdk_root': str(CANON/'build/x360/sdk-6534.4/portable/XDK'),
    'compiler': bind['native_compiler'],
    'build_driver_sha256': bind['driver_sha256'],
    'vendor_fallback_gitlinks': fallbacks,
}
receipt = {
    'format': 'native-x360-private-delivery-v1',
    'created_utc': datetime.now(timezone.utc).isoformat(),
    'repository': str(ROOT),
    'branch': BRANCH,
    'baseline': BASE,
    'commit': commit,
    'parent': parent,
    'author': git('show', '-s', '--format=%an <%ae>', commit),
    'coauthor_trailer': 'Claude Luna <noreply@anthropic.com>',
    'owned_paths': changed,
    'source_captures': {
        'pre_manifest': file_ref(pre_manifest_path),
        'post_manifest': file_ref(post_manifest_path),
        'files': source_rows,
    },
    'native_compile_gate': binding_summary,
    'native_coff_objects': {
        'proof': file_ref(proof_path),
        'machine': '0x01F2',
        'files': object_rows,
    },
    'precommit_verification': file_ref(verify_path),
    'scope_limit': 'Five-source X360 compile only; no link or runtime claim.',
    'verified_against_commit': True,
}
receipt_path = ART/'DELIVERY.json'
receipt_path.write_text(json.dumps(receipt, indent=2)+'\n', encoding='utf-8')
receipt_sha = sha(receipt_path)
(ART/'DELIVERY.json.sha256').write_text(receipt_sha+'  DELIVERY.json\n', encoding='ascii')

# Re-read the sealed receipt and verify its identity and source refs against this commit.
sealed = json.loads(receipt_path.read_text(encoding='utf-8'))
assert sealed['commit'] == git('rev-parse', 'HEAD') and sealed['parent'] == git('rev-parse', 'HEAD^')
assert sealed['owned_paths'] == git('diff-tree', '--no-commit-id', '--name-only', '-r', commit).splitlines()
for row in sealed['source_captures']['files']:
    assert git('rev-parse', f"{commit}:{row['path']}") == row['post_commit_blob']
    assert git('hash-object', '--path='+row['path'], str(ART/'source-post'/row['path'])) == row['post_commit_blob']
assert sha(receipt_path) == receipt_sha
verification = {
    'verified': True,
    'receipt': str(receipt_path),
    'receipt_sha256': receipt_sha,
    'commit': commit,
    'parent': parent,
    'committed_paths': changed,
    'source_blobs_match_post_snapshots': True,
    'gate': '5/5 passed',
    'coff_machine': '0x01F2 for all five objects',
    'actual_include_files': len(bind['actual_headers']),
}
(ART/'delivery-verification.json').write_text(json.dumps(verification, indent=2)+'\n', encoding='utf-8')
print(json.dumps(verification, indent=2))