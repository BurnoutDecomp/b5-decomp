"""Conservative R173 straight-line leaf supplement; never modifies official census."""
import collections
import gzip
import hashlib
import importlib.util
import json
import re
import struct
import sys
from pathlib import Path

import pefile

sys.dont_write_bytecode = True
ROOT = Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave20')
LINK = ROOT / 'r173-link'
PINS_SHA = '181b603ce18ee655ebc666a6fcb33bf1d2f3d3410345c3dd5d7010782f004a2c'
HEAD = '2ddfc33a3ba62295655a2f897711e5d343df20b2'
OFFICIAL = ROOT / 'r156-byte-census/R173_CENSUS.json'
OUT = ROOT / 'r173-leaf-census/LEAF_SUPPLEMENT.json'


def sha(b): return hashlib.sha256(b).hexdigest()
def require(ok, why):
    if not ok: raise ValueError(why)


def coff_index(b):
    machine,nsec,_,so,nn,opt,_=struct.unpack_from('<HHIIIHH',b)
    require(machine==0x1f2,'not PPC COFF')
    st=b[so+nn*18:];sy={};i=0
    while i<nn:
        raw,val,sec,typ,cl,aux=struct.unpack_from('<8sIhHBB',b,so+i*18)
        off=struct.unpack_from('<I',raw,4)[0] if raw[:4]==bytes(4) else 0
        if off:
            require(4<=off<len(st),'invalid COFF string offset')
            end=st.find(b'\0',off);require(end>=0,'unterminated COFF symbol')
            name=st[off:end].decode('latin1')
        else:name=raw.rstrip(b'\0').decode('latin1')
        sy[i]=(name,val,sec,typ,cl);i+=1+aux
    byname=collections.defaultdict(list); bysection=collections.defaultdict(list)
    for sym in sy.values():
        if sym[2]>0:
            byname[sym[0]].append(sym)
            if sym[3]==0x20 or sym[4]==2:bysection[sym[2]].append(sym)
    def get(name):
        rows=byname.get(name,[])
        if len(rows)!=1:return None
        _,val,sec,typ,cl=rows[0]
        if val!=0 or typ!=0x20 or not 1<=sec<=nsec:return None
        if len(bysection[sec])!=1:return None
        h=struct.unpack_from('<8sIIIIIIHHI',b,20+opt+40*(sec-1))
        if h[0].rstrip(b'\0')!=b'.text' or h[7]!=0 or not (h[9]&0x20) or not (h[9]&0x1000):return None
        if h[3]==0 or h[4]+h[3]>len(b):return None
        return b[h[4]:h[4]+h[3]]
    return get


def main():
    require(not OUT.exists(),'refusing to overwrite existing supplement')
    p=(LINK/'ROOT_LINK_PINS.json').read_bytes();require(sha(p)==PINS_SHA,'root pins mismatch')
    pins=json.loads(p);require(pins['head']==HEAD,'wrong head')
    data={}
    for key in ('seal','invocation','result','completion','pe','map'):
        row=pins['pins'][key];data[key]=Path(row['path']).read_bytes()
        require(sha(data[key])==row['sha256'],key+' pin mismatch')
    seal,inv,result,done=(json.loads(data[x]) for x in ('seal','invocation','result','completion'))
    require(seal['head']==HEAD and inv['sealed_sha256']==sha(data['seal']) and done['head']==HEAD and
            done['status']=='SEALED_NO_REGRESSIONS' and done['passed']==done['requested'] and done['failed']==0 and
            result['exit_code']==0 and result['all_post_checks_pass'],'sealed invocation/intake failed')
    outputs={Path(k.replace('\\','/')).name:v['sha256'] for k,v in result['outputs'].items()}
    require(outputs['native-game.pe']==sha(data['pe']) and outputs['native-game.map']==sha(data['map']),'link result mismatch')
    official=json.loads(OFFICIAL.read_bytes())
    require(official['head']==HEAD and official['pe_sha256']==sha(data['pe']) and
            official['map_sha256']==sha(data['map']) and official['seal_sha256']==sha(data['seal']) and
            (official['eligible'],official['raw_exact'],official['verified_branch_displacement_only'])==(12383,44,293),
            'official census provenance/count mismatch')
    spec=importlib.util.spec_from_file_location('leaf_census_baseline',ROOT/'r156-byte-census/count_matches.py')
    c=importlib.util.module_from_spec(spec);spec.loader.exec_module(c)
    pe=pefile.PE(data=data['pe'],fast_load=True);base=pe.OPTIONAL_HEADER.ImageBase
    sections={s.Name.rstrip(b'\0'):s for s in pe.sections};t=sections[b'.text'];q=sections[b'.pdata']
    compiled=c.packed_extents(q.get_data()[:q.Misc_VirtualSize],base+t.VirtualAddress,base+t.VirtualAddress+t.Misc_VirtualSize)
    original_pdata=c.artist.rd(0x821bb000,0x821ed960-0x821bb000)
    original=c.packed_extents(original_pdata,0x821f0000,0x82c775d0)
    require(sha(original_pdata)==official['original_pdata_sha256'] and sha(c.NAMES.read_bytes())==official['original_name_pack_sha256'],
            'original census inputs differ')
    names=collections.defaultdict(list)
    for addr,row in json.loads(gzip.decompress(c.NAMES.read_bytes()))['functions'].items():names[row['name']].append(int(addr,16))
    linked,linked_symbols=c.map_functions(data['map'])
    mapall=collections.defaultdict(set)
    rx=re.compile(r'^\s*0003:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{8,16})(?:\s|$)')
    for line in data['map'].decode(errors='replace').splitlines():
        m=rx.match(line)
        if m:mapall[int(m[2],16)].add(m[1])
    selected={Path(row['object']).name:row for row in seal['selected']}
    require(len(selected)==len(seal['selected']),'ambiguous selected object basenames')
    invoked={str(Path(row['object']).resolve()).casefold():row for row in inv['objects']}
    require(len(invoked)==len(inv['objects']),'ambiguous invocation object paths')
    official_names={x['name'] for x in official['raw_exact_functions']+official['branch_displacement_only_functions']}
    cache={};exclusions=collections.Counter(); matches=[]; candidates=0
    for name, addresses in names.items():
        rows=linked.get(name,())
        if len(addresses)!=1 or len(rows)!=1:continue
        va,owner,mangled=next(iter(rows)); old=addresses[0]
        if len(linked_symbols[va])!=1:continue
        if old in original and va in compiled:continue
        candidates+=1
        row=selected.get(owner)
        if not row or not row['source'].startswith('src/'):
            exclusions['outside src ownership']+=1;continue
        if name in official_names or len(mapall[va])!=1 or mangled not in mapall[va]:
            exclusions['official duplicate or code alias']+=1;continue
        path=c.EXPORTS/f'0x{old:08X}.json'
        if not path.exists():exclusions['no original export']+=1;continue
        exp=json.loads(path.read_bytes())
        if (exp.get('name')!=name or name.startswith(('sub_','nullsub_','j_','thunk_')) or
                'thunk' in name.lower() or mangled.startswith(('??_G','??_E'))):
            exclusions['original name or thunk']+=1;continue
        lines=exp.get('assembly','').splitlines(); ins=[]; padding=False; good=True
        for line in lines:
            m=re.match(r'^0x([0-9A-Fa-f]+)\s+(\S+)(.*)$',line)
            if not m:good=False;break
            address=int(m[1],16);op=m[2].lower();tail=m[3].strip()
            if op=='.long' and tail in ('0','0x0'):
                padding=True;continue
            if padding:good=False;break
            ins.append((address,op))
        if not good or not ins or len(ins)>1024 or [a for a,_ in ins]!=list(range(old,old+4*len(ins),4)):
            exclusions['noncontiguous or ambiguous original export']+=1;continue
        if ins[-1][1]!='blr' or any(op.startswith('b') for _,op in ins[:-1]):
            exclusions['not straight-line sole return']+=1;continue
        size=4*len(ins)
        try: orig=c.artist.rd(old,size)
        except KeyError:exclusions['unmapped original']+=1;continue
        words=[z[0] for z in struct.iter_unpack('>I',orig)]
        if words[-1]!=0x4e800020 or any((w>>26) in (16,18,19) for w in words[:-1]):
            exclusions['machine branch or nonreturn']+=1;continue
        resolved=str(Path(row['object']).resolve()).casefold();ir=invoked.get(resolved)
        if not ir or ir['sha256']!=row['sha256'] or ir['source']!=row['source']:
            exclusions['selected/invocation disagreement']+=1;continue
        if resolved not in cache:
            b=Path(row['object']).read_bytes();require(sha(b)==row['sha256'],'selected object hash mismatch')
            cache[resolved]=coff_index(b)
        body=cache[resolved](mangled)
        if body is None or len(body)!=size:
            exclusions['COFF function extent, alias or relocation']+=1;continue
        rva=va-base-t.VirtualAddress
        if not 0<=rva<=rva+size<=min(t.Misc_VirtualSize,t.SizeOfRawData):
            exclusions['outside linked code']+=1;continue
        off=t.PointerToRawData+rva; final=data['pe'][off:off+size]
        if not(orig==body==final):exclusions['raw bytes differ']+=1;continue
        matches.append({'name':name,'artist':f'0x{old:08X}','linked':f'0x{va:08X}',
                        'bytes':size,'source':row['source'],'object_sha256':row['sha256'],
                        'body_sha256':sha(body),'original_export':str(path)})
    report={'schema':'r173-straight-leaf-supplement-v1','head':HEAD,'pins_sha256':sha(p),
            'official_census_sha256':sha(OFFICIAL.read_bytes()),'official_eligible':12383,'official_raw_exact':44,
            'official_branch_only':293,'missing_packed_unique_name_candidates':candidates,'excluded':exclusions,
            'additional_raw_exact_count':len(matches),'additional_raw_exact_bytes':sum(x['bytes'] for x in matches),
            'matches':matches,'limitations':'Named original export entries only; sole straight-line blr, selected zero-relocation whole COFF code section, unique map-visible linked symbol, raw bytes equal. Map/COFF checks exclude visible aliases but cannot rule out unexposed linker or original-binary aliases. Not total program coverage; no vtable-only anchors or runtime claims.'}
    with OUT.open('x',encoding='utf-8') as f:json.dump(report,f,indent=2);f.write('\n')
    print(json.dumps({k:v for k,v in report.items() if k!='matches'},indent=2))


if __name__=='__main__':main()
