"""Verify R171's full InitBanks and DestroyBank reset subrange, read-only."""
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
DONOR='0551b40b49d8f682e7586b178f9b244065db6a3e'
HEADER='src/GameShared/GameClasses/Memory/CgsMemoryBank.h'
SOURCE='src/GameShared/GameClasses/Memory/CgsMemoryModule.cpp'
INIT='?InitBanks@MemoryModule@CgsMemory@@AAAXXZ'
DESTROY='?DestroyBank@MemoryModule@CgsMemory@@AAAHH_N@Z'


def sha(data): return hashlib.sha256(data).hexdigest()


def check(ok, message):
    if not ok: raise ValueError(message)


def coff(data):
    machine,nsec,_,so,nn,opt,_=struct.unpack_from('<HHIIIHH',data)
    check(machine==0x1f2,'not PPC COFF')
    st=data[so+nn*18:]; sy={}; i=0
    while i<nn:
        raw,val,sec,typ,cl,aux=struct.unpack_from('<8sIhHBB',data,so+i*18)
        off=struct.unpack_from('<I',raw,4)[0] if raw[:4]==bytes(4) else 0
        name=(st[off:st.find(b'\0',off)] if off else raw.rstrip(b'\0')).decode('latin1')
        sy[i]=(name,val,sec); i+=1+aux
    def get(name):
        rows=[z for z in sy.values() if z[0]==name and z[2]>0]
        check(len(rows)==1 and rows[0][1]==0,'ambiguous COFF function')
        h=struct.unpack_from('<8sIIIIIIHHI',data,20+opt+40*(rows[0][2]-1))
        body=data[h[4]:h[4]+h[3]]
        rels=[(a,t,sy[k][0]) for j in range(h[7]) for a,k,t in [struct.unpack_from('<IIH',data,h[5]+10*j)]]
        return body,rels
    return get


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--pins',type=Path,required=True)
    ap.add_argument('--pins-sha256',required=True)
    ap.add_argument('--head',required=True)
    ap.add_argument('--parent',required=True)
    ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args()
    check(a.pins.name=='ROOT_LINK_PINS.json' and not a.output.exists(),'invalid pins/output')
    raw=a.pins.read_bytes();check(sha(raw)==a.pins_sha256.lower(),'pins hash mismatch')
    pins=json.loads(raw)
    check(pins['head']==a.head and pins['parent']==a.parent and
          re.fullmatch('[0-9a-f]{40}',a.head) and re.fullmatch('[0-9a-f]{40}',a.parent),'identity mismatch')
    def git(*args):return subprocess.check_output(['git','-C',REPO,*args],text=True).strip()
    check(git('rev-parse',a.head+'^{tree}')==pins['tree'] and
          git('rev-list','--parents','-n','1',a.head).split()==[a.head,a.parent],'git identity mismatch')
    check(git('rev-parse',a.head+':'+HEADER)==git('rev-parse',DONOR+':'+HEADER),'donor header differs')
    blobs={}
    for key in ('pe','map','seal','completion','result'):
        row=pins['pins'][key];blobs[key]=Path(row['path']).read_bytes()
        check(sha(blobs[key])==row['sha256'],key+' hash mismatch')
    seal,complete,result=(json.loads(blobs[k]) for k in ('seal','completion','result'))
    check(seal['head']==a.head and complete['head']==a.head and complete['status']=='SEALED_NO_REGRESSIONS' and
          complete['requested']==complete['passed'] and complete['failed']==0 and
          result['exit_code']==0 and result['all_post_checks_pass'],'seal/intake/result failed')
    outputs={Path(k.replace('\\','/')).name:v['sha256'] for k,v in result['outputs'].items()}
    check(outputs['native-game.pe']==sha(blobs['pe']) and outputs['native-game.map']==sha(blobs['map']),'result mismatch')
    selected=[x for x in seal['selected'] if x['source'].replace('\\','/')==SOURCE]
    check(len(selected)==1,'missing/duplicate selected owner')
    obj=Path(selected[0]['object']).read_bytes()
    check(sha(obj)==selected[0]['sha256'],'selected object hash mismatch')
    get=coff(obj)
    rx=re.compile(r'^\s*0003:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{8,16})\s+f\s+(?:i\s+)?(\S+)')
    rows=[]
    for line in blobs['map'].decode(errors='replace').splitlines():
        m=rx.match(line)
        if m: rows.append((m[1],int(m[2],16),m[3]))
    def addr(name):
        rows_for=[(v,o) for n,v,o in rows if n==name]
        check(len({v for v,_ in rows_for})==1 and any(o.startswith('CgsMemoryModule-') for _,o in rows_for),
              'missing/ambiguous/wrong owner: '+name)
        return rows_for[0][0]
    pe=pefile.PE(data=blobs['pe'],fast_load=True);base=pe.OPTIONAL_HEADER.ImageBase
    section=next(x for x in pe.sections if x.Name.rstrip(b'\0')==b'.text')
    def read(va,n):
        off=va-base-section.VirtualAddress
        check(0<=off and off+n<=min(section.Misc_VirtualSize,section.SizeOfRawData),'code out of bounds')
        start=section.PointerToRawData+off
        return blobs['pe'][start:start+n]
    sizes=sorted({va for _,va,_ in rows})
    init,init_rels=get(INIT);destroy,destroy_rels=get(DESTROY)
    init_va,destroy_va=addr(INIT),addr(DESTROY)
    check(len(init)==164 and not init_rels and init==x360rd.rd(0x828670d0,164),'InitBanks COFF mismatch')
    check(min((x for x in sizes if x>init_va),default=base+section.VirtualAddress+section.Misc_VirtualSize)>=init_va+164,
          'intervening InitBanks symbol')
    check(read(init_va,164)==init,'linked InitBanks mismatch')
    original_reset=x360rd.rd(0x828697b0,64)
    pos=destroy.find(original_reset)
    check(pos>=0 and destroy.find(original_reset,pos+1)<0,'missing/ambiguous reset subrange')
    check(not any(pos<=off<pos+64 for off,_,_ in destroy_rels),'reset has COFF relocation')
    check(min((x for x in sizes if x>destroy_va),default=base+section.VirtualAddress+section.Misc_VirtualSize)>=destroy_va+len(destroy),
          'intervening DestroyBank symbol')
    check(read(destroy_va+pos,64)==original_reset,'linked reset mismatch')
    for body in (init,original_reset):
        words=[x[0] for x in struct.iter_unpack('>I',body)]
        check(not any(w>>26==38 and w&0xffff==0x53 for w in words),'unexpected flag-byte store')
    report={'status':'pass','head':a.head,'tree':pins['tree'],'parent':a.parent,'pins_sha256':sha(raw),
            'pe_sha256':sha(blobs['pe']),'map_sha256':sha(blobs['map']),'header_blob':git('rev-parse',a.head+':'+HEADER),
            'selected_object_sha256':sha(obj),'initbanks':{'original':'0x828670d0','linked':hex(init_va),
            'bytes':164,'raw_exact':True,'relocations':0},'destroybank_reset':{'original':'0x828697b0',
            'linked':hex(destroy_va+pos),'offset':hex(pos),'bytes':64,'raw_exact':True,'relocations':0},
            'reset_flag_byte_store_absent':True,'scope':'full InitBanks, bounded DestroyBank reset; static PE, no runtime'}
    with a.output.open('x',encoding='utf-8') as f:json.dump(report,f,indent=2);f.write('\n')
    print(json.dumps(report,indent=2))


if __name__=='__main__':main()
