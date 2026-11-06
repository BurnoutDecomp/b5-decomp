"""Verify R162 linked InputPads initialization against ARTIST with pinned inputs."""
import argparse
import hashlib
import json
import re
import struct
import subprocess
import sys
from pathlib import Path

import pefile

sys.path.insert(0, 'D:/Reverse/BP-Decomp_Workflow/tools/re')
import x360rd

CANDIDATE = '8b9f64f5ebb873ba5b6577734d1afb9881176c30'
TREE = 'c9a45a7234193c374f89d1161d756928f0ba94b7'
PARENT = '808f548465be6e2c81967f4ea8af1419f6f651fc'
REPO = 'C:/Users/adri1/.codex/worktrees/bp360/native_resources'
INITIALIZE = '?InitializePads@InputPads@CgsInput@@AAAXXZ'
CALLER = '?Construct@InputPads@CgsInput@@QAAXXZ'
CALLEE = '?Construct@DeviceX360Pad@CgsInput@@QAAXXZ'
ORIGINAL = 0x828e7238


def sha(data):
    return hashlib.sha256(data).hexdigest()


def check(condition, message):
    if not condition:
        raise ValueError(message)


def branch_target(word, pc):
    check(word >> 26 == 18 and word & 3 == 1, 'expected relative branch-and-link')
    delta = word & 0x03fffffc
    if delta & 0x02000000:
        delta -= 0x04000000
    return (pc + delta) & 0xffffffff


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--pins', type=Path, required=True)
    ap.add_argument('--pins-sha256', required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    check(args.pins.name == 'ROOT_LINK_PINS.json', 'expected ROOT_LINK_PINS.json')
    check(not args.output.exists(), 'refusing to overwrite output')
    pin_data = args.pins.read_bytes()
    check(sha(pin_data) == args.pins_sha256.lower(), 'pins hash mismatch')
    pins = json.loads(pin_data)
    head = pins['head']
    check(re.fullmatch(r'[0-9a-f]{40}', head) is not None, 'invalid pinned head')
    check(pins['tree'] == TREE and pins['parent'] == PARENT, 'pinned tree/parent mismatch')
    def git(*args):
        return subprocess.check_output(['git', '-C', REPO, *args], text=True).strip()
    check(git('rev-parse', head + '^{tree}') == TREE, 'integration tree mismatch')
    check(git('rev-parse', CANDIDATE + '^{tree}') == TREE, 'candidate tree mismatch')
    check(git('rev-list', '--parents', '-n', '1', head).split() == [head, PARENT],
          'integration is not single-parent at expected parent')
    blobs = {}
    for key in ('pe', 'map', 'seal', 'completion', 'result'):
        row = pins['pins'][key]
        blob = Path(row['path']).read_bytes()
        check(sha(blob) == row['sha256'], key + ' hash mismatch')
        blobs[key] = blob
    seal = json.loads(blobs['seal'])
    result = json.loads(blobs['result'])
    check(seal['head'] == head, 'seal head mismatch')
    completion = json.loads(blobs['completion'])
    check(completion['head'] == head and completion['status'] == 'SEALED_NO_REGRESSIONS' and
          completion['requested'] == completion['passed'] and completion['failed'] == 0,
          'intake completion mismatch')
    check(result['exit_code'] == 0 and result['all_post_checks_pass'], 'strict link result failed')
    outputs = {Path(k.replace('\\', '/')).name: v['sha256'] for k, v in result['outputs'].items()}
    check(outputs.get('native-game.pe') == sha(blobs['pe']), 'PE not in result')
    check(outputs.get('native-game.map') == sha(blobs['map']), 'map not in result')
    pe = pefile.PE(data=blobs['pe'], fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    sections = {s.Name.rstrip(b'\0'): s for s in pe.sections}
    text = sections[b'.text']
    pdata = sections[b'.pdata']
    entries = list(struct.iter_unpack('>II', blobs['pe'][pdata.PointerToRawData:
                            pdata.PointerToRawData + pdata.Misc_VirtualSize]))
    sizes = {}
    for i, (va, flags) in enumerate(entries):
        size = ((flags >> 8) & 0x3fffff) * 4
        end = entries[i + 1][0] if i + 1 < len(entries) else base + text.VirtualAddress + text.Misc_VirtualSize
        if flags >> 30 == 1 and size and base + text.VirtualAddress <= va < va + size <= end:
            check(va not in sizes, 'duplicate unwind extent')
            sizes[va] = size
    wanted = (INITIALIZE, CALLER, CALLEE)
    found = {s: [] for s in wanted}
    rx = re.compile(r'^\s*0003:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{8,16})\s+f(?:\s|$)')
    for line in blobs['map'].decode(errors='replace').splitlines():
        m = rx.match(line)
        if m and m[1] in found:
            found[m[1]].append(int(m[2], 16))
    check(all(len(x) == 1 for x in found.values()), 'missing or ambiguous map symbol')
    va, caller, callee = (found[name][0] for name in wanted)
    check(sizes.get(va) == 136 and sizes.get(caller, 0) >= 0xec, 'invalid function extents')

    def read_code(address, length):
        start = address - base - text.VirtualAddress
        check(0 <= start and start + length <= min(text.Misc_VirtualSize, text.SizeOfRawData), 'code outside section')
        off = text.PointerToRawData + start
        return blobs['pe'][off:off + length]

    original = x360rd.rd(ORIGINAL, 136)
    linked = read_code(va, 136)
    old = struct.unpack('>34I', original)
    new = struct.unpack('>34I', linked)
    differences = [hex(i * 4) for i, (a, b) in enumerate(zip(old, new))
                   if (a & (0xfc000003 if i == 23 else 0xffffffff)) !=
                      (b & (0xfc000003 if i == 23 else 0xffffffff))]
    check(not differences, 'fixed-bit mismatch: ' + str(differences))
    expected_callee = branch_target(old[23], ORIGINAL + 92)
    check(expected_callee == 0x828dc578, 'unexpected original pad constructor target')
    check(branch_target(new[23], va + 92) == callee, 'wrong DeviceX360Pad constructor target')
    caller_word = struct.unpack_from('>I', read_code(caller, sizes[caller]), 0xe8)[0]
    check(branch_target(caller_word, caller + 0xe8) == va, 'caller edge missing at +0xe8')
    check(branch_target(struct.unpack('>I', x360rd.rd(0x828efbe4, 4))[0], 0x828efbe4) == ORIGINAL,
          'original caller edge mismatch')
    report = {'status': 'pass', 'head': head, 'tree': TREE, 'parent': PARENT,
              'candidate_same_tree': CANDIDATE, 'pins_sha256': sha(pin_data),
              'pe_sha256': sha(blobs['pe']), 'map_sha256': sha(blobs['map']),
              'original': hex(ORIGINAL), 'linked': hex(va), 'bytes': 136,
              'raw_exact': original == linked, 'fixed_instruction_bits_equal': True,
              'masked_fields': ['branch displacement at +0x5c'], 'linked_callee': hex(callee),
              'original_callee': hex(expected_callee), 'callee_symbol': CALLEE,
              'caller': hex(caller), 'caller_branch_offset': '0xe8', 'caller_target_verified': True,
              'scope': 'static linked PE; no runtime execution'}
    with args.output.open('x', encoding='utf-8') as out:
        json.dump(report, out, indent=2)
        out.write('\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
