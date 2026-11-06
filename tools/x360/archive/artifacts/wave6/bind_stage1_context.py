"""Pin receipt history, immutable audit helpers and unchanged publication context."""
import hashlib
import json
from pathlib import Path
import subprocess
from datetime import datetime, timezone

HERE = Path(__file__).resolve().parent
CONFIG = json.loads((HERE / 'campaign.json').read_text())
WORK = Path(CONFIG['workspace'])
CANONICAL = Path(CONFIG['canonical'])
PARENT = CANONICAL.parent


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def git(cwd, *args):
    return subprocess.check_output(['git', '--no-optional-locks', *args], cwd=cwd).decode().strip()


meta_path = HERE / 'native-parallel-wave6-full-stage1-patch.json'
audit_path = HERE / 'full-stage1-terminal-audit.json'
meta, audit = read(meta_path), read(audit_path)
assert audit['metadata_sha256'] == sha(meta_path)
assert git(WORK, 'rev-parse', 'HEAD') == meta['head']
assert not git(WORK, 'status', '--porcelain=v1')
assert git(CANONICAL, 'rev-parse', 'HEAD') == CONFIG['base']
assert not git(CANONICAL, 'status', '--porcelain=v1')
assert git(CANONICAL, 'branch', '--show-current') == 'codex/experimental-x360-byte-match'
baseline_meta = Path(CONFIG['baseline_metadata'])
assert sha(baseline_meta) == CONFIG['baseline_metadata_sha256']
assert read(baseline_meta)['head'] == CONFIG['base']
receipt_path = HERE / 'wave6-receipts.json'
assert sha(receipt_path) == meta['receipt_sha256']
receipts = read(receipt_path)
assert receipts['head'] == meta['head'] and receipts['state'] == 'complete'
assert len(receipts['receipts']) == 55
commits = git(WORK, 'rev-list', '--reverse', meta['base'] + '..' + meta['head']).splitlines()
assert commits == [row['integrated_commit'] for row in receipts['receipts']]
assert len({row['source_commit'] for row in receipts['receipts']}) == 55
assert set(git(WORK, 'diff', '--name-only', meta['base']).splitlines()) == set(meta['paths'])
assert len(meta['paths']) == 110
archive = HERE / 'frozen-stage1-55-receipts.json'
with archive.open('xb') as stream:
    stream.write(receipt_path.read_bytes())
parent_index = git(PARENT, 'ls-files', '--stage', '--', 'b5-decomp')
prior_publication = read(HERE.parent / 'wave5-delivery/wave5-final-publish-receipt.json')
assert parent_index == prior_publication['parent_gitlink_index_before']
helpers = [HERE / 'capture_terminal.py',
           HERE.parent / 'wave5-delivery/wave5_delivery.py',
           PARENT / 'scratch/x360_sdk/parallel-native/wave4_integrator_sol/wave4_evidence.py',
           WORK / 'tools/build_x360.py']
result = {
    'head': meta['head'], 'base': meta['base'], 'patch_sha256': meta['sha256'],
    'intake_tree': git(WORK, 'rev-parse', 'HEAD^{tree}'),
    'canonical_tree_at_terminal': git(CANONICAL, 'rev-parse', 'HEAD^{tree}'),
    'terminal_audit': str(audit_path), 'terminal_audit_sha256': sha(audit_path),
    'frozen_receipts': str(archive), 'frozen_receipts_sha256': sha(archive),
    'integrated_commits': commits, 'receipt_count': 55, 'changed_paths': 110,
    'baseline_metadata_sha256': sha(baseline_meta),
    'parent_gitlink_index': parent_index,
    'verifier_and_driver_hashes': {str(p): sha(p) for p in helpers},
    'created_utc': datetime.now(timezone.utc).isoformat(),
    'scope': 'Read-only terminal context; published234459 is unchanged. Frozen55 receipts are archived before authorized successor intake.'}
output = HERE / 'terminal-binding-context.json'
with output.open('x', encoding='utf-8') as stream:
    json.dump(result, stream, indent=2)
print('CONTEXT', output, sha(output), 'receipts', len(commits), flush=True)
