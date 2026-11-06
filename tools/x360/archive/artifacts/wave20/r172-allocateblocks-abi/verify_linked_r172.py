"""Verify pinned R172 AllocateBlocks ABI and prior bank reset witnesses."""
import argparse
import hashlib
import json
import re
import struct
import subprocess
import sys
from pathlib import Path

import pefile

sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/tools/re')
import x360rd

REPO='C:/Users/adri1/.codex/worktrees/bp360/native_link_inventory_luna'
CANDIDATE='34cbedc5316a1ee0592385ccb02c76cf06927365'
TREE='27d9d594a2eea753b0057137f4898cbc1d3c3900'
PARENT='b7b549c7f423d38c8ddc63ba089c11152e66d8fd'
SOURCE='src/GameShared/GameClasses/Memory/CgsMemoryModule.cpp'
ABI='?AllocateBlocks@MemoryModule@CgsMemory@@AAA_NGGG_NPAI1@Z'
ROOT='?CreateRootBank@MemoryModule@CgsMemory@@AAAXXZ'
CHILD='?AllocateIntoBank@MemoryModule@CgsMemory@@AAAHPAUBankAllocRequest@12@@Z'
INIT='?InitBanks@MemoryModule@CgsMemory@@AAAXXZ'
DESTROY='?DestroyBank@MemoryModule@CgsMemory@@AAAHH_N@Z'


def sha(b):return hashlib.sha256(b).hexdigest()
def check(ok,msg):
    if not ok:raise ValueError(msg)
def target(word,pc):
    check(word>>26==18 and word&3==1,'expected relative branch-and-link')
    v=word&0x03fffffc
    if v&0x02000000:v-=0x04000000
    return (pc+v)&0xffffffff


def parse_coff(data):
    machine,nsec,_,so,nn,opt,_=struct.unpack_from('<HHIIIHH',data)
    check(machine==0x1f2,'not PPC COFF')
    st=data[so+nn*18:];sy={};i=0
    while i<nn:
        raw,val,sec,typ,cl,aux=struct.unpack_from('<8sIhHBB',data,so+i*18)
        off=struct.unpack_from('<I',raw,4)[0] if raw[:4]==bytes(4) else 0
        name=(st[off:st.find(b'\0',off)] if off else raw.rstrip(b'\0')).decode('latin1')
        sy[i]=(name,val,sec);i+=1+aux
    def get(name):
        found=[v for v in sy.values() if v[0]==name and v[2]>0]
        check(len(found)==1 and found[0][1]==0,'bad COFF function '+name)
        h=struct.unpack_from('<8sIIIIIIHHI',data,20+opt+40*(found[0][2]-1))
        code=data[h[4]:h[4]+h[3]]
        rels=[(a,t,sy[k][0]) for j in range(h[7]) for a,k,t in [struct.unpack_from('<IIH',data,h[5]+10*j)]]
        return code,rels
    return get


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--pins',type=Path,required=True)
    ap.add_argument('--pins-sha256',required=True)
    ap.add_argument('--head',required=True)
    ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args()
    check(a.pins.name=='ROOT_LINK_PINS.json' and not a.output.exists(),'invalid pins/output')
    pb=a.pins.read_bytes();check(sha(pb)==a.pins_sha256.lower(),'pins hash mismatch')
    pins=json.loads(pb)
    check(pins['head']==a.head and pins['tree']==TREE and pins['parent']==PARENT and
          re.fullmatch('[0-9a-f]{40}',a.head),'wrong integration identity')
    def git(*args):return subprocess.check_output(['git','-C',REPO,*args],text=True).strip()
    check(git('rev-parse',a.head+'^{tree}')==TREE and git('rev-parse',CANDIDATE+'^{tree}')==TREE and
          git('rev-list','--parents','-n','1',a.head).split()==[a.head,PARENT],'commit/tree/parent mismatch')
    blobs={}
    for key in ('pe','map','seal','completion','result'):
        row=pins['pins'][key];blobs[key]=Path(row['path']).read_bytes()
        check(sha(blobs[key])==row['sha256'],key+' pin mismatch')
    seal,done,result=(json.loads(blobs[k]) for k in ('seal','completion','result'))
    check(seal['head']==a.head and done['head']==a.head and done['status']=='SEALED_NO_REGRESSIONS' and
          done['requested']==done['passed'] and done['failed']==0 and
          result['exit_code']==0 and result['all_post_checks_pass'],'link/intake failed')
    outputs={Path(k.replace('\\','/')).name:v['sha256'] for k,v in result['outputs'].items()}
    check(outputs['native-game.pe']==sha(blobs['pe']) and outputs['native-game.map']==sha(blobs['map']),'result mismatch')
    owner=[x for x in seal['selected'] if x['source'].replace('\\','/')==SOURCE]
    check(len(owner)==1,'wrong selected owner')
    obj=Path(owner[0]['object']).read_bytes();check(sha(obj)==owner[0]['sha256'],'object hash mismatch')
    get=parse_coff(obj)
    rx=re.compile(r'^\s*0003:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{8,16})\s+f\s+(?:i\s+)?(\S+)')
    rows=[]
    for line in blobs['map'].decode(errors='replace').splitlines():
        m=rx.match(line)
        if m:rows.append((m[1],int(m[2],16),m[3]))
    def addr(name):
        z=[(va,o) for n,va,o in rows if n==name]
        check(len({v for v,_ in z})==1 and any(o.startswith('CgsMemoryModule-') for _,o in z),'bad map owner '+name)
        return z[0][0]
    addresses=sorted({v for _,v,_ in rows})
    pe=pefile.PE(data=blobs['pe'],fast_load=True);base=pe.OPTIONAL_HEADER.ImageBase
    text=next(s for s in pe.sections if s.Name.rstrip(b'\0')==b'.text')
    def read(va,n):
        off=va-base-text.VirtualAddress
        check(0<=off and off+n<=min(text.Misc_VirtualSize,text.SizeOfRawData),'outside code')
        at=text.PointerToRawData+off
        return blobs['pe'][at:at+n]
    def get_link(name):
        data,rels=get(name);va=addr(name)
        check(min((x for x in addresses if x>va),default=base+text.VirtualAddress+text.Misc_VirtualSize)>=va+len(data),
              'function extent overlaps another symbol')
        return va,data,rels,read(va,len(data))
    va,objbody,rels,linked=get_link(ABI)
    proof={0x1c:0x7cbd2b78,0x20:0x7cdb3378,0x5c:0x57ab043e,0x60:0x2b0b00ff,
           0xc8:0x57bc063e,0xcc:0x577b063e,0x118:0x9b2b0009,0x11c:0x9b8b0008,
           0x120:0x9b6b000a,0x124:0x9b0b000b,0x178:0x2f1fffff,
           0x94:0x3960ffff,0x98:0x38600000,0x9c:0x91730000,0xa0:0x91720000,
           0x1ac:0x38600001,0x1b8:0x92d30000,0x1bc:0x91520000}
    for off,word in proof.items():
        check(struct.unpack_from('>I',objbody,off)[0]==word==struct.unpack_from('>I',linked,off)[0],
              'ABI/output word mismatch '+hex(off))
    witnesses=[]
    for name,off,checks in [(ROOT,0x2a8,{0x290:0xa09a0000,0x29c:0x5606043e,0x2a0:0x38a00000}),
                            (CHILD,0x254,{0x23c:0x5726043e,0x248:0x5645063e,0x250:0x7fa4eb78})]:
        cv,body,rr,lb=get_link(name)
        check((off,6,ABI) in rr,'missing selected caller relocation')
        for k,w in checks.items():
            check(struct.unpack_from('>I',body,k)[0]==w==struct.unpack_from('>I',lb,k)[0],'caller ABI mismatch')
        check(target(struct.unpack_from('>I',lb,off)[0],cv+off)==va,'caller target mismatch')
        witnesses.append({'caller':name,'call_site':hex(cv+off),'target':hex(va)})
    iv,ib,ir,il=get_link(INIT)
    check(len(ib)==164 and not ir and ib==il==x360rd.rd(0x828670d0,164),'InitBanks raw mismatch')
    dv,db,dr,dl=get_link(DESTROY)
    reset=x360rd.rd(0x828697b0,64);pos=db.find(reset)
    check(pos>=0 and db.find(reset,pos+1)<0 and dl[pos:pos+64]==reset and
          not any(pos<=off<pos+64 for off,_,_ in dr),'DestroyBank reset mismatch')
    report={'status':'pass','head':a.head,'tree':TREE,'pins_sha256':sha(pb),'pe_sha256':sha(blobs['pe']),
            'map_sha256':sha(blobs['map']),'selected_object_sha256':sha(obj),'allocateblocks':hex(va),
            'u16_bank_check_and_byte_stores_verified':True,'safe_ffffffff_terminator_verified':True,
            'success_failure_outputs_verified':True,'callers':witnesses,
            'initbanks_raw_exact':{'address':hex(iv),'bytes':164},
            'destroybank_reset_raw_exact':{'address':hex(dv+pos),'bytes':64},
            'scope':'selected COFF + linked PPC witnesses, not full AllocateBlocks match or runtime'}
    with a.output.open('x',encoding='utf-8') as f:json.dump(report,f,indent=2);f.write('\n')
    print(json.dumps(report,indent=2))


if __name__=='__main__':main()
