"""Verify the pinned R170 MemoryModule owner and lifecycle in the linked PE."""
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

REPO = 'C:/Users/adri1/.codex/worktrees/bp360/native_link_inventory_luna'
CANDIDATE = '147ef0add704f1bf2f0c7b86de113372c579910d'
TREE = '294aed21c77b70a6e403903e07c0218098c0dd63'
PARENT = '6bc4e0d6d9d5e6af3e664764e9f330fac90d1b93'
SOURCE = 'src/GameShared/GameClasses/Memory/CgsMemoryModule.cpp'
PREPARE = '?Prepare@MemoryModule@CgsMemory@@UAA_NXZ'
CONSTRUCT = '?Construct@MemoryModule@CgsMemory@@UAAXPAUInitOptions@12@PAUIResourceAllocator@rw@@@Z'
BASE = '?Construct@ModuleSingleBuffered@CgsModule@@UAAXXZ'
SCRATCH = '?Construct@LinearMalloc@CgsMemory@@QAAXXZ'
CREATE = '?Create@LinearMalloc@CgsMemory@@QAAXPAXI@Z'
INIT = ['?InitBanks@MemoryModule@CgsMemory@@AAAXXZ', '?InitBlocks@MemoryModule@CgsMemory@@AAAXXZ',
        '?CreateRootBank@MemoryModule@CgsMemory@@AAAXXZ']
MESSAGE = b'Invalid stage\n'
FILE = b'd:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\memory\\CgsMemoryModule.cpp'


def sha(b):
    return hashlib.sha256(b).hexdigest()


def check(ok, message):
    if not ok:
        raise ValueError(message)


def target(word, pc):
    check(word >> 26 == 18 and word & 2 == 0, 'expected PPC relative branch')
    d = word & 0x03fffffc
    if d & 0x02000000:
        d -= 0x04000000
    return (pc + d) & 0xffffffff


def parse_coff(b):
    machine, nsec, _, so, nn, opt, _ = struct.unpack_from('<HHIIIHH', b)
    check(machine == 0x1f2, 'not PPC COFF')
    st = b[so+nn*18:]
    sy = {}
    i = 0
    while i < nn:
        raw, val, sec, _, _, aux = struct.unpack_from('<8sIhHBB', b, so+i*18)
        off = struct.unpack_from('<I', raw, 4)[0] if raw[:4] == bytes(4) else 0
        name = (st[off:st.find(b'\0',off)] if off else raw.rstrip(b'\0')).decode('latin1')
        sy[i] = (name,val,sec)
        i += 1+aux
    def get(name):
        found = [z for z in sy.values() if z[0] == name and z[2] > 0]
        check(len(found)==1 and found[0][1]==0, 'ambiguous COFF function')
        sec = found[0][2]
        h = struct.unpack_from('<8sIIIIIIHHI', b,20+opt+40*(sec-1))
        body = b[h[4]:h[4]+h[3]]
        rels = [(a,t,sy[k]) for j in range(h[7])
                for a,k,t in [struct.unpack_from('<IIH',b,h[5]+10*j)]]
        return sec, body, rels
    return get


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--pins',type=Path,required=True)
    ap.add_argument('--pins-sha256',required=True)
    ap.add_argument('--head',required=True)
    ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args()
    check(a.pins.name=='ROOT_LINK_PINS.json' and not a.output.exists(), 'invalid pins/output')
    pin_data=a.pins.read_bytes()
    check(sha(pin_data)==a.pins_sha256.lower(), 'pins hash mismatch')
    pins=json.loads(pin_data)
    check(pins['head']==a.head and pins['tree']==TREE and pins['parent']==PARENT and
          re.fullmatch('[0-9a-f]{40}',a.head), 'wrong integration identity')
    def git(*args):
        return subprocess.check_output(['git','-C',REPO,*args],text=True).strip()
    check(git('rev-parse',a.head+'^{tree}')==TREE and git('rev-parse',CANDIDATE+'^{tree}')==TREE,
          'candidate/integration trees differ')
    check(git('rev-list','--parents','-n','1',a.head).split()==[a.head,PARENT], 'wrong git parent')
    check(git('rev-parse',a.head+':'+SOURCE)==git('rev-parse',CANDIDATE+':'+SOURCE), 'owner source differs')
    blobs={}
    for key in ('pe','map','seal','completion','result'):
        row=pins['pins'][key]
        blobs[key]=Path(row['path']).read_bytes()
        check(sha(blobs[key])==row['sha256'],key+' hash mismatch')
    seal,complete,result=(json.loads(blobs[k]) for k in ('seal','completion','result'))
    check(seal['head']==a.head and complete['head']==a.head and complete['status']=='SEALED_NO_REGRESSIONS' and
          complete['requested']==complete['passed'] and complete['failed']==0 and
          result['exit_code']==0 and result['all_post_checks_pass'], 'seal/intake/result failed')
    outputs={Path(k.replace('\\','/')).name:v['sha256'] for k,v in result['outputs'].items()}
    check(outputs['native-game.pe']==sha(blobs['pe']) and outputs['native-game.map']==sha(blobs['map']), 'result mismatch')
    selected=[x for x in seal['selected'] if x['source'].replace('\\','/')==SOURCE]
    check(len(selected)==1,'missing/duplicate selected owner')
    obj=Path(selected[0]['object']).read_bytes()
    check(sha(obj)==selected[0]['sha256'], 'selected object hash mismatch')
    get=parse_coff(obj)
    symbols={}
    funcs=set()
    owners={}
    rx=re.compile(r'^\s*([0-9a-fA-F]{4}):[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{8,16})(?:\s+(.*))?$')
    for line in blobs['map'].decode(errors='replace').splitlines():
        m=rx.match(line)
        if not m: continue
        section,name,va,tail=int(m[1],16),m[2],int(m[3],16),m[4] or ''
        symbols.setdefault(name,set()).add(va)
        if section==3 and re.match(r'f(?:\s|$)',tail):
            funcs.add(va)
            owners.setdefault(name,[]).append(tail)
    def addr(name):
        values=symbols.get(name,set())
        check(len(values)==1,'missing/ambiguous symbol: '+name)
        return next(iter(values))
    pe=pefile.PE(data=blobs['pe'],fast_load=True)
    base=pe.OPTIONAL_HEADER.ImageBase
    def read(va,size):
        rva=va-base
        sections=[s for s in pe.sections if s.VirtualAddress<=rva and
                  rva+size<=s.VirtualAddress+min(s.Misc_VirtualSize,s.SizeOfRawData)]
        check(len(sections)==1,'invalid PE mapped address')
        s=sections[0];start=s.PointerToRawData+rva-s.VirtualAddress
        return blobs['pe'][start:start+size]
    results={}
    for name in (CONSTRUCT,PREPARE):
        sec, body, rels=get(name)
        va=addr(name)
        check(any('CgsMemoryModule-0d269f92.obj' in s for s in owners.get(name,[])), 'wrong owner')
        later=[f for f in funcs if f>va]
        check(not later or min(later)>=va+len(body), 'function extent exceeds next symbol')
        linked=read(va,len(body))
        by={off:(typ,sym) for off,typ,sym in rels if typ!=18}
        check(all(t in (2,6,16,17,18) for _,t,_ in rels),'unknown relocation type')
        for off in range(0,len(body),4):
            cv,lv=(struct.unpack_from('>I',x,off)[0] for x in (body,linked))
            typ,sym=by.get(off,(None,None))
            mask=0 if typ==2 else 0xfc000003 if typ==6 else 0xffff0000 if typ in (16,17) else 0xffffffff
            check(cv&mask==lv&mask,name+' fixed bits differ at '+hex(off))
            if typ is None: continue
            symname,value,symsec=sym
            resolved=va+value if symsec==sec else addr(symname)
            if typ==2:
                check(lv==(resolved+cv)&0xffffffff,'jump table relocation mismatch')
            elif typ==6:
                check(target(lv,va+off)==resolved,'branch target mismatch: '+symname)
            elif typ==16:
                check((lv&0xffff)==((resolved+0x8000)>>16)&0xffff,'high address mismatch: '+symname)
            elif typ==17:
                check((lv&0xffff)==resolved&0xffff,'low address mismatch: '+symname)
        calls=[(off,sym[0]) for off,typ,sym in rels if typ==6]
        results[name]={'va':va,'body':linked,'calls':calls}
    pc=results[PREPARE]; cc=results[CONSTRUCT]
    calls=pc['calls']
    relevant=[(off,name) for off,name in calls if name in INIT]
    check(relevant==[(0x7c,INIT[0]),(0x80,INIT[1]),(0x84,INIT[2])], 'bank-stage call order mismatch')
    check([(off,name) for off,name in cc['calls'] if name in (BASE,SCRATCH,CREATE)]==
          [(0x18,BASE),(0xb8,SCRATCH),(0x138,CREATE)],'construct order mismatch')
    expected={0x50:0x93df0228,0x5c:0x917f0228,0x6c:0x419a0080,0x70:0x39600002,
              0x78:0x917f0228,0xcc:0x38a0009f}
    for off,word in expected.items():
        check(struct.unpack_from('>I',pc['body'],off)[0]==word,'stage/line witness mismatch '+hex(off))
    check(struct.unpack_from('>I',cc['body'],0xe4)[0]==0x3b200001 and
          struct.unpack_from('>I',cc['body'],0x26c)[0]==0x9b3f0004,'derived flag store mismatch')
    check(read(addr(BASE),56)==x360rd.rd(0x8286e768,56),'base flag construct mismatch')
    check(x360rd.rd(0x8286af58,4)==bytes.fromhex('9b5c0004'),'ARTIST derived flag changed')
    check(x360rd.rd(0x820dd228,len(MESSAGE)+1)==MESSAGE+b'\0' and
          x360rd.rd(0x820e7118,len(FILE)+1)==FILE+b'\0','original diagnostic mismatch')
    for literal in (MESSAGE,FILE):
        check(literal+b'\0' in obj,'selected object literal absent')
        matches=[]
        for off,typ,sym in get(PREPARE)[2]:
            if typ==16 and sym[0].startswith('??_C@'):
                va=addr(sym[0])
                try:
                    if read(va,len(literal)+1)==literal+b'\0': matches.append(va)
                except ValueError: pass
        check(len(set(matches))==1,'linked diagnostic literal mismatch')
    report={'status':'pass','head':a.head,'tree':TREE,'pins_sha256':sha(pin_data),
            'pe_sha256':sha(blobs['pe']),'map_sha256':sha(blobs['map']),
            'selected_object_sha256':sha(obj),'selected_object':selected[0]['object'],
            'construct':hex(cc['va']),'prepare':hex(pc['va']),'full_coff_bits_and_relocation_targets_verified':True,
            'scratch_order':[hex(cc['va']+0xb8),hex(cc['va']+0x138)],
            'stage2_call_sites':[hex(pc['va']+x) for x in (0x7c,0x80,0x84)],
            'diagnostic':'Invalid stage\n','diagnostic_line':159,'flag_lifecycle_witness':True,
            'scope':'static linked PE/selected COFF, not ARTIST byte match or runtime proof'}
    with a.output.open('x',encoding='utf-8') as f:
        json.dump(report,f,indent=2);f.write('\n')
    print(json.dumps(report,indent=2))


if __name__=='__main__': main()
