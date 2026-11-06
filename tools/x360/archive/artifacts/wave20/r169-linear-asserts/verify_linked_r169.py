"""Check pinned R169 LinearMalloc linked bodies against ARTIST and donor COFF."""
import argparse
import hashlib
import json
import re
import struct
import sys
from pathlib import Path

import pefile

sys.path.insert(0, 'D:/Reverse/BP-Decomp_Workflow/tools/re')
import x360rd

OBJ = Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r169-linear-asserts/CgsLinearMalloc.obj')
OBJ_SHA = '3435436ae65ae3a3afce062a7ceb8cdd597c7b65e05ed1e928cb341bab5be7bd'
PARENT = '6bf19bb5d1c842569c8cda83229bf84ead257827'
ANCHORS = [('?Construct@LinearMalloc@CgsMemory@@QAAXXZ', 0x828690e0, 36),
           ('?GetUsage@LinearMalloc@CgsMemory@@QBAIXZ', 0x82866d40, 16),
           ('?GetFreeMemory@LinearMalloc@CgsMemory@@QBAIXZ', 0x82866db0, 16),
           ('?FreeAll@LinearMalloc@CgsMemory@@QAAXXZ', 0x82866e60, 12)]
DIAGNOSTICS = [('?SetAlignment@LinearMalloc@CgsMemory@@QAAXI@Z', 0x82866cc0, 128),
               ('?GetSize@LinearMalloc@CgsMemory@@QBAIXZ', 0x82866d50, 96)]
STRINGS = {0x820e6fc8:b'd:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\memory\\CgsLinearMalloc.cpp',
           0x820e7074:b'liAlignment> 0',
           0x820e7030:b'( ( ( liAlignment- 1 ) | liAlignment) + 1 ) == ( liAlignment* 2 )',
           0x820e7084:b'mbCreated==true'}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def check(ok, message):
    if not ok:
        raise ValueError(message)


def target(word, pc):
    check(word >> 26 == 18 and word & 2 == 0, 'not a relative branch')
    d = word & 0x03fffffc
    if d & 0x02000000:
        d -= 0x04000000
    return (pc + d) & 0xffffffff


def coff(data):
    machine, nsec, _, off, count, opt, _ = struct.unpack_from('<HHIIIHH', data)
    check(machine == 0x1f2, 'not PPC COFF')
    strings = data[off + count * 18:]
    sy = {}
    i = 0
    while i < count:
        raw, val, sec, _, _, aux = struct.unpack_from('<8sIhHBB', data, off + 18 * i)
        st = struct.unpack_from('<I', raw, 4)[0] if raw[:4] == bytes(4) else 0
        name = (strings[st:strings.find(b'\0', st)] if st else raw.rstrip(b'\0')).decode('latin1')
        sy[i] = (name, val, sec)
        i += 1 + aux
    def get(name):
        rows = [v for v in sy.values() if v[0] == name and v[2] > 0]
        check(len(rows) == 1 and rows[0][1] == 0, 'ambiguous COFF function')
        h = struct.unpack_from('<8sIIIIIIHHI', data, 20 + opt + 40 * (rows[0][2] - 1))
        body = data[h[4]:h[4] + h[3]]
        rels = [(a, t, sy[j][0]) for k in range(h[7])
                for a, j, t in [struct.unpack_from('<IIH', data, h[5] + 10 * k)]]
        return body, rels
    return get


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--pins', type=Path, required=True)
    ap.add_argument('--pins-sha256', required=True)
    ap.add_argument('--head', required=True)
    ap.add_argument('--output', type=Path, required=True)
    a = ap.parse_args()
    check(a.pins.name == 'ROOT_LINK_PINS.json' and not a.output.exists(), 'invalid pins/output')
    pd = a.pins.read_bytes()
    check(sha(pd) == a.pins_sha256.lower(), 'pins hash mismatch')
    pins = json.loads(pd)
    check(pins['head'] == a.head and pins['parent'] == PARENT and re.fullmatch('[0-9a-f]{40}', a.head), 'wrong head/parent')
    blobs = {}
    for key in ('pe','map','seal','completion','result'):
        row = pins['pins'][key]
        blobs[key] = Path(row['path']).read_bytes()
        check(sha(blobs[key]) == row['sha256'], key + ' hash mismatch')
    seal, complete, result = (json.loads(blobs[x]) for x in ('seal','completion','result'))
    check(seal['head'] == a.head and complete['head'] == a.head and complete['status'] == 'SEALED_NO_REGRESSIONS' and
          complete['requested'] == complete['passed'] and complete['failed'] == 0 and
          result['exit_code'] == 0 and result['all_post_checks_pass'], 'seal/completion failed')
    outputs = {Path(k.replace('\\','/')).name: v['sha256'] for k,v in result['outputs'].items()}
    check(outputs['native-game.pe'] == sha(blobs['pe']) and outputs['native-game.map'] == sha(blobs['map']), 'result mismatch')
    obj = OBJ.read_bytes()
    check(sha(obj) == OBJ_SHA, 'COFF hash mismatch')
    get = coff(obj)
    symbols = {}
    function_addresses = set()
    rx = re.compile(r'^\s*([0-9a-fA-F]{4}):[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{8,16})(?:\s+(.*))?$')
    for line in blobs['map'].decode(errors='replace').splitlines():
        m = rx.match(line)
        if not m:
            continue
        section, name, va, tail = int(m[1],16), m[2], int(m[3],16), m[4] or ''
        symbols.setdefault(name, set()).add(va)
        if section == 3 and re.match(r'f(?:\s|$)', tail):
            function_addresses.add(va)
    def addr(name):
        values = symbols.get(name, set())
        check(len(values) == 1, 'missing/ambiguous symbol: ' + name)
        return next(iter(values))
    pe = pefile.PE(data=blobs['pe'], fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    def read(va, size):
        rva = va - base
        secs = [s for s in pe.sections if s.VirtualAddress <= rva and
                rva + size <= s.VirtualAddress + min(s.SizeOfRawData, s.Misc_VirtualSize)]
        check(len(secs) == 1, 'invalid section address')
        s = secs[0]
        p = s.PointerToRawData + rva - s.VirtualAddress
        return blobs['pe'][p:p+size]
    for va, text in STRINGS.items():
        check(x360rd.rd(va, len(text)+1) == text+b'\0', 'original literal mismatch')
        check(text+b'\0' in obj, 'donor literal missing')
    results = []
    for name, original, length in ANCHORS + DIAGNOSTICS:
        body, rels = get(name)
        va = addr(name)
        check(len(body) == length, 'COFF extent mismatch')
        later = [x for x in function_addresses if x > va]
        check(not later or min(later) >= va + length, 'intervening function symbol')
        linked = read(va, length)
        artist = x360rd.rd(original, length)
        by = {off:(typ,sym) for off,typ,sym in rels if typ != 18}
        check(all(t in (6,16,17,18) for _,t,_ in rels), 'unexpected relocation type')
        for off in range(0,length,4):
            av, cv, lv = (struct.unpack_from('>I', data, off)[0] for data in (artist,body,linked))
            typ, sym = by.get(off, (None,None))
            mask = 0xfc000003 if typ == 6 else 0xffff0000 if typ in (16,17) else 0xffffffff
            check(av & mask == cv & mask == lv & mask, name + ' fixed word mismatch ' + hex(off))
            if typ == 6:
                check(target(lv,va+off) == addr(sym), 'wrong branch target: '+sym)
        groups = {}
        for off,(typ,sym) in by.items():
            if typ in (16,17):
                groups.setdefault(sym,{})[typ] = off
        for sym, offsets in groups.items():
            check(set(offsets) == {16,17}, 'incomplete data relocation pair')
            hi = struct.unpack_from('>I',linked,offsets[16])[0] & 0xffff
            lo = struct.unpack_from('>h',linked,offsets[17]+2)[0]
            value = ((hi << 16) + lo) & 0xffffffff
            check(value == addr(sym), 'wrong string address')
            old_hi = struct.unpack_from('>I',artist,offsets[16])[0] & 0xffff
            old_lo = struct.unpack_from('>h',artist,offsets[17]+2)[0]
            old_value = ((old_hi << 16) + old_lo) & 0xffffffff
            check(old_value in STRINGS, 'unknown original diagnostic literal')
            literal = STRINGS[old_value]
            check(read(value, len(literal)+1) == literal+b'\0', 'linked literal differs')
        if (name,original,length) in ANCHORS:
            check(not rels and linked == artist == body, 'raw anchor mismatch')
        results.append({'symbol':name,'artist':hex(original),'linked':hex(va),'bytes':length,
                        'raw_exact':linked==artist,'relocations':len(by),'verified_fixed_bits':True})
    report = {'status':'pass','head':a.head,'tree':pins['tree'],'pins_sha256':sha(pd),'pe_sha256':sha(blobs['pe']),
              'map_sha256':sha(blobs['map']),'coff_sha256':sha(obj),'functions':results,
              'diagnostic_strings_verified':len(STRINGS),'scope':'static linked PE; no runtime'}
    with a.output.open('x',encoding='utf-8') as f:
        json.dump(report,f,indent=2)
        f.write('\n')
    print(json.dumps(report,indent=2))


if __name__ == '__main__':
    main()
