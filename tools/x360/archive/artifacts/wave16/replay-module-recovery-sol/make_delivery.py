import datetime
import hashlib
import json
from pathlib import Path
import re
import subprocess

OUT = Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave16/replay-module-recovery-sol')
OWN = Path('D:/Reverse/BP-X360-Experimental-Worktrees/device_storage_terra')
ICE = Path('D:/Reverse/BP-X360-Experimental-Worktrees/cxx03_five_luna_04ed')
WORKFLOW = Path('D:/Reverse/BP-Decomp_Workflow')
SDK = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/build/x360/sdk-6534.4/portable/XDK')
ICE_HEAD = 'ba05880df217c6c75ec63b7a7534b6c2f9c31121'
HEADER = 'src/GameSource/Replays/BrnReplayModule.h'


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args], text=True).strip()


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def bind(path):
    p = Path(path)
    return {'path': str(p), 'sha256': sha(p), 'length': p.stat().st_size}


pre = json.loads((OUT / 'PREIMAGES.json').read_text())
proposal = json.loads((OUT / 'REPLAY_STORAGE_PROPOSAL.json').read_text())
assert git(OWN, 'rev-parse', 'HEAD^') == pre['baseline']
assert git(OWN, 'status', '--porcelain') == ''
assert git(ICE, 'rev-parse', 'HEAD') == ICE_HEAD and git(ICE, 'status', '--porcelain') == ''
changed = git(OWN, 'diff-tree', '--no-commit-id', '--name-only', '-r', 'HEAD').splitlines()
assert sorted(changed) == sorted(r['path'] for r in pre['paths'])
sources = [{**r, 'post_raw_sha256': sha(OWN / r['path']), 'post_git_blob': git(OWN, 'rev-parse', 'HEAD:' + r['path'])}
           for r in pre['paths']]
gates = []
for folder, root in [('attempt03-replay-native-final', OWN), ('attempt04-typed-game-overlay-final', ICE)]:
    report = json.loads((OUT / folder / 'compile-results.json').read_text())
    frozen = json.loads((OUT / folder / 'context-before.json').read_text())
    check = json.loads((OUT / folder / 'context-check.json').read_text())
    assert check['changed'] == {} and report['failed'] == 0
    rows = []
    diagnostics = []
    for row in report['results']:
        assert row['machine'] == 0x1f2 and row['source_sha256'] == sha(root / row['source'])
        assert frozen[str((root / row['source']).resolve())] == row['source_sha256']
        text = Path(row['log']).read_text(encoding='utf-8')
        diagnostics += [l for l in text.splitlines() if re.search(r'\b(?:warning|error) [A-Z]\d+', l)]
        rows.append({**row, 'object_sha256': sha(row['object']), 'assembly_sha256': sha(row['assembly']),
                     'log_sha256': sha(row['log'])})
    assert not diagnostics
    includes = json.loads((OUT / folder / 'showIncludes-bindings.json').read_text())
    unique = {r['path']: r['sha256'] for b in includes for r in b['includes']}
    for p, h in unique.items():
        assert sha(p) == h == frozen[p]
        assert not any(x.lower() == 'pc' for x in Path(p).parts)
    gates.append({'context': folder, 'source_root': str(root), 'source_root_head': report['source_root_head'],
                  'jobs': 1, 'passed': report['passed'], 'failed': 0, 'warnings': 0, 'errors': 0,
                  'all_nine_canonical_vendor_fallbacks': True, 'show_includes': True,
                  'frozen_files': check['files'], 'frozen_changes': 0,
                  'unique_include_paths': len(unique), 'include_bindings': sum(len(b['includes']) for b in includes),
                  'results': rows})
overlay_binding = json.loads((OUT / 'attempt04-typed-game-overlay-final/overlay-binding.json').read_text())
assert overlay_binding['source_header_sha256'] == sha(OWN / HEADER)
assert overlay_binding['used'] and not overlay_binding['stale_original_header_bindings']
assert sha(OUT / 'typed-game-overlay-final/GameSource/Replays/BrnReplayModule.h') == sha(OWN / HEADER)
layout_log = (OUT / 'attempt04-typed-game-overlay-final/BrnGameModule-f766a093.log').read_text()
assert 'class BrnGameModule\tsize(10097408):' in layout_log
assert re.search(r'9163520\s+\| ReplayModule mReplayModule', layout_log)
assert re.search(r'9188864\s+\| BrnNetworkModule mNetworkModule', layout_log)

def native_inventory(names):
    return sorted(p for p in names if p.startswith(('src/', 'vendor/')) and Path(p).suffix.lower() in ('.cpp', '.c')
                  and not any(x.lower() == 'pc' for x in Path(p).parts))

before_inventory = native_inventory(git(OWN, 'ls-tree', '-r', '--name-only', pre['baseline']).splitlines())
after_inventory = native_inventory(git(OWN, 'ls-files', '*.cpp', '*.c').splitlines())
assert before_inventory == after_inventory
primary = [WORKFLOW / '.ida-exports/BURNOUT_X360_ARTIST.XEX' / ('0x' + a + '.json') for a in [
    '823A8ED8', '827E60D8', '827E5F00', '827E03D0', '82652768', '82656E20', '8264B4A8',
    '8264B570', '8264E470', '8264B010', '8264E8F8', '8265F618', '826608A8', '82652878',
    '8264B400', '82867BE8', '82867790', '82652978', '8264F9F8',
    '8264BFF0', '8264C000', '8264C010', '8264C020', '8264C030', '8264C040']]
primary += [WORKFLOW / 'references/DecFIGS/dwarfdump' / p for p in [
    'GameSource/Replays/BrnReplayModule.h',
    'GameShared/GameClasses/Module/CgsEventReceiverQueue.h',
    'GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamProducer.h']]
primary += [WORKFLOW / 'tools/re/x360rd.py', WORKFLOW / 'tools/re/findinit.py',
            Path('D:/Reverse/IDA_Files/artist_i64.raw'),
            ICE / 'src/GameSource/Game/BrnGameModule.hpp', ICE / 'src/GameSource/Game/BrnGameModule.cpp',
            OWN / 'src/GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamProducer.h',
            OWN / 'src/GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h',
            OWN / 'src/GameSource/Replays/BrnReplayReels.h']
artifacts = [
    'PREIMAGES.json', 'REPLAY_STORAGE_PROPOSAL.json', 'DIRECT_NATIVE_RANGE_ACCESSES.json',
    'RAW_WINDOW_CANDIDATES.json', 'RAW_WINDOW_CODE_CANDIDATES.json', 'DEBUG_ESCAPE_COMPONENT.json',
    'AFFINE_MODULE_VIEW_CANDIDATES.json', 'FINAL_NATIVE_LAYOUTS.json', 'FINAL_BINDING_AUDIT.json',
    'CONSTRUCTOR_STORAGE_AUDIT.json', 'scan_native_windows.py', 'trace_module_views.py',
    'run_replay_gates.py', 'make_delivery.py',
    'typed-game-overlay/GameSource/Replays/BrnReplayModule.h',
    'typed-game-overlay-final/GameSource/Replays/BrnReplayModule.h',
]
for folder in ['attempt01-replay-native', 'attempt02-typed-game-overlay',
               'attempt03-replay-native-final', 'attempt04-typed-game-overlay-final']:
    artifacts += [str(p.relative_to(OUT)) for p in (OUT / folder).glob('*.json')]
    artifacts += [str(p.relative_to(OUT)) for p in (OUT / folder).glob('*.log')]
delivery = {
    'created_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
    'state': 'stable narrow committed candidate; not integrated or published',
    'commit': git(OWN, 'rev-parse', 'HEAD'), 'parent': pre['baseline'],
    'branch': git(OWN, 'branch', '--show-current'), 'worktree': str(OWN), 'worktree_clean': True,
    'preserved_old_replay_branch': {'ref': 'codex/experimental-x360-r18-replays-lock-recovery',
                                    'head': git(OWN, 'rev-parse', 'codex/experimental-x360-r18-replays-lock-recovery')},
    'changed_paths': sources,
    'representation': {
        'windows': proposal['windows'],
        'meaning': 'bounded native lifetime byte-storage views; original private C++ types are not invented',
        'no_added_initialization': True, 'no_new_alignment_attributes': True, 'no_capacity_changes': True,
        'field_order': 'ARTIST secondary pointer910 before header position914; DWARF order does not override target',
        'active_anchors': proposal['expected_anchors'],
        'hard_guard_count': (OWN / HEADER).read_text().count('CGS_STATIC_ASSERT'),
        'native_counts': {'serialisers': 11, 'jobs': 4, 'job_stride': '0x350', 'reels': 6, 'reel_stride': '0x101'},
        'native_extent': 'DebugComponent last pointer+34 ends Replay629C; existing native128 child alignment rounds to6300; next Game Network begins8C3600, independently bounding Replay from8BD300 to6300 bytes',
    },
    'lifetime_and_escape_evidence': {
        'native_main_path': 'main allocates Game9A1300/align128, calls DebugMemoryInit before Game constructor; Game constructor calls Replay at8BD300 and Network8C3600',
        'initial_bytes': 'DebugMemoryInit823A8EE8..8EF8 zeroes the entire Game. Four subsequent debug pattern-fill ranges exclude Replay. This is not an arbitrary Replay-constructor zero guarantee.',
        'constructor_and_construct': 'No new writes to storage intervals; compiled direct module stores were checked. Native C++ ctor binds four350-stride jobs at4940, serialiseJob5680 and debug vptr6264. Existing GPU-stream ctor is out-of-line in compiled source versus inlined native; no full constructor byte match claimed.',
        'producer_escape': 'WaitForSerialiseJobs8264B4E0 and UpdatePlaying_PreSim82660E14 derive producer5A00, pass poster5A80 to End82867BE8, then clear module5B00; End/GetEncodedStatus82867790 touch poster0..7 and+28, below5B80',
        'debug_escape': 'DebugComponent::Construct82652998 stores actual Replay pointer at debug+0C. Debug methods/callbacks are included in47 exported-body candidate pass. OnActivate passes only module5C45 flag address; six callbacks write5C3F..44. No blanket no-escape assertion.',
        'reels_copy_bound': 'SetStatusInterface8264E470 passes module5C47+n*101 to SetReel8264B010 for exactly6 records; end624D exclusive, followed by natural3-byte alignment before record6250',
        'export_hole': 'RenderDebugHUD8264B9C0..8264BB68 raw trace reads/writes625C and reads920/928/92C; no window access observed in that bounded body',
        'scanner_limits': 'Linear affine candidate scan covers47 exported ReplayModule/DebugComponent bodies and reports0 window candidates; this is manual navigation/evidence, not formal CFG/no-alias proof. Whole-binary immediate candidates contain unrelated objects and are not treated as type attestation.',
        'inactive_names': 'DWARF waiting/last-record member names corroborate the small gap only; no guessed bool/int declarations were adopted',
    },
    'gates': gates,
    'combined_context': {
        'ice_checkout': str(ICE), 'ice_head': ICE_HEAD, 'ice_clean_unchanged': True,
        'typed_chain': ['b85657d5feb9147ddc379106605408ea92192d76', 'ef8c37c279e070814a59880ff1ea6bed31c6d148', ICE_HEAD],
        'only_overlay_path': 'GameSource/Replays/BrnReplayModule.h',
        'header_binding': overlay_binding, 'no_hidden_type_patches': True,
        'outer_guards_unchanged': True, 'measured_game_size': '0x9A1300',
        'measured_replay_offset': '0x8BD300', 'measured_network_offset': '0x8C3600',
        'scope': 'separate read-only combined-context compile, not an integration/intake result',
    },
    'inventory': {'before': len(before_inventory), 'after': len(after_inventory), 'added': [], 'removed': []},
    'coverage_accounting': 'Three already-passing native Replay sources remain passing. The stronger typed Game context now passes its native outer guards; no extra baseline compiler gain or global sweep is claimed.',
    'superseded_attempts': 'attempt01/02 diagnostics are retained. They failed only three bare nonstatic-array sizeof count expressions in a static guard under old XDK C++03. Final guards qualify unevaluated member expressions; no physical layout changes or guard bypass were used.',
    'compiler': [bind(SDK / 'bin/win32' / p) for p in ['cl.exe', 'c1xx.dll', 'c1.dll', 'c2.dll'] if (SDK / 'bin/win32' / p).is_file()],
    'primary_evidence': [bind(p) for p in primary if p.exists()],
    'artifacts': [bind(OUT / p) for p in sorted(set(artifacts))],
    'limits': [
        'Original private C++ type names/roles of the five inactive byte intervals remain unidentified. The representation preserves their native storage/lifetime contract without assigning new behavior.',
        'No formal proof excludes every possible arbitrary alias. Observed pointer escapes/helpers and scanner limits are explicit.',
        'No full ReplayModule/DebugComponent behavioral or numerical parity claim; unrelated retained source debt is outside this layout repair.',
        'No full affected-header integration sweep, game link, XEX generation, launch or runtime validation performed.',
        'No intake/canonical/main/dev/ledger/publication changes. ICE source checkout remains at the original clean ba058 commit.',
    ],
}
(OUT / 'DELIVERY.json').write_text(json.dumps(delivery, indent=2), encoding='utf-8')
print(json.dumps({'commit': delivery['commit'], 'delivery_sha256': sha(OUT / 'DELIVERY.json'),
                  'own_gate': '3/3', 'typed_overlay_gate': '1/1',
                  'ice_unchanged': True, 'worktree_clean': True, 'inventory': delivery['inventory']}, indent=2))
