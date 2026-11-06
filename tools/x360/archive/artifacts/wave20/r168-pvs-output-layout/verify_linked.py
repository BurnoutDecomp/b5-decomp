"""R168 PVS strict linked-image verifier. Inputs are mandatory pinned artifacts; no build or edits."""
import argparse, bisect, hashlib, json, re, struct, subprocess, sys
from pathlib import Path
import pefile

p=argparse.ArgumentParser()
for n in ('head','tree','repo','seal','seal-sha','invocation','invocation-sha','result','result-sha','pe','pe-sha','map','map-sha'):
    p.add_argument('--'+n, required=True)
a=p.parse_args()
for n in ('head','tree'):
    if not re.fullmatch(r'[0-9a-fA-F]{40}',getattr(a,n)): p.error('--'+n+' must be a full SHA-1')
for n in ('seal_sha','invocation_sha','result_sha','pe_sha','map_sha'):
    if not re.fullmatch(r'[0-9a-fA-F]{64}',getattr(a,n)): p.error('--'+n.replace('_','-')+' must be SHA-256')
def require(ok,msg):
    if not ok: raise ValueError(msg)
def sha(data): return hashlib.sha256(data).hexdigest()
def pinned(path,digest):
    b=Path(path).read_bytes(); require(sha(b)==digest.lower(),'hash mismatch: '+path); return b
seal=json.loads(pinned(a.seal,a.seal_sha)); inv=json.loads(pinned(a.invocation,a.invocation_sha)); result=json.loads(pinned(a.result,a.result_sha)); pebytes=pinned(a.pe,a.pe_sha); mapbytes=pinned(a.map,a.map_sha)
require(seal['head'].lower()==a.head.lower(),'seal HEAD differs')
require(inv['sealed_sha256'].lower()==a.seal_sha.lower(),'invocation does not bind seal')
require(result['exit_code']==0 and result['all_post_checks_pass'] is True,'link result not clean')
for path,digest in ((a.pe,a.pe_sha),(a.map,a.map_sha)):
    matches=[v for k,v in result['outputs'].items() if Path(k).name.lower()==Path(path).name.lower()]
    require(len(matches)==1 and matches[0]['sha256'].lower()==digest.lower(),'result does not bind '+path)
repo=Path(a.repo)
def git(*x): return subprocess.check_output(['git','-C',str(repo),*x])
require(git('rev-parse',a.head+'^{tree}').decode().strip().lower()==a.tree.lower(),'head/tree mismatch')
focused=Path(__file__).resolve().parent/'EVIDENCE.json'; ev=json.loads(pinned(str(focused),'e88a0feac7e5adf1c4988ba1531626cb377d39282a10e9d395c82f6c92a5f078')); require(ev['head']=='cec2a7aa412f49fdd039a35fdf3788293a6d6adf' and ev['compile_success'],'unexpected donor evidence')
comp=json.loads(pinned(str(focused.parent/'compile-results.json'),'908c4742a53c607caa07b37e7680e4af7338ad32c8d2accf8e4307cc7353a58d')); require(len(comp)==7 and all(x['exit_code']==0 for x in comp),'focused compile did not pass')
for x in comp:
    obj=Path(x['command'][-3][3:]) if False else next(Path(v[3:]) for v in x['command'] if v.startswith('/Fo'))
    require(sha(obj.read_bytes())==x['object_sha256'] and obj.read_bytes()[:2]==b'\xf2\x01','focused object mismatch: '+x['source'])
for path,expected in ev['source_sha256'].items():
    require(git('show',a.head+':'+path)==git('show',ev['head']+':'+path),'integrated source blob differs from donor: '+path)
    hits=[x for x in seal['sources'] if x['path'].replace('\\','/').lower().endswith('/'+path.lower())]
    require(len(hits)==1 and hits[0]['sha256']==expected,'seal source mismatch: '+path)
selected={x['source'].replace('\\','/'):x for x in seal['selected']}; invoked={x['source'].replace('\\','/'):x for x in inv['objects']}
for path in sorted(set(x['source'] for x in comp) | {x for x in ev['source_sha256'] if x.endswith('.cpp')}):
    require(path in selected and path in invoked,'missing selected object: '+path)
    s=selected[path]; v=invoked[path]; require(s['sha256']==v['sha256'] and s['object'].lower()==v['object'].lower(),'selected/invoked mismatch: '+path)
    require(sha(Path(s['object']).read_bytes())==s['sha256'],'selected object hash mismatch: '+path)
pe=pefile.PE(data=pebytes); base=pe.OPTIONAL_HEADER.ImageBase
entries=[]
for line in mapbytes.decode(errors='replace').splitlines():
    m=re.match(r'\s*[0-9a-fA-F]+:[0-9a-fA-F]+\s+(\S+)\s+([0-9a-fA-F]{8})\s+(.*)',line)
    if m: entries.append((int(m[2],16),m[1],m[3].split()[-1] if m[3].split() else ''))
names={}
for addr,name,obj in entries: names.setdefault(addr,[]).append(name)
def symbol(prefix):
    found=[(x,n,o) for x,n,o in entries if n.startswith(prefix)]
    require(len(found)==1,'missing or ambiguous symbol '+prefix); return found[0]
def words(addr,n): return list(struct.unpack('>'+str(n)+'I',pe.get_data(addr-base,n*4)))
def target(pc,w):
    require((w & 0xfc000000)==0x48000000,'not direct branch at '+hex(pc))
    disp=w&0x03fffffc; disp=disp-(1<<26) if disp&(1<<25) else disp
    return (disp if w&2 else pc+disp)&0xffffffff
def follow(t):
    # XDK linker may use a direct unconditional branch thunk.
    for _ in range(2):
        if t in names: break
        w=words(t,1)[0]
        if (w&0xfc000003)!=0x48000000: break
        t=target(t,w)
    return t
def dest(pc,w,needle):
    t=follow(target(pc,w)); ns=names.get(t,[])
    require(any(needle in x for x in ns),'branch '+hex(pc)+' target '+hex(t)+' expected '+needle+' got '+str(ns))
    return {'at':hex(pc),'target':hex(t),'symbols':ns}
def owner(name,source):
    addr,n,obj=symbol(name); require(obj.lower()==Path(selected[source]['object']).name.lower(),'map owner mismatch '+name+' '+obj); return addr
P='src/GameSource/World/EntityModules/WorldEntityModule/PVSModule/'; io=P+'BrnPVSModuleIO.cpp'; mod=P+'BrnPVSModule.cpp'; world='src/GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModule.cpp'; bridge='src/GameSource/World/EntityModules/WorldEntityModule/Bridges/WorldEntityBridgePVSToOutput.cpp'
raw={'construct':'7d8802a64891a8919421ff907c7f1b784bff6b493ba000003bdf17187fc3f37893bf00084bf25a3d7fc3f3784bf137957fc3f37893bf00084bf13789382100704891a8a4','prepare':'7d8802a69181fff89421ffa07c6b1b7839400000386b1718914b00084bf1375d38600001382100608181fff87d8803a64e800020','release':'7d8802a69181fff89421ffa07c6b1b7839400000386b1718914b00084bf1372538600001382100608181fff87d8803a64e800020','destruct':'7c6b1b7839400000386b1718914b00084bf136f8'}
orig={k:list(struct.unpack('>'+str(len(bytes.fromhex(v))//4)+'I',bytes.fromhex(v))) for k,v in raw.items()}
def fixed(label,addr,expected,edges):
    actual=words(addr,len(expected)); calls=[]
    for i,(w,ref) in enumerate(zip(actual,expected)):
        if i in edges:
            require((w&0xfc000003)==(ref&0xfc000003),'branch opcode mismatch '+label+' '+hex(i*4)); calls.append(dest(addr+4*i,w,edges[i]))
        else: require(w==ref,'fixed instruction mismatch '+label+' '+hex(i*4)+': '+hex(w)+' != '+hex(ref))
    return {'address':hex(addr),'words':len(actual),'branches':calls}
clear='?Clear@?$VariableEventQueue@$0CAA@$0BA@@CgsModule@@'; ctor='?Construct@?$VariableEventQueue@$0CAA@$0BA@@CgsModule@@'; queue='?Construct@?$EventQueue@UGetZoneResponse@PVSIO@BrnWorld@@'
checks={}
for kind in ('prepare','release','destruct'):
    prefix='?'+kind.capitalize()+'@OutputBuffer@PVSIO@BrnWorld@@'
    address=owner(prefix,io); branch_index=4 if kind=='destruct' else 7
    checks[kind]=fixed(kind,address,orig[kind],{branch_index:clear})
ctor_matches=[x for x in entries if x[1].startswith('?Construct@OutputBuffer@PVSIO@BrnWorld@@')]
if ctor_matches:
    addr=owner('?Construct@OutputBuffer@PVSIO@BrnWorld@@',mod)
    checks['construct']=fixed('construct',addr,orig['construct'],{1:'__savegprlr_29',4:queue,9:ctor,11:clear,14:clear,16:'__restgprlr_29'})
else:
    addr=owner('?CreateOutputDataStructure@?$ModuleSingleBufferedTemplate@UInputBuffer@PVSIO@BrnWorld@@',mod)
    code=words(addr,32); require(0x3be30440 in code,'CreateOutput missing module +0x440')
    pattern=orig['construct'][4:15]; branch_offsets={0:queue,5:ctor,7:clear,10:clear}
    hits=[]
    for i in range(len(code)-len(pattern)+1):
        if all(((w&0xfc000003)==(r&0xfc000003)) if j in branch_offsets else w==r for j,(w,r) in enumerate(zip(code[i:i+len(pattern)],pattern))): hits.append(i)
    require(len(hits)==1,'inlined constructor sequence missing or ambiguous')
    checks['construct']=fixed('inlined construct',addr+hits[0]*4,pattern,branch_offsets)
    checks['construct']['containing_create_output']=hex(addr)
# Bounded symbol windows; functions have unique adjacent mapped start addresses.
addrs=sorted({x for x,n,o in entries})
def window(addr):
    idx=bisect.bisect_right(addrs,addr); end=addrs[idx] if idx<len(addrs) else addr+0x1000
    require(0<end-addr<=0x10000,'bad map extent '+hex(addr)); return words(addr,(end-addr)//4)
def calls_to(addr,expected):
    out=[]
    for i,w in enumerate(window(addr)):
        if w&0xfc000003==0x48000001:
            t=follow(target(addr+4*i,w))
            if t==expected: out.append(hex(addr+4*i))
    return out
def hasimm(addr,imm,op):
    return [hex(addr+4*i) for i,w in enumerate(window(addr)) if (w>>26)==op and (w&0xffff)==imm]
getter=owner('?GetGameDataRequestInt@PVSModule@BrnWorld@@',mod); outgetter=owner('?GetOutputInterface@PVSModule@BrnWorld@@',mod); prep=owner('?Prepare@PVSModule@BrnWorld@@',mod); update=owner('?Update@PVSModule@BrnWorld@@',mod); total=owner('?GetTotalZones@WorldEntityModule@BrnWorld@@',world); stream=owner('?UpdateStream@WorldEntityModule@BrnWorld@@',world); b=owner('?BridgePVSToOutput_Prepare@WorldEntityModule@BrnWorld@@',bridge)
continuity={'getter_request_addi':hasimm(getter,0x1718,14),'prepare_request_addi':hasimm(prep,0x1718,14),'update_request_addi':hasimm(update,0x1718,14),'prepare_total_store':hasimm(prep,0x1710,36),'update_total_store':hasimm(update,0x1710,36),'total_load':hasimm(total,0x1710,32),'total_to_output_getter':calls_to(total,outgetter),'stream_to_output_getter':calls_to(stream,outgetter),'bridge_to_request_getter':calls_to(b,getter)}
for k,v in continuity.items(): require(v,'missing linked continuity witness '+k)
print(json.dumps({'status':'pass','head':a.head,'tree':a.tree,'seal_sha256':a.seal_sha,'invocation_sha256':a.invocation_sha,'result_sha256':a.result_sha,'pe_sha256':a.pe_sha,'map_sha256':a.map_sha,'donor_head':ev['head'],'lifecycle':checks,'continuity':continuity,'caveats':['Getters retain inherited assertion file/line drift.','Static linked evidence is not launch, runtime, or replay/gameplay proof.']},indent=2))
