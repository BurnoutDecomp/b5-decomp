
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import hashlib
import json
import re
import struct

root = Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave20')
artifact = root / 'r35-gamestate-events-sol'
source = Path(r'D:/Reverse/BP-X360-Experimental-Worktrees/r35-gamestate-events-sol/src/GameSource/GameState')
base = 'a98e1ced2f3c565f250b30229c96dcd92706253e'
seal = json.loads((root / 'r35-link/seal.json').read_bytes())
assert seal['head'] == base


def read_coff(data):
    machine, sections, _, pointer, count, optional, _ = struct.unpack_from('<HHIIIHH', data)
    assert machine == 0x1f2 and optional == 0
    table = pointer + count * 18
    symbols = {}
    i = 0
    while i < count:
        raw, value, sec, _, storage, aux = struct.unpack_from('<8sIhHBB', data, pointer + i*18)
        if raw[:4] == bytes(4):
            at = table + struct.unpack_from('<I', raw, 4)[0]
            raw = data[at:data.index(b'\0', at)]
        symbols[i] = raw.rstrip(b'\0').decode('ascii'), value, sec, storage
        i += 1 + aux
    return symbols, sections


def code(data, prefix):
    syms, _ = read_coff(data)
    name, value, sec, _ = next(x for x in syms.values() if x[0].startswith(prefix) and x[2] > 0)
    _, _, _, size, raw, rel, _, nrel, _, _ = struct.unpack_from('<8sIIIIIIHHI', data, 20 + (sec - 1) * 40)
    rels = {}
    for n in range(nrel):
        offset, index, kind = struct.unpack_from('<IIH', data, rel + 10*n)
        if kind != 18:
            rels[offset] = (kind, syms[index])
    return name, value, sec, data[raw:raw+size], rels


def archive_defs(data):
    assert data.startswith(b'!<arch>\n')
    count = struct.unpack_from('>I', data, 68)[0]
    p = 72 + count * 4
    for _ in range(count):
        end = data.index(b'\0', p)
        yield data[p:end].decode('ascii')
        p = end + 1


gate = json.loads((artifact / 'native-verified/compile-results.json').read_bytes())
assert gate['passed'] == gate['requested'] == 7 and not gate['failed']
rows = {Path(row['source']).stem: row for row in gate['results']}
provider = Path(rows['GameStateModule_ProcessGameEvents']['object']).read_bytes()
symbol, val, sec, body, rels = code(provider, '?ProcessGameEvents@')
assert val == 0

sealed_caller, = (r for r in seal['selected'] if r['source'].endswith('/BrnGameStateModule.cpp'))
caller = Path(sealed_caller['object']).read_bytes()
assert hashlib.sha256(caller).hexdigest() == sealed_caller['sha256']
rebuilt_caller = Path(rows['BrnGameStateModule']['object']).read_bytes()
for obj in (caller, rebuilt_caller):
    _, _, _, _, caller_rels = code(obj, '?PreWorldUpdate@GameStateModule@')
    assert [x[0] for x in caller_rels.values() if x[1][0] == symbol] == [6]

words = struct.unpack(f'>{len(body)//4}I', body)
branch_table_offset = 4 * (words.index(0x4e800420) + 1)
assert words[branch_table_offset//4-8:branch_table_offset//4] == (
    0x2b1f00af, 0x4199037c, 0x3d800000, 0x398c0000,
    0x57e0103a, 0x7c0c002e, 0x7c0903a6, 0x4e800420)
start = branch_table_offset
table = []
for i in range(176):
    kind, (_, dest, target_sec, _) = rels[start+4*i]
    assert target_sec == sec and kind in (2,3)
    table.append(dest)
artist = bytes.fromhex(json.loads((artifact/'artist-bytes.json').read_bytes())['0x823a107c'])
assert struct.unpack_from('>I', artist)[0] == 0x4e800420
original = struct.unpack('>176I',artist[4:])
enum = (source/'BrnGameEvents.h').read_text(encoding='utf-8-sig').split('enum EGameEventType')[1].split('};',1)[0]
names = {n:int(v,0) for n,v in re.findall(r'(?m)^\s*(E_\w+)\s*=\s*(0x[\da-fA-F]+|\d+)\s*,?',enum)}
expected = {}
objects = {'dispatcher': (body, rels)}
for group in range(1,6):
    stem = f'GameStateModule_ProcessGameEvents_wBT_0{group}'
    raw = (source/(stem+'.cpp')).read_text(encoding='utf-8-sig')
    for name in re.findall(r'(?m)^    case (?:GameStateModuleIO::)?(\w+)\s*:',raw):
        i = int(name) if name.isdecimal() else names[name]
        assert i not in expected
        expected[i] = group, name
    selected, = [r for r in seal['selected'] if Path(r['source']).stem == stem]
    retained = Path(selected['object']).read_bytes()
    assert hashlib.sha256(retained).hexdigest() == selected['sha256']
    n, v, _, group_body, group_rels = code(retained, f'?ProcessGameEvents_Group{group}@')
    new_group = code(Path(rows[stem]['object']).read_bytes(), f'?ProcessGameEvents_Group{group}@')
    assert v == 0 and (n, v, group_body, group_rels)==(new_group[0],new_group[1],new_group[3],new_group[4])
    assert any(k==6 and s[0]==n for k,s in rels.values())
    objects[f'group{group}']=(group_body,group_rels)


def route(offset):
    for pc in range(offset, len(body), 4):
        if pc in rels:
            k,s = rels[pc]
            name = s[0]
            if k == 6 and 'ProcessGameEvents_Group' in name:
                return int(re.search(r'ProcessGameEvents_Group(\d)',name)[1])
            if k == 6 and 'BeginAssert@Assert' in name:
                return 'assert'
            if k == 6 and 'GetNextEvent' in name:
                return 'advance'
        word = words[pc//4]
        if word >> 26 == 18 and word & 3 == 0:
            delta = word & 0x03fffffc
            if delta >= 0x02000000:
                delta -= 0x04000000
            return route(pc+delta)
    raise AssertionError(f'Unknown route {offset:#x}')


cases=[]
for i,(target,compiled) in enumerate(zip(original,table)):
    group,name = ('assert',None) if target==0x823a4e80 else ('advance',None) if target==0x823a4ee0 else expected[i]
    assert route(compiled)==group,(i,hex(target),hex(compiled),route(compiled),group)
    cases.append({'event':i,'name':name,'artist':hex(target),'compiled_offset':hex(compiled),'route':group})
assert len(expected)==161
assert route(0x94+(words[0x94//4]&0xfffc))=='assert'
callseq=[s[0] for k,s in rels.values() if k==6]
find=lambda fragment:next(i for i,n in enumerate(callseq) if fragment in n)
assert find('StartMonitor') < find('GetTimerStatusInterface') < find('??4TimerStatusInterface@') < find('GetFirstEvent')
assert bytes.fromhex('3c7e000338632dc8') in body
assert find('GetNextEvent') < find('GetProfile') < find('AddEvent') < find('StopMonitor')
assert bytes.fromhex('38c00004') in body and bytes.fromhex('38a000c7') in body
assert bytes.fromhex('3ffe00043bff75c0') in body
assert bytes.fromhex('3c7e00013863bb30') in body
assert bytes.fromhex('396be638') in body
progression, = (r for r in seal['selected'] if r['source'].endswith('/BrnProgressionManager.cpp'))
progression_obj = Path(progression['object']).read_bytes()
assert hashlib.sha256(progression_obj).hexdigest() == progression['sha256']
assert code(progression_obj, '?GetProfile@ProgressionManager@')[3] == bytes.fromhex('386301704e800020')
assert 0xbb30 + 0x170 + 0xe638 == 0x1a2d8

need = {s[0] for _, section_rels in objects.values() for _,s in section_rels.values() if s[2]==0 and s[3]==2}
provider_syms,_=read_coff(provider)
need -= {n for n,v,s,k in provider_syms.values() if k==2 and s>0}
found=defaultdict(list)
def scan(row):
    path=Path(row['object'])
    try:
        data = path.read_bytes()
        syms,_=read_coff(data)
    except (ValueError,AssertionError,struct.error,UnicodeError):
        return []
    hits = [(n,row['source']) for n,_,sec,k in syms.values() if n in need and k==2 and sec>0]
    if hits:
        assert hashlib.sha256(data).hexdigest() == row['sha256'],row['source']
    assert not any(n == symbol and sec > 0 and k == 2 for n,_,sec,k in syms.values()),row['source']
    return hits
with ThreadPoolExecutor(max_workers=8) as pool:
    for r in pool.map(scan,seal['selected']):
        for n,p in r:
            found[n].append(p)
for pin in seal['libraries']:
    path=Path(pin['path'])
    data = path.read_bytes()
    assert hashlib.sha256(data).hexdigest() == pin['sha256']
    for n in set(archive_defs(data)) & need:
        found[n].append(path.name)

missing=sorted(need-found.keys())
out={'base':base,'routes':cases,'group_counts':{str(g):sum(c['route']==g for c in cases) for g in range(1,6)},
     'noops':[c['event'] for c in cases if c['route']=='advance'],
     'assertions':[c['event'] for c in cases if c['route']=='assert'],
     'sealed_caller':sealed_caller,'dispatcher_symbol':symbol,'dispatcher_branch_table_offset':hex(start),
     'provider_count':len(need),'missing_direct_providers':missing,
     'direct_providers':{n:sorted(set(p)) for n,p in sorted(found.items())}}
(artifact/'dispatch-verification.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8')
print({k:out[k] for k in ('group_counts','noops','assertions','provider_count','missing_direct_providers')})

