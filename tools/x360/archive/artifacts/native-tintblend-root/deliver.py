from pathlib import Path
import ast, hashlib, json, subprocess, sys

HERE = Path(__file__).resolve().parent
ROOT = Path('D:/Reverse/BP-X360-Experimental-Worktrees/drive-thru-native-root')
source = 'src/GameShared/Jobs/TintBlend/TintBlend.cpp'

def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])

head = git('rev-parse', 'HEAD').decode().strip()
parent = git('rev-parse', 'HEAD^').decode().strip()
assert parent == '9acbf432092522daf57bfc087b42c9a0f9375f46'
assert git('diff-tree', '--no-commit-id', '--name-only', '-r', head).decode().splitlines() == [source]
assert 'Co-Authored-By: Codex GPT-6.1 Sol <noreply@openai.com>' in git('log', '-1', '--format=%B').decode()
before = json.loads((HERE / 'source-final-before.json').read_text())['hashes']
after = json.loads((HERE / 'source-final-after.json').read_text())['hashes']
assert before == after and all(sha(ROOT / p) == h for p, h in after.items())
native = json.loads((HERE / 'native-final/compile-results.json').read_text())
assert native['passed'] == 2 and native['failed'] == 1
leaf = json.loads((HERE / 'leaf-final.json').read_text())
surface = json.loads((HERE / 'surface-full.json').read_text())
assert leaf['cases'] == 1280 and surface['cases'] == 44
assert not leaf['mismatches'] and not surface['mismatches']
bindings = json.loads((HERE / 'actual-build-bindings.json').read_text())
assert all(sha(p) == h for p, h in bindings['actual_headers'].items())
assert all(sha(p) == h for p, h in bindings['compiler_and_runtime'].items())

# Hash the actual model dependencies without invoking their test drivers.
script = HERE / 'validate_native.py'
nodes = []
for node in ast.parse(script.read_text()).body:
    if isinstance(node, ast.Assert):
        break
    nodes.append(node)
exec(compile(ast.Module(body=nodes, type_ignores=[]), str(script), 'exec'), globals())
fixture_inputs = {}
for name in ('native_atomic_fixture', 'validate_native_strings', 'validate_native_math',
             'validate_render_states', 'validate_resource_pointer', 'vmxemu', 'emu64',
             'sincos_emu', 'atc_update_emu', 'scalarsincos'):
    module = sys.modules.get(name)
    if module is not None:
        p = Path(module.__file__)
    else:
        p = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk') / (name + '.py')
    assert p.is_file(), (name, p)
    fixture_inputs[str(p)] = sha(p)

exports = Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')
original_exports = {}
for address in ('82AD2CE8', '82AD2F38', '82AD3250', '82AD3668', '82AD3AF8',
                '82AD4078', '82AD4170', '82AD4280', '82AD43D0', '82AD4528', '82AD46B0', '82AD4860'):
    p = exports / ('0x' + address + '.json')
    original_exports[str(p)] = sha(p)

evidence_names = ['leaf-final.json', 'surface-full.json', 'surface-smoke.json',
    'formatting-only.json', 'formatting-retention.json', 'dispatcher-instruction-match.json',
    'old-clamp-negative-witness.json', 'actual-build-bindings.json', 'POSTMORTEM.md',
    'native-final/compile-results.json', 'source-final-before.json', 'source-final-after.json',
    'final-command.json', 'final-exit.json', 'validate_native.py', 'validate_surface.py',
    'seal_compile.py', 'seal_evidence.py', 'deliver.py']
evidence = {str(HERE / p): sha(HERE / p) for p in evidence_names}
receipt = {'base': parent, 'head': head,
    'sources': {source: sha(ROOT / source)},
    'git_blobs': {source: git('rev-parse', head + ':' + source).decode().strip()},
    'patch_sha256': hashlib.sha256(git('diff', parent, head, '--binary', '--', source)).hexdigest(),
    'native_report': str(HERE / 'native-final/compile-results.json'),
    'native_report_sha256': sha(HERE / 'native-final/compile-results.json'),
    'passed': native['passed'], 'failed': native['failed'],
    'unchanged_published_failure': 'BrnPostFx.cpp: missing mpColourTextureState plus undeclared lpColourCube and resulting GetPixels error; all three diagnostics identical to published 3534 full sweep.',
    'stable_tracked_code_inputs': len(before),
    'source_before_sha256': sha(HERE / 'source-final-before.json'),
    'source_after_sha256': sha(HERE / 'source-final-after.json'),
    'actual_headers': bindings['actual_headers'], 'header_count': bindings['header_count'],
    'compiler_and_runtime': bindings['compiler_and_runtime'], 'artifacts': bindings['artifacts'],
    'original_exports': original_exports, 'original_leaf_raw_bindings': leaf['functions'],
    'fixture_inputs': fixture_inputs, 'evidence': evidence,
    'positive_comparison_cases': 1324, 'positive_groups': {'actual_helpers': 1280, 'actual_surfaces_entry_dispatch': 44},
    'negative_control_cases': 1,
    'scope': 'One native job implementation; no new native CPP, shared-header, PC-folder, integration-branch, workflow-ledger or canonical/intake mutation.',
    'instruction_match': 'Only dispatcher: seven instructions/28 bytes after two genuine table-address relocation immediates are normalized.',
    'limits': 'Inert PC files among6450 inputs are provenance only. Actual whole CPPs2/3 (one baseline failure). Software VMX/finite FP/no-alias buffers; CRT nonvolatile-save convention controlled, unsigned conversion private extension, no exceptional weights/full VSCR/full register preservation. Source EOF cleanup is rebound by13 identical actual relocated sections4672bytes, preserving original comparison bindings. Helpers emit more instructions than ARTIST and entry is inlined, so no performance/whole-byte/full-ABI claim. No fresh global full sweep, strict game link, GPU/hardware/image/launch claim.',
    'verifier_sha256': sha(__file__)}
with (HERE / 'DELIVERY.json').open('x', encoding='utf-8') as f:
    json.dump(receipt, f, indent=2)
print(json.dumps({'head': head, 'receipt_sha256': sha(HERE / 'DELIVERY.json'),
                  'passed': native['passed'], 'failed': native['failed'],
                  'comparisons': 1324, 'headers': bindings['header_count']}, indent=2))
