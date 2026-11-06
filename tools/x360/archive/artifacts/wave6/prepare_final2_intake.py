"""Prepare only the stable-input CPP correction after the current full result is bound."""
import hashlib
import json
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
WORK = Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
COMMIT = '66c859286a4f7bd51069eed775eeefc747caee8d'
SPLICE = 'src/GameShared/GameClasses/Sound/Playback/Splicer/internal/SpliceObjects.cpp'


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def git(*args):
    return subprocess.check_output(['git', '--no-optional-locks', *args], cwd=WORK).decode().strip()


meta_path = HERE / 'native-parallel-wave6-final-patch.json'
meta = read(meta_path)
assert git('rev-parse', 'HEAD') == meta['head'] == 'd3fc771a05e4f1b3da4c10c53da557cd195d54f7'
assert not git('status', '--porcelain=v1')
report_path = HERE / 'repair-stage1/compile-results.json'
report = read(report_path)
assert report['state'] == 'complete' and report['requested'] == 3613
audit_path = HERE / 'repair-stage1-terminal-audit.json'
audit = read(audit_path)
assert audit['head'] == meta['head'] and audit['report_sha256'] == sha(report_path)
assert audit['metadata_sha256'] == sha(meta_path)
completion = read(HERE / 'final-binding-completion.json')
assert completion['head'] == meta['head'] and completion['actual_recompiled_sources'] == 3613
assert completion['summary']['prior_passes_retained'] == 3341
assert not completion['summary']['regressions'] and not completion['summary']['timeouts']
review_path = HERE / 'native-parallel-wave6-final-review-result.json'
review = read(review_path)
assert review['verdict'] == 'fail' and review['head'] == meta['head'] and review['sha256'] == meta['sha256']
assert sha(review['report']) == review['report_sha256']
receipt_path = HERE / 'wave6-receipts.json'
assert sha(receipt_path) == sha(HERE / 'final-wave6-receipts.json') == meta['receipt_sha256']
assert len(read(receipt_path)['receipts']) == 58
assert git('diff-tree', '--no-commit-id', '--name-only', '-r', COMMIT).splitlines() == [SPLICE]
assert git('rev-parse', COMMIT + ':' + SPLICE) == 'eabadb71ad2ea5cc9959d21fedcb1bca3492cb55'
note = Path('D:/Reverse/BP-Decomp_Workflow/scratch/parallel-native/splice_constructor_luna/followup/native-bindings.md')
assert sha(note) == '78563f0cb4666883897c76bc4265ae80146a19eeba7303c18258dcf2d92d66b0'
prior_plan = read(HERE / 'intake-plan-03.json')
plan = {
    'base': meta['base'], 'frozen_previous_full': meta['head'],
    'collect_now': prior_plan['collect_now'] + [COMMIT], 'successor_paths': [SPLICE],
    'new_commit': COMMIT, 'expected_total_receipts': 59,
    'preserved_58_receipts_sha256': sha(HERE / 'final-wave6-receipts.json'),
    'previous_full_metadata': str(meta_path), 'previous_full_metadata_sha256': sha(meta_path),
    'previous_full_report': str(report_path), 'previous_full_report_sha256': sha(report_path),
    'previous_full_terminal_audit': str(audit_path), 'previous_full_terminal_audit_sha256': sha(audit_path),
    'previous_failed_review': str(review_path), 'previous_failed_review_sha256': sha(review_path),
    'evidence': str(note), 'evidence_sha256': sha(note),
    'scope': 'Only W6-F3 stable original FindSplice pointer capture. Existing58 receipts remain an exact prefix. No header, API, storage or nextbatch change.',
    'native_gate': 'Exact affected CPP plus CgsSplicerContent and BrnDebugComponent; preserve complete3613 prior-full coverage with all source/header/external inputs invariant outside this CPP.',
    'limits': [
        'Actual-object equivalence to unchanged compiled data-pointer constructor, not a located standalone ARTIST constructor body.',
        'Original private d014 COFF bytes were overwritten by its author; replacement rebuild provenance is explicitly recorded. Root full b85a7be5 object and failing witness remain immutable.',
        'No full Sound lifecycle, strict-link, image, launch or physical audio claim.']}
with (HERE / 'intake-plan-04.json').open('x', encoding='utf-8') as stream:
    json.dump(plan, stream, indent=2)
source = (HERE / 'integrate_wave6_successor.py').read_text()
assert source.count('intake-plan-03.json') == 1
with (HERE / 'integrate_wave6_final2.py').open('x', encoding='utf-8') as stream:
    stream.write(source.replace('intake-plan-03.json', 'intake-plan-04.json'))
print('READY: closed final2 plan, one CPP, expected59 receipts; source has not changed.')
