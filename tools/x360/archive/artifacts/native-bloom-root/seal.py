from pathlib import Path
import ast, hashlib, json, re, struct, subprocess

HERE = Path(__file__).resolve().parent
ROOT = Path('D:/Reverse/BP-X360-Experimental-Worktrees/drive-thru-native-root')
sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
git = lambda *args: subprocess.check_output(['git', *args], cwd=ROOT, text=True).strip()
head = git('rev-parse', 'HEAD')
assert head == 'be0c898d42d50191f0bd80ace3a848c0eea04835'
owned = ['src/GameSource/Graphics/PostFx/BrnPostFxBloom.cpp', 'src/GameSource/Graphics/PostFx/BrnPostFxBloom.h']
assert sorted(git('diff-tree', '--no-commit-id', '--name-only', '-r', head).splitlines()) == sorted(owned)
assert 'Co-Authored-By: Codex GPT-6.1 Sol <noreply@openai.com>' in git('log', '-1', '--format=%B')
assert git('status', '--short') == '?? tests/x360_drive_thru_layout_probe.cpp'
bindings = {}
for stage, expected in [('native-final', 6), ('program-prereqs', 2)]:
    report_path = HERE / stage / 'compile-results.json'
    report = json.loads(report_path.read_text())
    binding_path = HERE / stage / 'source-object-binding.json'
    binding = json.loads(binding_path.read_text())
    assert report['passed'] == expected and report['failed'] == 0
    assert binding['report_sha256'] == sha(report_path)
    for name, digest in binding['sources'].items(): assert sha(ROOT / name) == digest, name
    for name, digest in binding['actual_headers'].items(): assert sha(name) == digest, name
    for row in report['results']:
        assert struct.unpack_from('<H', Path(row['object']).read_bytes())[0] == 0x1F2
    bindings[stage] = {'report': str(report_path), 'report_sha256': sha(report_path),
        'binding': str(binding_path), 'binding_sha256': sha(binding_path),
        'whole_cpp_passes': expected, 'actual_headers': len(binding['actual_headers'])}
comparisons = {}
for name, count in [('constructor-final-comparison.json', 128), ('passes-final-comparison.json', 416)]:
    p = HERE / name; d = json.loads(p.read_text())
    assert d['cases'] == count and not d['mismatches']
    if 'objects' in d:
        for obj, digest in d['objects'].items(): assert sha(obj) == digest
    else: assert sha(d['object']) == d['object_sha256']
    verifier = HERE / ('validate_constructor_final.py' if count == 128 else 'validate_passes_final.py')
    assert sha(verifier) == d['verifier_sha256']
    comparisons[name] = {'path': str(p), 'sha256': sha(p), 'cases': count, 'limits': d['limits']}
program_receipt = Path('D:/Reverse/programbuffer-terra-build/final2/RECEIPT.json')
assert program_receipt.is_file()
baseline_path = HERE / 'baseline/compile-results.json'
baseline = json.loads(baseline_path.read_text())
assert baseline['passed'] == 5 and baseline['failed'] == 1
exports = Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')
raw_exports = {}
for address in ['82401628', '82401AE8', '82401F50', '82402308', '82402B40']:
    p = exports / ('0x' + address + '.json'); d = json.loads(p.read_text())
    raw_exports[d['name']] = {'address': d['address'], 'path': str(p), 'sha256': sha(p)}
source_bindings = {name: {'sha256': sha(ROOT/name), 'git_blob': git('rev-parse', head+':'+name)} for name in owned}
baseline_rows = [{'source': row['source'], 'passed': row['passed'], 'log': row['log'], 'log_sha256': sha(row['log'])} for row in baseline['results']]
delivery = {'state': 'SEALED_PRIVATE_CANDIDATE_NOT_COLLECTED_OR_PUBLISHED', 'commit': head,
    'parent': git('rev-parse', head+'^'), 'branch': git('branch', '--show-current'),
    'changed_paths': source_bindings, 'prerequisite': {'original_commit': 'f17811c21a2033d2fc590409b35c6576b72003be',
        'private_alias': '57f106c6f96d714a9ccdbb4612d10a1cc05273a2',
        'receipt': str(program_receipt), 'receipt_sha256': sha(program_receipt),
        'scope': 'Native ProgramBuffer five-lane rw::Resource API; migrate this prerequisite separately before Bloom. Other older Root prerequisites already recorded in earlier receipts.'},
    'baseline': {'report': str(baseline_path), 'sha256': sha(baseline_path), 'passed': 5, 'failed': 1, 'rows': baseline_rows},
    'gates': bindings, 'comparisons': comparisons, 'total_bounded_cases': 544,
    'original_exports': raw_exports,
    'changes': ['Remove PC shader-size externs, refusal/report early returns, invented null CreateProgram and wrong D3D redeclarations.',
        'Restore six original shader packages through real PfxHelper::CreateProgram, native allocator and shader lookup order.',
        'Restore genuine shader-state iterators and SDK FLOAT3/FLOAT2 strip vertices, half-texel UV math, exact downsample and Gaussian constants.',
        'Keep native size78/descriptor18/mode1C/program20,3C,50/threshold70 and vertex20 guards. Preserve the original one-pass reuse of downsample handles28/2C.'],
    'limits': ['Software-model original/object comparisons with external allocation, descriptor/init, handle lookup, render-target, constant destination and D3D boundaries controlled.',
        'Original shadow caches are prebound and product setters controlled; no full device-shadow/GPU state proof.',
        'Finite thresholds/white levels and nonzero valid/equal blur dimensions. No failure-assert path proof; source file/line diagnostic identity not certified.',
        'Classic stvewx implemented privately using effective-address word selection and big-endian aligned store; shared fixture unchanged. No physical FP/status proof.',
        'Shader package byte identity is data evidence, not a function-code or whole-build byte-match percentage.',
        'No whole-frame/game/strict link/image/launch result; R4 excludes this commit and Program prerequisite. Canonical/main/dev/origin/ledger and parent index untouched.'],
    'preserved_untracked_probe': {'path': 'tests/x360_drive_thru_layout_probe.cpp', 'sha256': sha(ROOT/'tests/x360_drive_thru_layout_probe.cpp')},
    'fixture_negative': {'path': str(HERE/'constructor-fixture-negative.json'), 'sha256': sha(HERE/'constructor-fixture-negative.json')},
    'seal_script_sha256': sha(__file__)}
p = HERE/'DELIVERY.json'; assert not p.exists();p.write_text(json.dumps(delivery,indent=2)+'\n',encoding='utf-8')
files = {}
for stage in ['baseline', 'native-candidate-1', 'program-prereqs', 'native-final']:
    for q in (HERE/stage).rglob('*'):
        if q.is_file() and q.suffix in ('.json', '.log', '.obj', '.asm'): files[str(q)] = sha(q)
for q in HERE.iterdir():
    if q.is_file() and q.suffix in ('.py', '.json', '.txt', '.md'): files[str(q)] = sha(q)
m = HERE/'artifact-manifest.json'; assert not m.exists();m.write_text(json.dumps({'files':files},indent=2)+'\n',encoding='utf-8')
print(json.dumps({'commit':head,'delivery_sha256':sha(p),'artifact_manifest_sha256':sha(m),'files':len(files),'bounded_cases':544}))
