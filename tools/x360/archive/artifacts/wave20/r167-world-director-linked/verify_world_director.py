"""Verify the World-to-Director path in the final pinned R167 Xbox 360 PE."""
import argparse
import json
from pathlib import Path
import runpy
import sys

OUT = Path(__file__).resolve().parent
WAVE = OUT.parent
CANDIDATE = 'aa0986c0b7b33cc978b2945dd8fbc4577f181cf7'
TREE = '54cb3dd0e5592344bdc40ea616b6c5800ba6c674'
PARENT = '135948fd62910b9dc8911e865cf476555d05460e'
DONOR_SHA = '506040d6554d66a0f6f8dab2d9ef22f83e46dc280ad720b64382039b974821cb'
COMPOSED_SHA = 'd29386addaf3ed54af942d467d8aed42ea9a62f8c316124baf9530309cbe5d12'
BINDING_SHA = 'dec2cdaaccd47a47596dd95e696ecf2107df18ea00af567c8c6f524a0eebdfd6'
H = runpy.run_path(str(WAVE / 'r160-gui-linked/verify_gui.py'))
IMAGE, COFF, load, sha, digest, word, canonical, pe_bytes, split_address, tree, blob, source_bytes = (
    H[key] for key in ('IMAGE', 'COFF', 'load', 'sha', 'digest', 'word', 'canonical',
                       'pe_bytes', 'split_address', 'tree', 'blob', 'source_bytes'))
map_symbols = H['TOOLS']['PRIOR']['HELPER']['map_symbols']
git = H['TOOLS']['git']
sys.path.insert(0, 'D:/Reverse/BP-Decomp_Workflow/tools/re')
from x360rd import u32

WORLD_IO = 'src/GameSource/World/BrnWorldModuleIO_UpdateOutputBuffer.cpp'
INPUT_IO = 'src/GameSource/Director/DirectorModule/BrnDirectorModuleIOInputBuffer.cpp'
DIRECTOR = 'src/GameSource/Director/BrnMainDirector.cpp'
REPLAY = 'src/GameSource/Replays/Serialisers/BrnReplayDirectorBridgeSerialiser.cpp'
BRIDGE = 'src/GameSource/Game/GameBridgeWorldToX.cpp'
STATIC = '?GetStaticLayout@DirectorBridgeSerialiser@'
WORLD_STATUS = '?GetWorldEntityStatusInterface@UpdateOutputBuffer@'
FOCUS = '?SetWorldWantsDebugControllerFocus@UpdateOutputBuffer@'
BOOST = '?SetPlayerBoostPercentage@InputBuffer@'
SHARED = '?BuildBehaviourSharedInfo@MainDirector@'


def pinned(path, expected):
    assert sha(path) == expected, path
    return load(path)


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument('--root-pins', type=Path, required=True)
    cli.add_argument('--root-pins-sha256', required=True)
    cli.add_argument('--output', type=Path, required=True)
    args = cli.parse_args()
    root = pinned(args.root_pins, args.root_pins_sha256.lower())
    head, pins = root['head'], root['pins']
    assert root['tree'] == tree(head) == tree(CANDIDATE) == TREE
    assert root['parent'] == git('rev-parse', head + '^').decode().strip() == PARENT
    for name, item in pins.items():
        assert sha(Path(item['path'])) == item['sha256'], name
    seal, inv, result, image_info, diagnostics, completion = (
        load(Path(pins[k]['path'])) for k in ('seal', 'invocation', 'result', 'image', 'diagnostics', 'completion'))
    assert seal['head'] == image_info['head'] == completion['head'] == head
    assert inv['sealed_sha256'] == pins['seal']['sha256'] and inv['objects'] == seal['selected']
    assert completion['passed'] == completion['requested'] and completion['failed'] == 0
    assert result['exit_code'] == 0 and result['all_post_checks_pass']
    assert not diagnostics['unresolved_messages'] and not diagnostics['duplicate_messages']
    assert image_info['magic'] == 'XEX2' and image_info['image_sha256'] == pins['xex']['sha256']
    outputs = {Path(path).name: item for path, item in result['outputs'].items()}
    for key in ('pe', 'map', 'xex'):
        assert outputs[Path(pins[key]['path']).name]['sha256'] == pins[key]['sha256']

    batch = WAVE / 'r167-batch-v3'
    composed = pinned(batch / 'COMPOSED_PPC_CHECK.json', COMPOSED_SHA)
    binding = pinned(batch / 'TARGETED_OBJECT_BINDING.json', BINDING_SHA)
    donor = pinned(WAVE / 'r168-world-director-replay/RECEIPT.json', DONOR_SHA)
    assert composed['head'] == binding['head'] == CANDIDATE and composed['tree'] == binding['tree'] == TREE
    assert composed['targeted_binding_sha256'] == BINDING_SHA and binding['parent'] == PARENT
    proof = composed['world_bridge']
    for label, expected in donor['candidate_word_witnesses'].items():
        assert proof['methods'][label]['words'] == expected
    for label, expected in donor['candidate_relocation_edges'].items():
        assert proof['methods'][label]['edges'] == expected
    for address, expected in donor['artist_words'].items():
        assert u32(int(address, 16)) == int(expected, 16), address
    for site, dest in ((0x823E8E80, 0x82657478), (0x823E8EE4, 0x823E3AB0), (0x823E8F64, 0x82657210)):
        assert IMAGE.target(None, site, u32(site)) == dest

    selected = {row['source']: row for row in inv['objects']}
    bound = {row['source']: row for row in binding['results']}
    src_root = Path(seal['source_root'])
    sources = {Path(row['path']).relative_to(src_root).as_posix().lower(): row for row in seal['sources']
               if Path(row['path']).is_relative_to(src_root)}
    map_path, pe_path = (Path(pins[k]['path']) for k in ('map', 'pe'))
    image = IMAGE(pe_path, map_path)
    entries = map_symbols(map_path.read_bytes())
    objects, records, bodies = {}, {}, {}

    def owner(source):
        if source not in objects:
            row = selected[source]
            previous = bound[source]
            previous_obj = previous['artifacts']['object']
            obj_path, pre_path = Path(row['object']), Path(previous_obj['path'])
            selected_bytes, pre_bytes = obj_path.read_bytes(), pre_path.read_bytes()
            assert digest(selected_bytes) == row['sha256'] and digest(pre_bytes) == previous_obj['sha256']
            actual = Path(sources[source.lower()]['path']).read_bytes()
            assert digest(actual) == sources[source.lower()]['sha256']
            assert actual.replace(b'\r\n', b'\n') == source_bytes(head, source).replace(b'\r\n', b'\n')
            assert blob(head, source) == blob(CANDIDATE, source) == previous['git_blob']
            objects[source] = obj_path, COFF(selected_bytes), COFF(pre_bytes)
        return objects[source]

    def method(label, source, prefix, words, edges, expected_size=None, forced_va=None):
        path, obj, baseline_obj = owner(source)
        symbol, code, refs = obj.function(prefix)
        _, before, before_refs = baseline_obj.function(prefix)
        assert canonical(code, refs) == canonical(before, before_refs), label
        if expected_size is not None:
            assert len(code) == expected_size, label
        listed = {at for at, map_owner in entries.get(symbol.name, []) if map_owner.lower() == path.name.lower()}
        if forced_va is None:
            [va] = listed
        else:
            va = forced_va
            assert not listed or listed == {va}, label
        linked = pe_bytes(image, va, len(code))
        assert canonical(linked, refs) == canonical(code, refs), label
        for offset, expected in words.items():
            assert offset not in refs and word(linked, offset) == word(code, offset) == expected, (label, hex(offset))
        record = {'source': source, 'object_sha256': selected[source]['sha256'], 'git_blob': blob(head, source),
                  'symbol': symbol.name, 'bytes': len(code), 'linked_va': hex(va), 'linked_sha256': digest(linked),
                  'words': {hex(at): f'{value:08X}' for at, value in words.items()}, 'calls': {}}
        for at, target_prefix in edges.items():
            kind, target = refs[at]
            assert kind == 6 and before_refs[at][0] == 6 and target.name == before_refs[at][1].name
            assert target.name.startswith(target_prefix), (label, hex(at), target.name)
            opcode = word(linked, at)
            assert opcode >> 26 == 18 and opcode & 1
            target_va = image.target(va + at, opcode)
            map_owners = sorted({map_owner for dest, map_owner in entries.get(target.name, []) if dest == target_va})
            # This helper is private to the selected bridge COFF. If omitted from
            # the map, prove its relocated call target against its own body below.
            assert map_owners or target.name.startswith('?BridgeLiveWorldToDirector@'), (label, hex(at), target.name, hex(target_va))
            record['calls'][hex(at)] = {'symbol': target.name, 'site_va': hex(va + at),
                                        'target_va': hex(target_va), 'map_owners': map_owners}
        records[label] = record
        bodies[label] = symbol, va, linked, refs, obj
        return record

    extras = {
        'mode_dispatch': {0x178: STATIC, 0x1C8: STATIC, 0x200: STATIC, 0x238: WORLD_STATUS, 0x254: WORLD_STATUS},
        'live_publish': {at: STATIC for at in (0x120, 0x238, 0x92C, 0xA2C, 0xB38, 0xCDC)},
        'frame_caller': {0x6C: '?IsPlaying@BaseSerialiser@', 0xC4: '?LockBuffersForIO@',
                         0xD4: '?BridgeControllerToDirector@', 0x140: '?IsRecording@BaseSerialiser@',
                         0x16C: '?UnlockBuffersForIO@'},
        'screenshot': {0x3B4: '?GetControll@InputBuffer@'},
    }
    for label, row in proof['methods'].items():
        forced = None
        if label == 'live_publish':
            forced = int(records['mode_dispatch']['calls']['0xfc']['target_va'], 16)
        words = {int(at, 16): int(value, 16) for at, value in row['words'].items()}
        if label == 'screenshot':
            words.update({0x44: 0x61293998, 0x64: 0xC00B325C})
        edges = {int(at, 16): target for at, target in row['edges'].items()} | extras.get(label, {})
        method(label, row['source'], row['symbol'], words, edges, row['bytes'], forced)

    for label, source, prefix, words, edges in (
        ('boost_input', INPUT_IO, BOOST, {0x4C: 0xD3FF325C}, {}),
        ('boost_seed', INPUT_IO, '?Construct@InputBuffer@DirectorIO@', {0x124: 0xD3FF325C}, {}),
        ('static_layout', REPLAY, STATIC, {0x18: 0x2F0B5F00, 0x40: 0x807F0020}, {}),
        ('world_status_provider', WORLD_IO, WORLD_STATUS, {0x44: 0x3C7F0003, 0x48: 0x38639480}, {}),
        ('world_focus_provider', WORLD_IO, FOCUS, {0x50: 0x616B5244}, {}),
        ('pre_scene_consumer', DIRECTOR, '?UpdateCameraBehavioursPreScene@MainDirector@', {}, {0x24: SHARED}),
        ('post_scene_consumer', DIRECTOR, '?UpdateCameraBehavioursPostScene@MainDirector@', {}, {0x24: SHARED}),
        ('playback_player', BRIDGE, '?GetActivePlayerIndex@DirectorBridgeSerialiser@', {}, {0x10: STATIC}),
        ('playback_activity', BRIDGE, '?GetVehicleActive@DirectorBridgeSerialiser@', {}, {0x18: STATIC}),
        ('playback_vehicle', BRIDGE, '?GetVehicleInfo@DirectorBridgeSerialiser@', {}, {0x18: STATIC}),
    ):
        method(label, source, prefix, words, edges)

    wrapper, va, code, refs, _ = bodies['mode_dispatch']
    assert refs[0xB4][0] == 16 and refs[0xB8][0] == 17 and refs[0xB4][1] == refs[0xB8][1]
    table = refs[0xB4][1]
    assert table.section == wrapper.section and table.value == 0xCC
    assert split_address(code, 0xB4, 0xB8) == va + 0xCC
    cases = {}
    for mode, (item, target) in enumerate(zip(proof['mode_table'], (0xEC,)*4 + (0x234, 0x104, 0x104, 0x234), strict=True)):
        offset = 0xCC + 4 * mode
        assert item['mode'] == mode and int(item['offset'], 16) == offset
        kind, symbol = refs[offset]
        assert kind == 2 and symbol.name == item['label'] and symbol.section == wrapper.section and symbol.value == target
        assert image.word(va + offset) == va + target
        cases[str(mode)] = {'slot_va': hex(va + offset), 'target_va': hex(va + target),
                            'path': donor['candidate_mode_table'][str(mode)]}
    branches = {}
    for at, target in ((0x94, 0xA8), (0xA4, 0x268), (0xB0, 0x268), (0x100, 0x268),
                       (0x130, 0x150), (0x158, 0x11C), (0x1C0, 0x174)):
        assert image.target(va + at, word(code, at)) == va + target
        branches[hex(va + at)] = hex(va + target)

    _, live_va, code, refs, obj = bodies['live_publish']
    boost_constants = {}
    for high, low, art in ((0xAEC, 0xB00, 0x82034E50), (0xB10, 0xB14, 0x82002514)):
        kind_high, target = refs[high]
        assert kind_high == 16 and refs[low] == (17, target)
        address = split_address(code, high, low)
        bits = u32(art).to_bytes(4, 'big')
        assert obj.literal(target, 4) == pe_bytes(image, address, 4) == bits
        boost_constants[hex(address)] = bits.hex().upper()

    receipt = {'state': 'pass', 'head': head, 'tree': TREE, 'parent': PARENT, 'candidate': CANDIDATE,
               'root_pins_sha256': args.root_pins_sha256.lower(), 'composed_ppc_sha256': COMPOSED_SHA,
               'binding_sha256': BINDING_SHA, 'donor_receipt_sha256': DONOR_SHA,
               'methods': records, 'linked_mode_table': cases, 'linked_continuation_branches': branches,
               'linked_boost_constants': boost_constants,
               'scope': 'World-to-Director linked PPC. Frame order only for adjacent bridges and codec. Runtime unverified.'}
    text = json.dumps(receipt, indent=2) + '\n'
    if args.output.exists():
        assert args.output.read_text(encoding='utf-8') == text
    else:
        with args.output.open('x', encoding='utf-8') as output:
            output.write(text)
    print('PASS', len(records), 'World-to-Director functions', sha(args.output))


if __name__ == '__main__':
    main()
