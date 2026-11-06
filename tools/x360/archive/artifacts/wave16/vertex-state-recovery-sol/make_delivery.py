import datetime
import hashlib
import json
from pathlib import Path
import re
import subprocess


ARTIFACT = Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave16/vertex-state-recovery-sol')
WORKTREE = Path('D:/Reverse/BP-X360-Experimental-Worktrees/cxx03_vec2_five_luna_04ed')
WORKFLOW = Path('D:/Reverse/BP-Decomp_Workflow')
SDK = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/build/x360/sdk-6534.4/portable/XDK')


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def git(*args):
    return subprocess.check_output(['git', '-C', str(WORKTREE), *args], text=True).strip()


def bind(path):
    p = Path(path)
    return {'path': str(p), 'sha256': sha(p), 'length': p.stat().st_size}


pre = json.loads((ARTIFACT / 'PREIMAGES.json').read_text())
frozen = json.loads((ARTIFACT / 'FROZEN_CONTEXT_BEFORE_gate2.json').read_text())
comparison = json.loads((ARTIFACT / 'FROZEN_CONTEXT_COMPARISON_gate2.json').read_text())
assert comparison['changed'] == {}
head = git('rev-parse', 'HEAD')
assert git('rev-parse', 'HEAD^') == pre['baseline']
assert git('status', '--porcelain') == ''
changed = git('diff-tree', '--no-commit-id', '--name-only', '-r', 'HEAD').splitlines()
assert sorted(changed) == sorted(r['path'] for r in pre['paths'])
sources = []
for row in pre['paths']:
    path = WORKTREE / row['path']
    assert frozen[str(path.resolve())] == sha(path)
    sources.append({**row, 'post_raw_sha256': sha(path), 'post_git_blob': git('rev-parse', 'HEAD:' + row['path'])})

report = json.loads((ARTIFACT / 'gate2/compile-results.json').read_text())
assert report['state'] == 'complete' and report['passed'] == 7 and report['failed'] == 0
results = []
diagnostics = []
for row in report['results']:
    source = WORKTREE / row['source']
    assert frozen[str(source.resolve())] == sha(source)
    text = Path(row['log']).read_text(encoding='utf-8')
    diagnostics += [x for x in text.splitlines() if re.search(r'\b(?:warning|error) [A-Z]\d+', x)]
    results.append({**row, 'source_raw_sha256': sha(source),
                    'object_sha256': sha(row['object']), 'assembly_sha256': sha(row['assembly']),
                    'log_sha256': sha(row['log'])})
assert not diagnostics
includes = json.loads((ARTIFACT / 'RESOLVED_INCLUDES_FINAL.json').read_text())
for row in includes:
    for entry in row['includes']:
        assert frozen[entry['path']] == entry['sha256'] == sha(entry['path'])
for row in includes:
    if any(x in row['source'] for x in ('CgsOcclusionCullManager', 'CgsIm3d.cpp', 'BrnPostFxShader')):
        assert any(x['path'].endswith('Dispatch\\shadowingdevice.h') for x in row['includes'])

primary_paths = [WORKFLOW / '.ida-exports/BURNOUT_X360_ARTIST.XEX' / ('0x' + a + '.json')
                 for a in ['82B629C0', '82B60B88', '827E7998', '827E7A10', '827ED6E0', '82B63BA8', '82B62478', '82B634B0', '82B60730']]
primary_paths += [WORKFLOW / '.ida-exports/DecFIGS_Burnout_Internal_PS3.ELF' / ('0x' + a + '.json')
                  for a in ['E0039C', 'E06420', 'DFD528', '821F7C', 'AAF664']]
primary_paths += [WORKFLOW / 'references/DecFIGS/dwarfdump' / p for p in [
    'GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.cpp',
    'SDKs/EATech/include/ps3/gcm/renderengine/states.h',
    'SDKs/EATech/include/ps3/gcm/renderengine/stateparams.h',
    'SDKs/EATech/include/cmn/renderengine/statebuffer.h',
]]
primary_paths += [WORKFLOW / 'tools/re/x360rd.py', WORKFLOW / 'tools/re/findinit.py',
                  Path('D:/Reverse/IDA_Files/artist_i64.raw'), SDK / 'include/xbox/d3d9.h', SDK / 'include/xbox/winnt.h']

preserved = [
    'src/SDKs/RenderEngineClub/MAIN/components/src/indexbuffer.cpp',
    'src/SDKs/RenderEngineClub/MAIN/components/src/indexbuffer.h',
    'src/SDKs/RenderEngineClub/MAIN/components/src/device.cpp',
    'src/SDKs/RenderEngineClub/MAIN/components/src/device.h',
]
assert git('diff', pre['baseline'], 'HEAD', '--', *preserved) == ''
compiler = [bind(SDK / 'bin/win32' / n) for n in ['cl.exe', 'c1xx.dll', 'c1.dll', 'c2.dll']
            if (SDK / 'bin/win32' / n).is_file()]
artifact_names = [
    'PREIMAGES.json', 'RECOVERY_MODEL_PROPOSED.json', 'ARTIST_ID0_NODES.json',
    'CLOSED_USE_ID0_XREFS.json', 'NATIVE_DATA_SCAN.json', 'ADDRESS_ARITHMETIC_SCAN.json',
    'ID1_ITEM_FLAGS.json', 'IDB_NAME_OCCURRENCES.json',
    'FROZEN_CONTEXT_BEFORE.json', 'FROZEN_CONTEXT_AFTER.json', 'FROZEN_CONTEXT_COMPARISON.json',
    'FROZEN_CONTEXT_BEFORE_gate2.json', 'FROZEN_CONTEXT_AFTER_gate2.json', 'FROZEN_CONTEXT_COMPARISON_gate2.json',
    'NATIVE_INVENTORY_DELTA.json', 'NATIVE_INVENTORY_DELTA_gate2.json',
    'GATE_COMMAND.json', 'GATE_COMMAND_gate2.json', 'gate1-console.log', 'gate2-console.log',
    'gate1/compile-results.json', 'gate2/compile-results.json',
    'COFF_NATIVE_COMPARISON.json', 'RESOLVED_INCLUDES_FINAL.json',
    'run_selected_gate.py', 'audit_candidate.py', 'make_delivery.py',
]
delivery = {
    'created_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
    'state': 'narrow committed candidate; not integrated or published',
    'worktree': str(WORKTREE), 'branch': git('branch', '--show-current'),
    'baseline': pre['baseline'], 'commit': head, 'worktree_clean': True,
    'preserved_r18_graphics_branch': {'ref': 'codex/experimental-x360-r18-native-graphics',
                                     'head': git('rev-parse', 'codex/experimental-x360-r18-native-graphics')},
    'source_changes': sources,
    'recovered_contract': {
        'scope': 'the complete linked ARTIST VertexProgramState NoBind use chain',
        'opaque_api': 'VertexProgramState handle; original class size is never used for allocation',
        'storage_owner': 'shadow::VertexProgramStateData char maData[] and genuine rw::Resource',
        'minimum_accessed_capacity': '0x28 bytes, not an original full sizeof/alignment claim',
        'untouched_prefix': 'NoBind binder/getter do not read/write payload0x00..0x1F',
        'pointer_types': 'Parameters carries const ProgramBufferData* then const VertexDescriptorData*; canonical native data-provider names correspond to original ProgramBuffer*/VertexDescriptor* semantics',
        'state_word_order': 'program at0x24 then descriptor at0x20; exact original instruction order',
        'shader_return': 'real SDK D3DVertexShader* at program+0x14, not a loaded shader pointer',
        'resource': 'five native void* lanes; actual sizeof0x14; namespace global and CRT constructor',
        'no_forced_alignment': True, 'no_invented_prefix_fields': True,
        'no_pointer_array_surrogate': True, 'no_pc_provider': True,
    },
    'closed_use_proof': {
        'payload_global': '83010920 has one ID0 dataref827ED700; native address arithmetic scan agrees',
        'resource_global': '83011118 refs are GetState/Initialize/startup only; all five words inspected',
        'initializer': '82B629C0 only caller827E79C4; no function datarefs/indirect-call escape',
        'getter': '82B60B88 only caller827E7A78; no function datarefs/indirect-call escape',
        'getstate': '827E7998 only call827E7A48; extra821D7A48 ref is pdata FuncStart/FuncLen29 metadata',
        'current_state': '832716C8 has only two write refs827E7A90/82B63D08 and no reads',
        'larger_live_extent': 'no linked allocation/copy/descriptor operation depends on a larger logical extent',
        'original_unused_apis': 'PS3 state factories/serializers corroborate ownership, not target layout; no new normal Initialize/Release/serialization bodies invented',
        'metadata_reader': 'read-only python-idb ID0(raw,8), validate true,3368626 records; no IDA database writes',
    },
    'native_gate': {'requested': 7, 'passed': 7, 'failed': 0, 'warnings': 0, 'errors': 0,
                    'jobs': 1, 'timeout_per_source_seconds': 180, 'show_includes': True,
                    'all_nine_canonical_vendor_fallbacks': True,
                    'frozen_context_files': 7979, 'frozen_context_changes': 0,
                    'unique_resolved_include_paths': 164, 'resolved_include_bindings': 1016,
                    'outputs_and_temp_on_D': True, 'results': results},
    'byte_comparison': json.loads((ARTIFACT / 'COFF_NATIVE_COMPARISON.json').read_text()),
    'native_inventory_delta': {k: v for k, v in json.loads((ARTIFACT / 'NATIVE_INVENTORY_DELTA_gate2.json').read_text()).items()
                               if k != 'candidate_paths'},
    'coverage_accounting': 'one prior failure repaired (shadowingdevice.cpp) plus one genuinely new provider input; five unchanged selected consumers/providers remain passing. This is not a global sweep or a two-old-failure gain.',
    'superseded_gate1': '7/7 passed but binder memcpy lowered to byte operations. Final gate2 refines only its word transport view and verifies exact6-instruction native binder. All gate1 artifacts remain intact.',
    'preserved_native_index_device_paths': [{ 'path': p, 'sha256': sha(WORKTREE / p), 'git_blob': git('rev-parse', 'HEAD:' + p)} for p in preserved],
    'primary_evidence': [bind(p) for p in primary_paths], 'compiler': compiler,
    'artifacts': [bind(ARTIFACT / p) for p in artifact_names],
    'limits': [
        'Original full VertexProgramState sizeof/alignment and meanings of unused prefix bytes remain unproved; no claim of recovery for those private details.',
        'The observed0x30 image span to the next live global is not used as an original array-size proof; capacity0x28 is the complete linked NoBind minimum contract.',
        'Resource CRT constructor matches all five zero stores; target additionally repeats lane0zero before return. Its ctor is semantically equivalent, not byte-exact.',
        'Device::Initialize comparison masks only real COFF relocation operands; final relocated addresses are not claimed identical.',
        'COFF exact method bytes are not proof of final linked game image matching, native runtime behavior, full shadowingdevice parity, full affected-header integration, full link, executable/XEX creation or launch.',
        'Unrelated retained source/declaration/numeric/ABI debts in the selected sources were not re-audited or expanded by this repair.',
        'No canonical/intake/main/dev/ledger/publication edits or other-chat actions performed.',
    ],
}
(ARTIFACT / 'DELIVERY.json').write_text(json.dumps(delivery, indent=2), encoding='utf-8')
print(json.dumps({'commit': head, 'branch': delivery['branch'], 'worktree_clean': True,
                  'delivery_sha256': sha(ARTIFACT / 'DELIVERY.json'),
                  'passed': 7, 'failed': 0, 'native_inventory': '3617 -> 3618'}, indent=2))
