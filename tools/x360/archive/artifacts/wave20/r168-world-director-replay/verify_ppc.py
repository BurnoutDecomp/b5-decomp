"""Verify the frozen R168 World-to-Director PPC and unchanged R167 slot-3 codec."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

OUT = Path(__file__).resolve().parent
WAVE = OUT.parent
REPO = Path('C:/Users/adri1/.codex/worktrees/bp360/pool_io_luna')
HEAD = 'c96a057e4d333084ea314e46ef7b1453c9dac4f5'
TREE = '41e8e461bfadad82ef98e59b4f39e7270d63dc31'
PARENT = '97b8171bb4712f809672cd02bc92b7ea6246735d'
PARENT_TREE = '7dc05451bc11983f3ba1f9819230f8c4a21f57e1'
ORIGINAL_REF = 'refs/heads/codex/r166-junkyard-setup-luna'
ORIGINAL_HEAD = 'd8fb9f5fbf3b956315948c43d88771e63baaea1d'
sys.path[:0] = [str(WAVE / 'r161-native-inputpads'), 'D:/Reverse/BP-Decomp_Workflow/tools/re']
import verify_focus as ppc
from x360rd import u32


def git(*args):
    return subprocess.check_output(['git', *args], cwd=REPO).decode('utf-8').strip()


def digest(data):
    return hashlib.sha256(data).hexdigest()


def hex_words(code, offsets):
    return {f'0x{off:X}': code[off:off + 4].hex().upper() for off in offsets}


def witness(code, words):
    assert hex_words(code, words) == {f'0x{k:X}': v for k, v in words.items()}
    return hex_words(code, words)


def calls(relocations, edges):
    result = {}
    for offset, prefix in edges.items():
        kind, target = relocations[offset]
        assert kind == 6 and target.startswith(prefix), (offset, kind, target)
        result[f'0x{offset:X}'] = target
    return result


assert git('rev-parse', 'HEAD', 'HEAD^', 'HEAD^{tree}', 'HEAD^^{tree}').splitlines() == [HEAD, PARENT, TREE, PARENT_TREE]
assert git('rev-parse', ORIGINAL_REF) == ORIGINAL_HEAD
assert not git('status', '--porcelain=v1')
changed = git('diff-tree', '--no-commit-id', '--name-only', '-r', HEAD).splitlines()
assert len(changed) == 8
assert git('rev-parse', f'{HEAD}:src/GameSource/Game/BrnGameModule.cpp') == git(
    'rev-parse', f'{PARENT}:src/GameSource/Game/BrnGameModule.cpp')
compile_path = OUT / 'compile-results.json'
compile_results = json.loads(compile_path.read_bytes())
assert compile_results['passed'] == 7 and compile_results['failed'] == 0
objects, sources = {}, {}
for row in compile_results['results']:
    assert row['passed'] and row['returncode'] == 0 and row['machine'] == 0x1F2
    obj, asm, log = (Path(row[key]) for key in ('object', 'assembly', 'log'))
    assert obj.read_bytes()[:2] == b'\xf2\x01'
    assert not re.search(r'\bwarning [A-Z]+[0-9]+:', log.read_text(errors='replace'))
    source = row['source']
    objects[Path(source).stem] = obj.stem
    sources[source] = { 'blob': git('rev-parse', f'HEAD:{source}'),
        'source_sha256': ppc.sha(REPO / source), 'object_sha256': ppc.sha(obj),
        'assembly_sha256': ppc.sha(asm), 'log_sha256': ppc.sha(log) }
compiler = Path(compile_results['results'][0]['command'][0])
ppc.OUT = OUT

def body(owner, symbol):
    return ppc.coff_body(objects[owner], symbol)

wrap, wrap_rels = body('GameBridgeWorldToX', '?BridgeWorldToDirector@')
live, live_rels = body('GameBridgeWorldToX', '?BridgeLiveWorldToDirector@')
crash, _ = body('BrnDirectorModuleIOInputBuffer', '?SetPlayerCrashInfo@InputBuffer@')
boost, boost_rels = body('BrnDirectorModuleIOInputBuffer', '?SetPlayerBoostPercentage@InputBuffer@')
update, update_rels = body('BrnMainDirector', '?Update@MainDirector@')
behaviour, _ = body('BrnMainDirector', '?BuildBehaviourSharedInfo@')
caller, caller_rels = body('BrnGameModule', '?DoUpdate_Director@')
mode_targets = [wrap_rels[0xCC + 4 * i][1] for i in range(8)]
assert mode_targets == ['$LN11'] * 4 + ['$LN2', '$LN10', '$LN10', '$LN2']
copy_loop = ['39200006', '7D2903A6', 'E92B0000', '396B0008', 'F92A0000', '394A0008', '4200FFF0']
raw = {0x823E3AF0:'4BFC2C29', 0x823E3C84:'3960FFFF', 0x823E3C88:'917A7AA8', 0x823E3C8C:'4800140C',
       0x823E3C98:'419A0268', 0x823E3CC8:'409A0238', 0x823E3CF0:'409A01D8', 0x823E3CF8:'419A01D0',
       0x823E3E38:'C01F5EFC', 0x823E3E3C:'D01A325C', 0x823E3E68:'397F5BA0', 0x823E3E6C:'395A78E0',
       0x823E3F88:'395A6AB0', 0x823E3F8C:'39200005', 0x823E5074:'394B5BA0', 0x823E50A4:'616B5244',
       0x823E50B0:'996A7AC7', 0x823E50B4:'4BFC170D', 0x82274BB8:'89637AC7', 0x82274BC8:'89630006',
       0x82274BD4:'9B6101E2', 0x82034E50:'34000000', 0x82002514:'B4000000'}
for first in (0x823E3E70, 0x823E4FCC, 0x823E507C):
    raw.update({first + 4 * i: word for i, word in enumerate(copy_loop)})
for address, expected in raw.items():
    assert f'{u32(address):08X}' == expected, hex(address)

old = WAVE / 'r167-composed-preflight'
old_command = json.loads((old / 'COMMAND.json').read_bytes())
assert old_command['head'] == PARENT and old_command['tree'] == PARENT_TREE
old_codec = 'BrnReplayDirectorBridgeSerialiser-1ea958bd'
codec = {}
for method in ('Read', 'Write', 'SerialiseGameActionQueue'):
    symbol = f'?{method}@DirectorBridgeSerialiser@'
    native = body('BrnReplayDirectorBridgeSerialiser', symbol)
    ppc.OUT = old
    baseline = ppc.coff_body(old_codec, symbol)
    ppc.OUT = OUT
    assert native == baseline
    codec[method] = {'bytes': len(native[0]), 'code_sha256': digest(native[0]), 'relocations': len(native[1])}

record = {'state':'pass', 'head':HEAD, 'tree':TREE, 'parent':PARENT, 'parent_tree':PARENT_TREE,
    'preserved_worktree_ref':ORIGINAL_HEAD, 'changed_blobs':{path: git('rev-parse', f'HEAD:{path}') for path in changed},
    'compiler_sha256':ppc.sha(compiler), 'compile_results_sha256':ppc.sha(compile_path),
    'compile_scope':sources, 'compiler_warnings':0, 'r167_codec_object_sha256':ppc.sha(old / f'{old_codec}.obj'),
    'codec_methods_identical_to_r167':codec, 'artist_words':{f'0x{k:X}':value for k, value in raw.items()},
    'candidate_mode_table':dict(zip(range(8), ('live',)*4 + ('readiness','playback','playback','readiness'))),
    'candidate_word_witnesses':{
        'mode_dispatch':witness(wrap, {0x98:'3880FFFF', 0xA4:'480001C4', 0xA8:'817C0000',
            0x154:'2F1F0008', 0x1A8:'38AB5BD0', 0x1B4:'3BFF0040', 0x1BC:'2F1F0200',
            0x1F4:'C03F5EFC', 0x228:'389F5BA0', 0x248:'614A10C8', 0x24C:'896B0004',
            0x260:'616B10C9', 0x25C:'89430004', 0x270:'616B5244', 0x278:'99797AC7'}),
        'live_publish':witness(live, {0xA4:'395B6AB0', 0xA8:'39200005', 0x12C:'9B830000', 0x248:'9BEB0003',
            0x938:'1D7D04F0', 0xA40:'394A5BD0', 0xB04:'C00B0224', 0xB20:'C1AB0220', 0xB24:'EFED0024',
            0xB44:'D3E35EFC', 0xCE8:'39435BA0', **{0xCF0+4*i:w for i,w in enumerate(copy_loop)}}),
        'crash_input':witness(crash, {0x4:'394378E0', **{0x8+4*i:w for i,w in enumerate(copy_loop)}}),
        'all_streamed':witness(behaviour, {0x1B8:'897C6AB4', 0x1CC:'997F05EC'}),
        'screenshot':witness(update, {0x3A8:'89637AC7', 0x3B8:'89630006', 0x3C4:'9AC10192'})},
    'candidate_relocation_edges':{
        'mode_dispatch':calls(wrap_rels, {0x24:'?Lock@BaseSerialiser@', 0xA0:'?SetPlayerCarIndex@',
            0xFC:'?BridgeLiveWorldToDirector@', 0x108:'?GetActivePlayerIndex@', 0x124:'?GetVehicleActive@',
            0x13C:'?GetVehicleInfo@', 0x14C:'?SetRaceCarInfo@', 0x1B0:'?SetCrashingCentreOfMass@',
            0x1F8:'?SetPlayerBoostPercentage@', 0x230:'?SetPlayerCrashInfo@', 0x27C:'?Unlock@BaseSerialiser@'}),
        'live_publish':calls(live_rels, {0x9C:'?GetWorldEntityStatusInterface@', 0xE4:'?Append@?$BaseEventQueue@UNewVehicleEvent@',
            0x924:'?SetRaceCarInfo@', 0x948:'??4VehicleInfo@', 0xB30:'?SetPlayerBoostPercentage@', 0xCD4:'?SetPlayerCrashInfo@'}),
        'frame_caller':calls(caller_rels, {0x80:'?Read@DirectorBridgeSerialiser@', 0xE4:'?BridgeWorldToDirector@',
            0xF4:'?BridgeGameStateToDirector@', 0x154:'?Write@DirectorBridgeSerialiser@'})},
    'scope_limit':'Focused Xbox 360 compile and COFF proof. No link or runtime result.'}
receipt = OUT / 'RECEIPT.json'
receipt.write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
print(receipt, ppc.sha(receipt))
