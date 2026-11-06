from pathlib import Path
import hashlib, json, subprocess

HERE = Path(__file__).resolve().parent
ROOT = Path('D:/Reverse/BP-X360-Experimental-Worktrees/drive-thru-native-root')

def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])

head = git('rev-parse', 'HEAD').decode().strip()
base = git('rev-parse', 'HEAD^').decode().strip()
paths = git('diff-tree', '--no-commit-id', '--name-only', '-r', head).decode().splitlines()
assert head.startswith('6ad868d7c') and len(paths) == 6
assert 'Co-Authored-By: Codex GPT-6.1 Sol <noreply@openai.com>' in git('log', '-1', '--format=%B').decode()
before = json.loads((HERE / 'native-combined-source-before.json').read_text())['hashes']
after = json.loads((HERE / 'native-combined-source-after.json').read_text())['hashes']
assert before == after and all(sha(ROOT / p) == h for p, h in after.items())
report = json.loads((HERE / 'native-combined/compile-results.json').read_text())
assert report['passed'] == 12 and report['failed'] == 0
caller = json.loads((HERE / 'caller-combined.json').read_text())
half = json.loads((HERE / 'halftexel-window-comparison.json').read_text())
assert caller['cases'] == half['cases'] == 128 and not caller['mismatches'] and not half['mismatches']
headers, artifacts = {}, {}
for result in report['results']:
    assert '/pc/' not in result['source'].replace('\\', '/').lower()
    for key in ('object', 'assembly', 'log'):
        p = Path(result[key])
        artifacts[str(p)] = sha(p)
    for line in Path(result['log']).read_text(errors='replace').splitlines():
        if 'Note: including file:' in line:
            p = Path(line.split('Note: including file:', 1)[1].strip())
            assert p.is_file(), p
            headers[str(p)] = sha(p)
tools = json.loads(Path('D:/Reverse/BP-X360-Experimental-Artifacts/native-tintblend-root/DELIVERY.json').read_text())['compiler_and_runtime']
assert all(sha(p) == h for p, h in tools.items())
peer = Path('D:/Reverse/BP-X360-Experimental-Artifacts/rendertarget-terra-final2/RENDERTARGET_PRIVATE_HANDOFF.json')
hardware = Path('D:/Reverse/BP-X360-Experimental-Artifacts/jobmanager-provider-luna-3534/provider-evidence.json')
names = ['POSTMORTEM.md', 'before-edit.json', 'forward-declarations.json',
    'native-combined/compile-results.json', 'native-combined-source-before.json',
    'native-combined-source-after.json', 'native-combined-command.json', 'native-combined-exit.json',
    'caller-combined.json', 'halftexel-window-comparison.json', 'render-target-retention.json',
    'make_native.py', 'run_gate.py', 'validate_caller.py', 'validate_halftexel.py',
    'check_target_retention.py', 'deliver.py']
evidence = {str(HERE / name): sha(HERE / name) for name in names}
originals = {}
for p in [
    Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x823F8380.json'),
    Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x823F8310.json'),
    Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8240A468.json'),
    Path('D:/Reverse/BP-Decomp_Workflow/references/DecFIGS/dwarfdump/SDKs/RenderEngineClub/MAIN/components/include/postfx/rwgpfxcolourcube.h')]:
    originals[str(p)] = sha(p)
receipt = {'base': base, 'head': head,
    'sources': {p: sha(ROOT / p) for p in paths},
    'git_blobs': {p: git('rev-parse', head + ':' + p).decode().strip() for p in paths},
    'patch_sha256': hashlib.sha256(git('diff', base, head, '--binary', '--', *paths)).hexdigest(),
    'foreign_prerequisites': [
        {'original': '4c5c03ecbe757e18b0b1dd251fb6e880e79d394b', 'local': git('rev-parse', 'HEAD~2').decode().strip()},
        {'original': 'a0656568742b2332c23323b19a530039b24c472a', 'local': base}],
    'peer_handoff': str(peer), 'peer_handoff_sha256': sha(peer),
    'already_published_scheduler_prerequisites': ['6509cbd2818e0ce2f8ae799712954392f7642cdd', 'e591e57dc54e5aa6ceb2bd873e7fa23abd48f77e'],
    'hardware_provider_receipt': str(hardware), 'hardware_provider_receipt_sha256': sha(hardware),
    'native_report': str(HERE / 'native-combined/compile-results.json'),
    'passed': 12, 'failed': 0, 'stable_tracked_code_inputs': len(before),
    'source_before_sha256': sha(HERE / 'native-combined-source-before.json'),
    'source_after_sha256': sha(HERE / 'native-combined-source-after.json'),
    'actual_headers': headers, 'header_count': len(headers),
    'compiler_and_runtime': tools, 'artifacts': artifacts,
    'executed_comparison_cases': 256, 'groups': {'actual_caller_and_providers': 128, 'actual_halftexel_window': 128},
    'retained_peer_comparisons': 1344, 'retention': '152 identical runtime COFF sections/8477bytes plus named relocations across two foreign provider/caller objects; .debug$S differs and is explicitly excluded/hashed. This does not recount peer cases as fresh execution.',
    'original_evidence': originals, 'evidence': evidence,
    'scope': 'Six owned native game/resource declaration paths, with separate three-path foreign native RenderTarget provider/caller prerequisites. No PC compile inputs, new CPP, workflow ledger, main/dev, canonical or intake mutation.',
    'limits': 'Later-only candidate on pre-Wave8-Math private base. Requires a fresh frozen c0d true-Math consumer rebind before next collection;12CPP/256 cases are not current Wave8 full results. Software PPC/FP and CRT convention; GPU lock/width and AddJobs controlled; no worker concurrency, whole Render signature/shader ABI/frame, GPU/hardware, whole-game link, image, launch or whole-build byte claim. Inert PC source hashes are provenance only. Real published scheduler object is separately source/hash bound; old private HardwareInit source is not claimed repaired.',
    'verifier_sha256': sha(__file__)}
with (HERE / 'DELIVERY.json').open('x', encoding='utf-8') as f:
    json.dump(receipt, f, indent=2)
print(json.dumps({'head': head, 'receipt_sha256': sha(HERE / 'DELIVERY.json'), 'passed': 12,
                  'comparisons': 256, 'headers': len(headers)}, indent=2))
