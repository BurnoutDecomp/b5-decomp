"""Publish a bounded R3 terminal checkpoint only after the actual full audit exists."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
PARENT = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler-session', type=int, required=True)
parser.add_argument('--binder-session', type=int, required=True)
args = parser.parse_args()


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def save(path, value):
    with Path(path).open('x', encoding='utf-8') as stream:
        json.dump(value, stream, indent=2)
        stream.write('\n')


def git(work, *options):
    return subprocess.check_output(['git', '--no-optional-locks', '-C', str(work), *options]).decode().strip()


config = read(HERE / 'campaign.json')
plan = read(config['current_plan'])
audit_path = HERE / 'full-stage1-terminal-audit.json'
audit = read(audit_path)
report = read(audit['report'])
terminal = read(audit['exit_receipt'])
receipts = read(HERE / 'repair-r3-receipts.json')
assert report['state'] == 'complete' and report['requested'] == 3616
assert len(report['results']) == report['requested'] == report['passed'] + report['failed']
assert sha(audit['report']) == audit['report_sha256'] == terminal['report_sha256']
assert terminal['head'] == audit['head'] == receipts['head']
assert terminal['exit_code'] == (1 if report['failed'] else 0)
assert receipts['state'] == 'complete' and len(receipts['receipts']) == len(plan['named_originals']) == 20
assert len(audit['source_snapshot']['hashes']) == 6297
assert audit['summary']['new_sources'] == audit['summary']['removed_sources'] == []
state = {key: {'head': git(config[key], 'rev-parse', 'HEAD'),
               'status': git(config[key], 'status', '--porcelain=v1')}
         for key in ('canonical', 'workspace')}
assert state['canonical'] == {'head': config['published_base'], 'status': ''}
assert state['workspace'] == {'head': audit['head'], 'status': ''}
assert git(PARENT, 'ls-files', '--stage', 'b5-decomp') == plan['parent_gitlink_index']
baseline_audit = read(config['baseline_artifact_manifest'])
published = read(config['published_artifact_manifest'])
ext = audit['external_inputs']
summary = {'head': audit['head'], 'base': audit['base'], 'patch_sha256': audit['patch_sha256'],
           'requested': report['requested'], 'passed': report['passed'], 'failed': report['failed'],
           'source_passed': audit['summary']['source_passed'], 'source_requested': audit['summary']['source_requested'],
           'versus_R2': audit['summary'], 'versus_published_wave7': audit['published_comparison'],
           'input_hashes': len(audit['source_snapshot']['hashes']),
           'actual_artifact_hash_entries': sum(len(v) for v in audit['artifact_manifest'].values()),
           'R2_artifact_entries_verified': sum(len(v) for v in baseline_audit['artifact_manifest'].values()),
           'published_artifact_entries_verified': sum(len(v) for v in published['artifact_manifest'].values()),
           'external_include_files': sum(len(v['files']) for v in ext['include_trees'].values()),
           'compiler_runtime_files': len(ext['compiler_and_runtime']), 'clean_pinned_submodules': len(ext['submodules']),
           'new_R3_receipts': len(receipts['receipts']), 'new_R3_paths': len(plan['candidate_paths']),
           'new_R3_headers': plan['new_paths'], 'new_product_cpp': [],
           'plan': config['current_plan'], 'plan_sha256': sha(config['current_plan']),
           'report': audit['report'], 'report_sha256': audit['report_sha256'],
           'terminal_audit': str(audit_path), 'terminal_audit_sha256': sha(audit_path),
           'canonical_and_intake': state, 'review_pass': False, 'publication_allowed': False,
           'goal': 'ACTIVE; all native C/C++, strict link, image and launch remain unproved',
           'created_utc': datetime.now(timezone.utc).isoformat()}
json_path = HERE / 'FROZEN_FULL_DIAGNOSTIC_R3.json'
md_path = HERE / 'FROZEN_FULL_DIAGNOSTIC_R3.md'
save(json_path, summary)
r2 = summary['versus_R2']
pub = summary['versus_published_wave7']
text = ('# Frozen R3 native diagnostic\n\n'
        f"Exact clean intake `{audit['head']}`; canonical remains published `{config['published_base']}`.\n\n"
        f"Actual full **{report['passed']} PASS / {report['failed']} FAIL / {report['requested']}**; compiler exit {terminal['exit_code']}. "
        f"R2: {r2['prior_passes_retained']} prior passes retained, {len(r2['new_passes'])} gains, {len(r2['regressions'])} regressions. "
        f"Published Wave7: {pub['prior_passes_retained']}/3400 retained, {len(pub['regressions'])} pass regressions; {len(pub['new_passes'])} gains.\n\n"
        f"Twenty original receipts, forty changed paths, four genuine new Mask4 headers, zero new CPPs. "
        f"All {summary['input_hashes']} source inputs and {summary['actual_artifact_hash_entries']} actual artifact hashes bound; "
        f"R2 {summary['R2_artifact_entries_verified']} and published {summary['published_artifact_entries_verified']} historical artifact entries verified. "
        f"External include files {summary['external_include_files']}, compiler/runtime files {summary['compiler_runtime_files']}, clean submodule pins {summary['clean_pinned_submodules']}.\n\n"
        f"Raw report `{audit['report']}` SHA256 `{audit['report_sha256']}`. "
        f"Terminal audit `{audit_path}` SHA256 `{sha(audit_path)}`.\n\n"
        'All raw failures and earlier negative witnesses remain immutable. Owner behavioral comparisons are bounded by their exact receipts, not whole-class or hardware proof. '
        'DeformUpdate, later shader-pointer cleanup, RCE containment/Render, WorldBoot, Corona/Curves/buffers, Volume and audio/DAC remain excluded. '
        'No formal review or publication has occurred. The native goal remains active; strict link/image/launch are unproved.\n')
with md_path.open('x', encoding='utf-8') as stream:
    stream.write(text)
completion = {'head': audit['head'], 'compiler_session': args.compiler_session, 'compiler_exit': terminal['exit_code'],
              'binder_session': args.binder_session, 'binder_exit': 0, 'raw_sha256': audit['report_sha256'],
              'audit_sha256': sha(audit_path), 'summary_sha256': sha(json_path), 'summary_md_sha256': sha(md_path),
              'canonical_and_intake': state, 'goal': 'ACTIVE', 'no_publication': True}
save(HERE / 'full-stage1-completion.json', completion)
print(json.dumps({k: summary[k] for k in ('head', 'requested', 'passed', 'failed', 'input_hashes', 'actual_artifact_hash_entries')}, indent=2))
print(json.dumps(completion, indent=2))
