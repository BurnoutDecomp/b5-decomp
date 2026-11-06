#!/usr/bin/env python3
"""Verify the pinned R156 strict-linked startup and action-77 consumer paths."""
import hashlib, json, re, struct, subprocess
from pathlib import Path

W=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave20")
OUT=Path(__file__).resolve().parent
REPO=Path(r"C:\Users\adri1\.codex\worktrees\bp360\native-sound")
LINK=W/"r156-link"; BASE=W/"r155-link"; INTAKE=W/"r156-intake-v2"; SEL=INTAKE/"native-selected"
R155="1d617fd4fdbacdc73faee7dfffd7a8d43cc6b9b1"
B521="b521454a05f57df74a009c39c89c47af55877d4d"
C9708="9708f3b6910954ea62a0a118405b4f9b23c163c6"
HEAD="8157850d32d5d15c6187b22969e939f5ca7c201d"
TREE="f49d1bf3c086e3a3ac5e5aaae120c1acae04b25a"; TREE_B521="74cc11db3f78b4ded89afcfab097c96ed2b3fc49"
PINS={"seal":"6376515b0c2ab708d8cd8e6d3ec4eeb635826fa8a7f40bbee77799ec8c43f1c4",
      "xex":"e1dd2fa9321932520dc561f491eb9e32f19029c6cd599a38f0a8a1302a083ed2",
      "pe":"d75e7d429422a174df8788ba55bd979460a348ebd7f18339e65a54225a3873c8",
      "map":"1dfdb006eff111042c95e4ce77de3d2c8e605c1fa748540c0c737189c0f8688e",
      "result":"01f55c0ae37a1c8f2db3e952fbfd31e805f7f38140bdb5cf5d418f133915fc05",
      "inventory":"68b3f3e8f0fc78b39b6e283c2d252cdab051c61b9393b2d208cd289fa50620ba",
      "fresh_compile":"29e233863368ba9d77092dce9c7043d6d8fd12a8d1195b97364210ac361545a3",
      "full_compile":"37c59ad2ced3d6a22ddaa91ba4fdd446d89de6a71a0c7c9fbcdfa4de0570c891",
      "commit_binding":"2c98c433dd59b67dfa87a6d97625ccae9dc09aa24702a846bb8cfc6616a41469"}
CAR="src/GameSource/GameState/CarSelect/BrnCarSelectManager.cpp"; MAIN="src/GameSource/Main/BrnMainX360.cpp"
DIR="src/GameSource/Director/BrnMainDirector.cpp"
RACE="src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.cpp"
EXIT="?UpdateExitState@CarSelectManager@BrnGameState@@AAAXPAV?$VariableEventQueue@$0DEAA@$0BA@@CgsModule@@@Z"
ADD="?AddEvent@?$VariableEventQueue@$0DEAA@$0BA@@CgsModule@@QAA_NPBUEvent@2@HH@Z"
GET="?GetRuntimeAllocator@BrnResource@@YAPAVHeapResourceAllocator@1@XZ"
CONSTRUCT="?Construct@Allocators@BrnResource@@SAXXZ"
ALLOC="?AllocateMemoryResource@IResourceAllocator@rw@@QAAPAXIIPBD@Z"
DIRECTOR="?ProcessInputQueue@MainDirector@BrnDirector@@QAAXPBUDirectorInputOutput@2@@Z"
RACECAR="?HandleGameActions@RaceCarEntityModule@BrnWorld@@QAAXPAUInputBuffer_PreScene@RaceCarEntityModuleIO@2@PAUOutputBuffer_PreScene@42@@Z"
GET_ACTIVE="?GetActiveRaceCar@RaceCarEntityModule@BrnWorld@@QAAPAVActiveRaceCar@2@W4EActiveRaceCarIndex@@@Z"
GET_GLOBAL="?GetGlobalRaceCar@ActiveRaceCar@BrnWorld@@QBAPAVRaceCar@2@XZ"
GET_VEHICLE_INDEX="?GetVehicleIndex@VehicleList@BrnResource@@QBAH_K@Z"
GET_VEHICLE_DATA="?GetVehicleData@VehicleList@BrnResource@@QBAPBUVehicleListEntry@2@H@Z"
CAR_STATS="?HandleCarStatsUpdate@RaceCarEntityModule@BrnWorld@@QAAXW4ECarType@BrnResource@@HH@Z"
CAR_TRAINING="?HandleCarTypeTrainingMessage@RaceCarEntityModule@BrnWorld@@QAAXI@Z"
EVENTS_155=[(76,8),(70,1),(99,1),(0,80),(7,48),(149,4),(55,1)]
EVENTS_156=[(76,8),(70,1),(77,32),(99,1),(0,80),(7,48),(149,4),(55,1)]
MASKS={6:0x03fffffc,16:0xffff,17:0xffff,18:0,2:0xffffffff}
MAP_RE=re.compile(r"^\s*[0-9a-fA-F]{4}:([0-9a-fA-F]{8})\s+(\S+)\s+([0-9a-fA-F]{8})\s+(.*?)\s*$")
ASM_RE=re.compile(r"^\s+([0-9a-fA-F]{5})\s+([0-9a-fA-F]{8})\s+(.*)$")
LABEL_RE=re.compile(r"^\s*(\$\S+):\s*$"); DCD_RE=re.compile(r"\bDCD\s+\|([^|]+)\|")

def sha(p):
    with Path(p).open('rb') as f: return hashlib.file_digest(f,'sha256').hexdigest()
def sha_bytes(b): return hashlib.sha256(b).hexdigest()
def jread(p): return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def git(*args): return subprocess.check_output(['git','-C',str(REPO),*args],stderr=subprocess.STDOUT).decode().strip()
def git_bytes(spec): return subprocess.check_output(['git','-C',str(REPO),'show',spec],stderr=subprocess.STDOUT)
def norm(p): return str(p).replace('/','\\').casefold()

# Reuse the established COFF function extractor, excluding its CLI.
_ns={'__file__':str(W/'r127-batch'/'verify_append_coff.py')}
exec((W/'r127-batch'/'verify_append_coff.py').read_text(encoding='utf-8-sig').split('\nif __name__ == "__main__":',1)[0],_ns)
coff=_ns['coff']
def obj_symbol(data,i):
    _,_,_,tab,count,_,_=struct.unpack_from('<HHIIIHH',data); assert i<count
    pos=tab+i*18; raw=data[pos:pos+8]
    if raw.startswith(bytes(4)):
        start=tab+count*18+struct.unpack_from('<I',raw,4)[0]; name=data[start:data.index(0,start)].decode('utf-8','replace')
    else: name=raw.rstrip(bytes(1)).decode('utf-8','replace')
    val,sec=struct.unpack_from('<Ih',data,pos+8); return name,val,sec
def collect(path,sym):
    path=Path(path); data=path.read_bytes(); coff['symbol']=sym.encode(); name,body,rels=coff['read_function'](path)
    assert name==sym,(name,sym,path); return data,body,rels

def parse_map(path):
    rows=[]
    for line in Path(path).read_text(errors='replace').splitlines():
        m=MAP_RE.match(line)
        if m:
            om=re.search(r'([^\s]+\.obj)\s*$',m.group(4),re.I)
            rows.append({'name':m.group(2),'address':int(m.group(3),16),'object':om.group(1) if om else None,'line':line})
    return rows
def map_symbol(rows,name):
    found=[r for r in rows if r['name']==name]; unique={(r['address'],r['object']):r for r in found}
    assert len(unique)==1,(name,[r['line'] for r in found[:8]])
    return next(iter(unique.values()))
def parse_pe(path):
    data=Path(path).read_bytes(); peoff=struct.unpack_from('<I',data,0x3c)[0]; assert data[peoff:peoff+4]==b'PE\0\0'
    machine,nsec,_,_,_,optsize,_=struct.unpack_from('<HHIIIHH',data,peoff+4); opt=peoff+24
    assert struct.unpack_from('<H',data,opt)[0]==0x10b
    entry_rva=struct.unpack_from('<I',data,opt+16)[0]; base=struct.unpack_from('<I',data,opt+28)[0]
    sections=[]; table=opt+optsize
    for i in range(nsec):
        name,vsize,rva,rsize,roff=struct.unpack_from('<8sIIII',data,table+i*40)
        sections.append((name.rstrip(bytes(1)).decode('ascii','replace'),rva,rsize,roff))
    def read(va,size):
        rva=va-base
        for _,start,rsize,roff in sections:
            if start<=rva and rva+size<=start+rsize:
                off=roff+rva-start; return data[off:off+size]
        raise ValueError(f'VA {va:#x} size {size:#x} not file-backed')
    return {'machine':machine,'base':base,'entry':base+entry_rva,'read':read}
def be32(data,off): return struct.unpack_from('>I',data,off)[0]
def branch_target(pc,w):
    assert w>>26==18 and not(w&2),(hex(pc),hex(w))
    disp=w&0x03fffffc
    if disp&0x02000000: disp-=0x04000000
    return pc+disp
def cond_target(pc,w):
    assert w>>26==16,(hex(pc),hex(w))
    disp=w&0xfffc
    if disp&0x8000: disp-=0x10000
    return disp if (w&2) else pc+disp

def fixed_equal(obj,linked,relocs):
    assert len(obj)==len(linked); masks={}
    for off,_,kind in relocs:
        assert kind in MASKS and off%4==0,(hex(off),kind)
        masks[off]=masks.get(off,0)|MASKS[kind]
    for off in range(0,len(obj),4):
        assert not((be32(obj,off)^be32(linked,off))&~masks.get(off,0)),hex(off)
def asm_function(path,sym):
    lines=Path(path).read_text(errors='replace').splitlines(); start=None
    for i,line in enumerate(lines):
        if ' PROC NEAR' in line and line.split(' PROC NEAR',1)[0].strip()==sym: start=i; break
    assert start is not None,(path,sym)
    end=next((i for i in range(start+1,len(lines)) if sym+' ENDP' in lines[i]),None); assert end is not None
    labels={}; dcd={}; pending=None
    for line in lines[start+1:end]:
        m=LABEL_RE.match(line)
        if m: pending=m.group(1); continue
        m=ASM_RE.match(line)
        if not m: continue
        off=int(m.group(1),16); tail=m.group(3)
        if pending: labels[pending]=off; pending=None
        d=DCD_RE.search(tail)
        if d: dcd[off]=d.group(1)
    return {'labels':labels,'dcd':dcd}
def call_rows(data,relocs):
    return [(off,obj_symbol(data,idx)[0]) for off,idx,kind in relocs if kind==6]
def verify_calls(rows,va,linked,maprows,names):
    out={}
    for name in names:
        sites=[off for off,target in rows if target==name]; target=map_symbol(maprows,name)['address']
        for off in sites: assert branch_target(va+off,be32(linked,off))==target,(name,hex(off))
        out[name]={'count':len(sites),'sites':[hex(x) for x in sites],'target_va':hex(target)}
    return out
def imm(body,call,reg):
    pat=0x3800+32*reg
    for off in range(call-4,max(-4,call-0x64),-4):
        w=be32(body,off)
        if w>>16==pat: return w&0xffff
    raise AssertionError((hex(call),reg))
def event_seq(data,body,rels):
    return [(off,imm(body,off,5),imm(body,off,6)) for off,idx,kind in rels if kind==6 and obj_symbol(data,idx)[0]==ADD]
def source_row(inventory,suffix):
    found=[r for r in inventory['files'] if norm(r['path']).endswith(norm(suffix))]
    assert len(found)==1,(suffix,found); return found[0]
def invoke_row(inv,source):
    rows=[r for r in inv['objects'] if r['source']==source]; assert len(rows)==1,(source,len(rows)); return rows[0]
def selected_row(seal,source):
    rows=[r for r in seal['selected'] if r['source']==source]; assert len(rows)==1,(source,len(rows)); return rows[0]
def verify_link(folder,head,pins=None):
    folder=Path(folder); sp=folder/'seal.json'; rp=folder/'result.json'; ip=folder/'source-inventory.json'; vp=folder/'invocation.json'
    pp=folder/'native-game.pe'; mp=folder/'native-game.map'; xp=folder/'native-game.exe'
    seal=jread(sp); result=jread(rp); inv=jread(ip); invocation=jread(vp)
    assert seal['head']==inv['head']==head and result['exit_code']==0 and result['all_post_checks_pass'] is True
    hashes={'seal':sha(sp),'result':sha(rp),'inventory':sha(ip),'invocation':sha(vp),'pe':sha(pp),'map':sha(mp),'xex':sha(xp)}
    if pins:
        for key,value in pins.items(): assert hashes[key]==value,(key,hashes[key],value)
    assert seal['source_inventory']['sha256']==hashes['inventory']
    outputs={norm(k):v['sha256'] for k,v in result['outputs'].items()}
    assert outputs[norm(pp)]==hashes['pe'] and outputs[norm(mp)]==hashes['map'] and outputs[norm(xp)]==hashes['xex']
    image=parse_pe(pp); assert image['machine']==0x1f2 and image['base']==0x82000000
    maps=parse_map(mp); entry=map_symbol(maps,'mainCRTStartup'); assert entry['address']==image['entry']
    return {'seal':seal,'result':result,'inventory':inv,'invocation':invocation,'hashes':hashes,'image':image,'maps':maps,'entry':entry}
def verify_method(link,source,sym):
    row=invoke_row(link['invocation'],source); sealed=selected_row(link['seal'],source); obj=Path(row['object'])
    assert row['sha256']==sealed['sha256']==sha(obj) and obj.resolve()==Path(sealed['object']).resolve()
    m=map_symbol(link['maps'],sym); assert m['object'] and Path(m['object']).name.casefold()==obj.name.casefold()
    data,body,rels=collect(obj,sym); linked=link['image']['read'](m['address'],len(body)); fixed_equal(body,linked,rels)
    return {'source':source,'symbol':sym,'object':str(obj),'object_sha256':sha(obj),'va':m['address'],'linked':linked,
            'body':body,'data':data,'relocs':rels,'bytes':len(body),'map_object':m['object']}
def linked_calls(method,link,names): return verify_calls(call_rows(method['data'],method['relocs']),method['va'],method['linked'],link['maps'],names)
def assert_words(body,words,label):
    out=[]
    for off,expected in words.items():
        actual=be32(body,off); assert actual==expected,(label,hex(off),hex(actual),hex(expected))
        out.append({'offset':hex(off),'word':f'{actual:08x}'})
    return out
def case_table(method,asm_path,sym,base,case,label):
    asm=asm_function(asm_path,sym); slot=base+case*4
    assert asm['dcd'].get(slot)==label and label in asm['labels'],(sym,hex(slot),asm['dcd'].get(slot))
    local={off for off,_,kind in method['relocs'] if kind==2}; assert local==set(asm['dcd']),(sym,len(local),len(asm['dcd']))
    for off,target_label in asm['dcd'].items():
        assert target_label in asm['labels']
        assert be32(method['linked'],off)==method['va']+asm['labels'][target_label],(sym,hex(off),target_label)
    target=method['va']+asm['labels'][label]
    return {'base_offset':hex(base),'action_id':case,'slot_offset':hex(slot),'label':label,'target_va':hex(target),
            'dcd_entries':len(asm['dcd']),'all_local_targets_match':True}

def source_provenance():
    assert git('show','-s','--format=%T',HEAD)==TREE and git('show','-s','--format=%T',C9708)==TREE
    assert git('show','-s','--format=%T',B521)==TREE_B521
    for rev in (HEAD,C9708,B521): assert git('show','-s','--format=%P',rev).split()==[R155]
    blobs={}; physical={}; normalized={}
    for src,blob in ((CAR,'b4abdaf79510051a4f9d806df555ea4bf7d83e4f'),(MAIN,'40561cab18afb5fe10610abf2c78c42573b67001')):
        ids=[git('rev-parse',f'{rev}:{src}') for rev in (HEAD,C9708,B521)]; assert ids==[blob]*3,(src,ids)
        content=git_bytes(f'{HEAD}:{src}'); work=(REPO/src).read_bytes()
        assert work.replace(b'\r\n',b'\n')==content,src
        blobs[src]=blob; physical[src]=sha_bytes(work); normalized[src]=sha_bytes(content)
    assert physical[CAR]=='b21583b09b16d25b9e2eb97d85f4e92e41110a7945554314a5147d5b9c648646'
    assert physical[MAIN]=='44fbc9397efc407753aaa435f410c3a77c1d7fd250b31f3b094459ca95a48c39'
    return {'head':HEAD,'tree':TREE,'base':R155,'aliases':{B521:TREE_B521,C9708:TREE},
            'source_blobs':blobs,'source_sha256':physical,'normalized_git_sha256':normalized}
def allocator(method,link,getter_count,wrapper_offsets=None):
    calls=call_rows(method['data'],method['relocs'])
    gets=[o for o,n in calls if n==GET]; cons=[o for o,n in calls if n==CONSTRUCT]; wraps=[o for o,n in calls if n==ALLOC]
    assert len(gets)==getter_count and len(cons)==1 and len(wraps)==6
    targets=linked_calls(method,link,[GET,CONSTRUCT,ALLOC])
    if getter_count==1:
        assert cons==[0xa8] and gets==[0xac] and wraps==[0xc4,0x164,0x18c,0x1b8,0x1e4,0x214]
        assert wrapper_offsets==wraps
        words={0xb0:0x3c80009a,0xb4:0x38c00000,0xb8:0x38a00080,0xbc:0x60841300,0xc0:0x7c7f1b78,
               0x150:0x38c00000,0x154:0x38a00080,0x158:0x3c800078,0x15c:0x7fe3fb78,
               0x17c:0x38c00000,0x180:0x38a00080,0x184:0x3c800078,0x188:0x7fe3fb78,
               0x1a4:0x38c00000,0x1a8:0x38a00080,0x1ac:0x3c800004,0x1b0:0x7fe3fb78,
               0x1d0:0x38c00000,0x1d4:0x38a00080,0x1d8:0x3c800002,0x1dc:0x7fe3fb78,
               0x1fc:0x3d600001,0x200:0x7fe3fb78,0x204:0x61648000,0x208:0x38c00000,0x20c:0x38a00080,0x210:0x7c9f2378}
        allocation_words=assert_words(method['body'],words,'R156 main arguments')
        allocations=[('0xc4','0x9a1300'),('0x164','0x780000'),('0x18c','0x780000'),('0x1b8','0x40000'),('0x1e4','0x20000'),('0x214','0x18000')]
        return {'construct_calls':[hex(x) for x in cons],'getter_calls':[hex(x) for x in gets],
                'wrapper_calls':[hex(x) for x in wraps],
                'allocations':[{'call_offset':off,'bytes':size,'alignment':128,'name_pointer':0} for off,size in allocations],
                'argument_words':allocation_words,'resolved_targets':targets,'cached_allocator_register':'r31'}
    assert getter_count==6 and not wrapper_offsets
    assert cons[0]<gets[0]<wraps[0]
    return {'construct_calls':[hex(x) for x in cons],'getter_calls':[hex(x) for x in gets],
            'wrapper_calls':[hex(x) for x in wraps],'resolved_targets':targets}

def main():
    prov=source_provenance()
    bindp=INTAKE/'commit-binding.json'; bind=jread(bindp)
    assert sha(bindp)==PINS['commit_binding'] and bind['head']==HEAD and bind['base']==R155
    assert CAR in bind['paths'] and MAIN in bind['paths']
    freshp=SEL/'compile-results.json'; fresh=jread(freshp)
    assert sha(freshp)==PINS['fresh_compile'] and fresh['requested']==fresh['passed']==1004 and fresh['failed']==0
    fullp=INTAKE/'composite-active-native.json'; full=jread(fullp)
    assert sha(fullp)==PINS['full_compile'] and full['provenance']['source_head']==HEAD
    assert full['requested']==full['passed']==3710 and full['failed']==0
    assert full['provenance']['fresh']==1004 and full['provenance']['retained']==2706
    link=verify_link(LINK,HEAD,{k:PINS[k] for k in ('seal','result','inventory','pe','map','xex')})
    assert link['seal']['report']['sha256']==PINS['full_compile']
    assert link['seal']['selected_objects_sha256']=='c9e5f0df6d6c286000c00b9b34376373774526814c679dc0b4c02047f5ee99b0'
    assert 'xapi0.obj' in link['entry']['object'].casefold()
    invsrc={}
    for src,digest in prov['source_sha256'].items():
        row=source_row(link['inventory'],src); srcpath=Path(row['path'])
        assert row['sha256']==digest and sha(srcpath)==row['sha256']
        assert srcpath.read_bytes().replace(b'\r\n',b'\n')==git_bytes(f'{HEAD}:{src}')
        invsrc[src]={'path':row['path'],'sha256':row['sha256'],'matches_commit_after_crlf_normalization':True}

    freshrows={r['source']:r for r in fresh['results']}; fullrows={r['source']:r for r in full['results']}
    sources=[(CAR,EXIT),(MAIN,'main'),(DIR,DIRECTOR),(RACE,RACECAR)]
    object_pins={}
    for src,sym in sources:
        ir=invoke_row(link['invocation'],src); sr=selected_row(link['seal'],src); obj=Path(ir['object'])
        assert ir['sha256']==sr['sha256']==sha(obj) and obj.resolve()==Path(sr['object']).resolve()
        cr=fullrows[src]; assert cr['passed'] and cr['build_head']==HEAD and Path(cr['object']).resolve()==obj.resolve()
        if src in freshrows:
            fr=freshrows[src]; assert fr['passed'] and fr['machine']==0x1f2 and Path(fr['object']).resolve()==obj.resolve()
        object_pins[src]={'object':str(obj),'sha256':sha(obj),'asm':str(Path(cr['assembly'])),'asm_sha256':sha(cr['assembly'])}

    exit_m=verify_method(link,CAR,EXIT); main_m=verify_method(link,MAIN,'main')
    dir_m=verify_method(link,DIR,DIRECTOR); race_m=verify_method(link,RACE,RACECAR)
    newev=event_seq(exit_m['data'],exit_m['body'],exit_m['relocs']); assert [(i,s) for _,i,s in newev]==EVENTS_156
    add=linked_calls(exit_m,link,[ADD])[ADD]; assert add['count']==len(EVENTS_156)
    assert newev[2][0]==0x118
    payload_words=assert_words(exit_m['body'],{0x50:0x3ba00000,0xd0:0x839f0038,0xd4:0x2b1c0000,0xd8:0x409a0020,0xf8:0x39610070,0xfc:0x1000e0c3,
        0x100:0x38c00020,0x104:0x38a0004d,0x108:0x38810070,0x110:0x100059c3,0x114:0x9ba10080},'R156 action77')
    assert branch_target(exit_m['va']+0x118,be32(exit_m['linked'],0x118))==map_symbol(link['maps'],ADD)['address']
    da=case_table(dir_m,object_pins[DIR]['asm'],DIRECTOR,0x24c,77,'$LN76@ProcessInp')
    ra=case_table(race_m,object_pins[RACE]['asm'],RACECAR,0x304,77,'$LN178@HandleGame')
    doff=int(da['target_va'],16)-dir_m['va']; roff=int(ra['target_va'],16)-race_m['va']
    assert doff==0xbe8 and roff==0x968
    dw=assert_words(dir_m['linked'],{0xc08:0x89770010,0xc0c:0x2b0b0000,0xc10:0x419a000c,
        0xc14:0x7f1f79ae,0xc20:0x7f1f992e,0xc24:0x616b3981},'Director action77')
    assert cond_target(dir_m['va']+0xc10,be32(dir_m['linked'],0xc10))==dir_m['va']+0xc1c
    rw=assert_words(race_m['linked'],{0x968:0x3d600001,0x96c:0x7c9fc02e,0x970:0x7fe3fb78,0x974:0x7f7fe9ae,
        0x97c:0x7f7ff1ae,0x980:0x7f7f59ae},'RaceCar action77')
    racecalls=linked_calls(race_m,link,[GET_ACTIVE,GET_GLOBAL,GET_VEHICLE_INDEX,GET_VEHICLE_DATA,CAR_STATS,CAR_TRAINING])
    case_calls={GET_ACTIVE:0x984,GET_GLOBAL:0x9ac,GET_VEHICLE_INDEX:0x9e4,GET_VEHICLE_DATA:0xa14,
                CAR_STATS:0xa54,CAR_TRAINING:0xa60}
    case_call_evidence={}
    for name,site in case_calls.items():
        assert hex(site) in racecalls[name]['sites'],(name,racecalls[name]['sites'])
        case_call_evidence[name]={'site':hex(site),'target_va':racecalls[name]['target_va']}
    startup=allocator(main_m,link,1,[0xc4,0x164,0x18c,0x1b8,0x1e4,0x214])

    # R155 is the official pre-change PE; bind its selected objects and confirm the pre-R156 bodies.
    base=verify_link(BASE,R155)
    assert base['hashes']['xex']=='dc82daf3cb18c67831c2ba76b2e0fcbc589a16d8584c9418e7a6400d61558406'
    assert base['hashes']['pe']=='c172bc7b3d101a7ed092a99c7767c1f056d70664f5cf6ff1e9b875957d16d510'
    assert base['hashes']['map']=='62833ae651d9f8e2839cf7924ebe7f403de3684e70d77af112aea7e3c3f03b5a'
    base_exit=verify_method(base,CAR,EXIT); base_main=verify_method(base,MAIN,'main')
    oldev=event_seq(base_exit['data'],base_exit['body'],base_exit['relocs']); assert [(i,s) for _,i,s in oldev]==EVENTS_155
    base_calls=call_rows(base_main['data'],base_main['relocs'])
    oldgets=[o for o,n in base_calls if n==GET]; oldallocs=[o for o,n in base_calls if n==ALLOC]
    assert len(oldgets)==6 and len(oldallocs)==6
    oldstartup=allocator(base_main,base,6)

    proof={
      'schema':'r156-boot-links-v1',
      'candidate':{'head':HEAD,'tree':TREE,'base':R155,'source_provenance':prov,
                   'commit_binding_sha256':sha(INTAKE/'commit-binding.json'),
                   'fresh_compile':{'sha256':sha(SEL/'compile-results.json'),'passed':fresh['passed'],'requested':fresh['requested']},
                   'full_compile':{'sha256':sha(INTAKE/'composite-active-native.json'),'passed':full['passed'],
                                   'requested':full['requested'],'fresh':full['provenance']['fresh'],'retained':full['provenance']['retained']},
                   'sealed_source_rows':invsrc,'selected_objects':object_pins},
      'strict_link':{'head':link['seal']['head'],'tree':TREE,'seal_sha256':link['hashes']['seal'],
                     'result_sha256':link['hashes']['result'],'source_inventory_sha256':link['hashes']['inventory'],
                     'invocation_sha256':link['hashes']['invocation'],'selected_objects_sha256':link['seal']['selected_objects_sha256'],
                     'xex_sha256':link['hashes']['xex'],'pe_sha256':link['hashes']['pe'],'map_sha256':link['hashes']['map'],
                     'machine':hex(link['image']['machine']),'image_base':hex(link['image']['base']),
                     'entrypoint':{'va':hex(link['image']['entry']),'symbol':'mainCRTStartup','object':link['entry']['object']},
                     'exit_code':link['result']['exit_code'],'all_post_checks_pass':link['result']['all_post_checks_pass']},
      'junkyard_exit':{'r155_baseline':{'head':R155,'xex_sha256':base['hashes']['xex'],'pe_sha256':base['hashes']['pe'],
                                         'object_sha256':base_exit['object_sha256'],'function_va':hex(base_exit['va']),
                                         'add_event_id_and_size':[(i,s) for _,i,s in oldev]},
                       'r156':{'object_sha256':exit_m['object_sha256'],'function_va':hex(exit_m['va']),
                               'body_bytes':exit_m['bytes'],'add_event_id_and_size':[(i,s) for _,i,s in newev],
                               'resolved_add_event_calls':add,
                               'action77':{'call_offset':hex(newev[2][0]),'id':77,'size':32,
                                           'spawn_location_source_offset':'0x38','position_payload_offset':'0x0',
                                           'online_flag_payload_offset':'0x10','online_flag_value':False,
                                           'instruction_words':payload_words}},
                       'baseline_sequence_preserved_with_action77_inserted':True},
      'consumers':{'main_director':{'object_sha256':dir_m['object_sha256'],'function_va':hex(dir_m['va']),
                                    'case77_table':da,'case_words':dw,
                                    'behavior':'reads payload byte +0x10; zero follows offline cleanup for junkyard state and car-mod-active'},
                    'race_car':{'object_sha256':race_m['object_sha256'],'function_va':hex(race_m['va']),
                                'case77_table':ra,'case_words':rw,'resolved_calls':case_call_evidence,
                                'behavior':'loads active race-car index, clears car-select state bytes, refreshes vehicle stats and training state'}},
      'startup_allocator':{'r155_baseline':{'head':R155,'object_sha256':base_main['object_sha256'],
                                             'function_va':hex(base_main['va']),'get_runtime_allocator_calls':len(oldgets),
                                             'allocation_wrapper_calls':len(oldallocs),'call_offsets':oldstartup['wrapper_calls']},
                           'r156':{'object_sha256':main_m['object_sha256'],'function_va':hex(main_m['va']),**startup}},
      'scope':'Static final-linked PE proof; no game launch, runtime, or UI observation is claimed.'}
    return proof

if __name__=='__main__':
    result=main(); target=OUT/'R156_BOOT_LINKS.json'; payload=json.dumps(result,indent=2)+'\n'
    expected=json.loads(payload)
    if target.exists():
        assert jread(target)==expected, f'Existing receipt differs: {target}'
    else:
        target.write_text(payload,encoding='utf-8')
    print(json.dumps({'verified':True,'receipt':str(target),'sha256':sha(target)},indent=2))
