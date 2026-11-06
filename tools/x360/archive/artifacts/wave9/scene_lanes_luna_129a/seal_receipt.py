from pathlib import Path
import hashlib
import json
import subprocess

REPO = Path(r'D:\Reverse\BP-X360-Experimental-Worktrees\scene_lanes_luna_129a')
ART = Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave9\scene_lanes_luna_129a')
BASE = '129afef95856e383366a55fb8f30a66ceb74de42'
SOURCES = [
    'src/GameShared/GameClasses/SceneManager/CgsEntityManager.cpp',
    'src/GameShared/GameClasses/SceneManager/CgsSceneManagerBridgeFunctions.cpp',
    'src/GameShared/GameClasses/SceneManager/CgsSceneManagerContact.cpp',
    'src/GameShared/GameClasses/Network/Debug Components/CgsNetworkPlayerManagerDebugComponent.cpp',
    'src/GameShared/GameClasses/Graphics/CgsMaterialAssembly.cpp',
]

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def git(*args):
    return subprocess.check_output(['git', *args], cwd=REPO, text=True).strip()
def git_bytes(*args):
    return subprocess.check_output(['git', *args], cwd=REPO)

commit = git('rev-parse', 'HEAD')
assert git('rev-parse', 'HEAD^') == BASE
assert git('branch', '--show-current') == 'codex/experimental-x360-scene-lanes-luna'
assert not git('status', '--porcelain'), 'worktree is not clean after commit'
assert set(git('diff-tree', '--no-commit-id', '--name-only', '-r', 'HEAD').splitlines()) == set(SOURCES)
message = git('show', '-s', '--format=%B', 'HEAD')
assert 'Co-Authored-By: Codex GPT-6 Luna <noreply@openai.com>' in message
binding_path = ART / 'native-gate-r1/source-object-binding.json'
binding = json.loads(binding_path.read_text(encoding='utf-8'))
compile_path = ART / 'native-gate-r1/compile-results.json'
compile_report = json.loads(compile_path.read_text(encoding='utf-8'))
negative_path = ART / 'wave8-negative-evidence.json'
negative = json.loads(negative_path.read_text(encoding='utf-8'))
assert binding['head'] == BASE and len(binding['actual_headers']) == 296
assert compile_report['passed'] == 5 and compile_report['failed'] == 0
assert all(row['passed'] and row['machine'] == 0x1F2 for row in compile_report['results'])
assert len(binding['objects']) == len(SOURCES)

def artifact_ref(path):
    return {'path': str(path), 'sha256': sha(path)}

source_rows = []
for source, negative_row in zip(SOURCES, negative['rows']):
    worktree_hash = sha(REPO / source)
    assert worktree_hash == binding['sources_after_sha256'][source]
    base_blob = git('rev-parse', f'{BASE}:{source}')
    commit_blob = git('rev-parse', f'{commit}:{source}')
    base_checkout = git_bytes('show', f'{BASE}:{source}').replace(b'\n', b'\r\n')
    assert hashlib.sha256(base_checkout).hexdigest() == negative_row['baseline_checkout_sha256']
    source_rows.append({
        'path': source,
        'base_blob': base_blob,
        'base_checkout_sha256': negative_row['baseline_checkout_sha256'],
        'committed_blob': commit_blob,
        'post_commit_worktree_sha256': worktree_hash,
        'gate_pre_compile_sha256': binding['sources_before_sha256'][source],
        'gate_post_compile_sha256': binding['sources_after_sha256'][source],
        'wave8_diagnostics': negative_row['diagnostics_count'],
        'wave8_log_sha256': negative_row['log_sha256'],
    })
compiler = Path(compile_report['results'][0]['command'][0])
receipt = {
    'task': 'five bounded native vector-lane C++03 fixes; private Xbox 360 experiment follow-up, not collected into R8',
    'repository': str(REPO),
    'branch': git('branch', '--show-current'),
    'parent': BASE,
    'commit': commit,
    'commit_subject': git('show', '-s', '--format=%s', 'HEAD'),
    'coauthor_trailer': 'Co-Authored-By: Codex GPT-6 Luna <noreply@openai.com>',
    'scope': {'source_count': len(SOURCES), 'sources': source_rows, 'other_repo_paths_changed': []},
    'negative_gate': {
        'report': negative['source'],
        'report_sha256': negative['report_sha256'],
        'target': negative['target'],
        'head': negative['reported_head'],
        'diagnostics_by_source': {row['source']: row['diagnostics_count'] for row in negative['rows']},
        'evidence_file': artifact_ref(negative_path),
    },
    'positive_gate': {
        'stage': str(ART / 'native-gate-r1'),
        'compile_results': artifact_ref(compile_path),
        'driver_log': artifact_ref(ART / 'native-gate-r1/driver.log'),
        'command_record': artifact_ref(ART / 'native-gate-r1/command.json'),
        'binding_manifest': artifact_ref(binding_path),
        'runner': artifact_ref(ART / 'run_gate.py'),
        'compiler': {'path': str(compiler), 'sha256': sha(compiler)},
        'sdk': str(Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\build\x360\sdk-6534.4\portable\XDK')),
        'passed': compile_report['passed'], 'failed': compile_report['failed'],
        'object_machine': '0x1F2 PPC32 Xbox 360',
        'objects': binding['objects'],
        'actual_header_count': len(binding['actual_headers']),
        'actual_header_manifest_sha256': sha(binding_path),
    },
    'limits': [
        'compile-only gate for these five source files; no whole-program link or runtime proof',
        'only the five assigned C++ files were committed; headers and canonical/intake repositories were untouched',
        'private local commit only; no merge, push, publish, ledger update, or parent gitlink change',
        'later-only follow-up; not part of R8 unless separately collected',
    ],
}
out = ART / 'receipt.json'
out.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
print('commit', commit)
print('receipt', out)
print('receipt_sha256', sha(out))
print('headers', len(binding['actual_headers']), 'objects', len(binding['objects']), 'sources', len(source_rows))
print('compiler', compiler, sha(compiler))
