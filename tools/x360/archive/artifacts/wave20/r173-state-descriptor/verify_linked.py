"""Pin and inspect R173 StateManager descriptor and CRT registration order; read-only."""
import argparse,hashlib,json,re,struct,subprocess,functools
from pathlib import Path
import pefile
p=argparse.ArgumentParser()
for n in ('head','tree','repo','seal','seal-sha','invocation','invocation-sha','result','result-sha','pe','pe-sha','map','map-sha'):p.add_argument('--'+n,required=True)
a=p.parse_args()
def check(x,msg):
 if not x: raise ValueError(msg)
def sha(b):return hashlib.sha256(b).hexdigest()
def pin(path,digest):
 check(re.fullmatch(r'[0-9a-fA-F]{64}',digest) is not None,'invalid sha');b=Path(path).read_bytes();check(sha(b)==digest.lower(),'sha mismatch: '+path);return b
for s in (a.head,a.tree):check(re.fullmatch(r'[0-9a-fA-F]{40}',s) is not None,'full git hash required')
seal=json.loads(pin(a.seal,a.seal_sha));inv=json.loads(pin(a.invocation,a.invocation_sha));result=json.loads(pin(a.result,a.result_sha));data=pin(a.pe,a.pe_sha);mapdata=pin(a.map,a.map_sha)
check(seal['head'].lower()==a.head.lower(),'seal head');check(inv['sealed_sha256'].lower()==a.seal_sha.lower(),'invocation seal');check(result['exit_code']==0 and result['all_post_checks_pass'] is True,'link result')
for f,d in ((a.pe,a.pe_sha),(a.map,a.map_sha)):
 v=[v for k,v in result['outputs'].items() if Path(k).name.lower()==Path(f).name.lower()];check(len(v)==1 and v[0]['sha256'].lower()==d.lower(),'result output')
def git(*x):return subprocess.check_output(['git','-C',a.repo,*x])
check(git('rev-parse',a.head+'^{tree}').decode().strip().lower()==a.tree.lower(),'tree mismatch')
ev=json.loads(pin(str(Path(__file__).resolve().parent/'EVIDENCE.json'),'de46b284378d4f025a400664e4362eb67edc2890b911705d634a382ff4c5eb2b'))
check(ev['head']=='7aa21805e465e4a9b70de4f51f34636e8c6ab0a6','donor evidence')
for fname,hashval in ev['files_sha256'].items():pin(str(Path(__file__).resolve().parent/fname),hashval)
binding=json.loads(pin(str(Path(__file__).resolve().parent.parent/'r173-batch'/'TARGETED_OBJECT_BINDING.json'),'a95edb30aae72df0eb4118da134fdf321a8ca99c9dfa3811ff89975e989bdd5f'))
manifest=json.loads(pin(str(Path(__file__).resolve().parent.parent/'r173-batch'/'MANIFEST.json'),'9140add7f42d0e96d07f07fa44fe508df76a5af6036f0f288e718964d47cf232'))
check(binding['head']=='40c362823a27c6eb262e851266258648fcedf927' and manifest['candidate']==binding['head'],'unexpected composition')
for path in ev['source_sha256']:
 matches=[x for x in manifest['final_paths'] if x['path']==path];check(len(matches)==1,'manifest path '+path)
 expected=matches[0]['new'].split()[-1]
 check(git('rev-parse',a.head+':'+path).decode().strip()==expected,'integrated composed blob differs: '+path)
 check(git('rev-parse',binding['head']+':'+path).decode().strip()==expected,'candidate blob differs: '+path)
 rec=[x for x in seal['sources'] if x['path'].replace('\\','/').lower().endswith('/'+path.lower())];check(len(rec)==1,'sealed source missing '+path)
 check(sha(Path(rec[0]['path']).read_bytes())==rec[0]['sha256'],'sealed source file mismatch '+path)
src='src/GameShared/GameClasses/Sound/Logic/CgsStateManager.cpp'
def record(seq):
 rec=[x for x in seq if x['source'].replace('\\','/')==src];check(len(rec)==1,'selected state manager missing');return rec[0]
s=record(seal['selected']);i=record(inv['objects']);check(s['object'].lower()==i['object'].lower() and s['sha256']==i['sha256'],'object selection mismatch');check(sha(Path(s['object']).read_bytes())==s['sha256'],'selected object hash')
targeted=[x for x in binding['results'] if x['source']==src];check(len(targeted)==1,'targeted base object missing'); t=targeted[0];check(t['git_blob']=='4bce2ac00f0467500bbfaac8ea1a97ee2c5545cd','targeted source blob');pin(t['artifacts']['object']['path'],t['artifacts']['object']['sha256']); seal_src=[x for x in seal['sources'] if x['path'].replace('\\','/').lower().endswith('/'+src.lower())][0];check(seal_src['sha256']==t['source_sha256'],'targeted source hash differs from seal')
pe=pefile.PE(data=data); base=pe.OPTIONAL_HEADER.ImageBase
entries=[]
for line in mapdata.decode(errors='replace').splitlines():
 m=re.match(r'\s*[0-9a-fA-F]+:[0-9a-fA-F]+\s+(\S+)\s+([0-9a-fA-F]{8})\s+(.*)',line)
 if m:entries.append((int(m[2],16),m[1],m[3].split()[-1] if m[3].split() else ''))
names={}
for addr,name,obj in entries:names.setdefault(addr,[]).append(name)
def symbol(prefix):
 found=[x for x in entries if x[1].startswith(prefix)];check(len(found)==1,'missing/ambiguous symbol: '+prefix);return found[0]
def read(address,n):
 rva=address-base;sec=[x for x in pe.sections if x.VirtualAddress<=rva and rva+n<=x.VirtualAddress+x.SizeOfRawData];check(len(sec)==1,'address not in one raw section '+hex(address));x=sec[0];pos=x.PointerToRawData+rva-x.VirtualAddress;return data[pos:pos+n]
def word(address):return struct.unpack('>I',read(address,4))[0]
def branch(pc):
 w=word(pc);check((w&0xfc000000)==0x48000000,'not direct branch '+hex(pc));d=w&0x03fffffc;d=d-(1<<26) if d&0x2000000 else d;return (d if w&2 else pc+d)&0xffffffff
def target(ptr):
 for _ in range(2):
  if ptr in names:break
  if (word(ptr)&0xfc000003)!=0x48000000:break
  ptr=branch(ptr)
 return ptr
def addrpair(h,l):
 w=word(h);q=word(l);lo=q&0xffff;return (((w&0xffff)<<16)+(lo-65536 if lo&32768 else lo))&0xffffffff
def textat(addr):return read(addr,128).split(b'\0')[0].decode('ascii')
desc,name,obj=symbol('?sTypeInfo@StateManager@Logic@CgsSound@@');check(obj.lower()==Path(s['object']).name.lower(),'descriptor map owner');d0,d1,d2,d3=struct.unpack('>4I',read(desc,16));check(d0==0xffffffff and d2==0 and d3==0 and textat(d1)=='StateManager','descriptor fields')
for prefix,expected in [('?GetTypeInfo@StateManager@Logic@CgsSound@@',[0x3d600000,0x386b0000,0x4e800020]),('?GetTypeName@StateManager@Logic@CgsSound@@',[0x3d600000,0x396b0000,0x806b0004,0x4e800020]),('?GetStaticTypeInfo@StateManager@Logic@CgsSound@@',[0x3d600000,0x386b0000,0x4e800020])]:
 address,n,o=symbol(prefix);check(o.lower()==Path(s['object']).name.lower(),'getter owner');got=[word(address+j*4) for j in range(len(expected))]
 for j,(v,w) in enumerate(zip(got,expected)):check((v&0xffff0000)==(w&0xffff0000) if j<2 else v==w,'getter instruction '+prefix)
 check(addrpair(address,address+4)==desc,'getter descriptor '+prefix)
entry,n,o=symbol('??__EgStateManagerTypeRegistration@');check(o.lower()==Path(s['object']).name.lower(),'registrar owner');check(word(entry)&0xffff0000==0x3d600000 and word(entry+4)&0xffff0000==0x386b0000,'registrar load');check(addrpair(entry,entry+4)==desc,'registrar descriptor');reg,n,o=symbol('?AddToClassTypeInfoArray@StateManager@Logic@CgsSound@@');check((word(entry+8)&0xfc000003)==0x48000000 and target(branch(entry+8))==reg,'registrar tail branch')
ca,_,_=symbol('__xc_a');cz,_,_=symbol('__xc_z');check(ca<cz and (cz-ca)%4==0 and cz-ca<0x100000,'CRT bounds');slots=[(q,word(q)) for q in range(ca+4,cz,4)];check(sum(v==entry for _,v in slots)==1,'base CRT thunk missing or duplicated')
patterns=[('BrnStateManager','gpBrnStateManagerReg'),('StreamingStateManager','gpStreamingStateManagerReg'),('PlayerVehicleStateManager','gpPlayerVehicleStateManagerReg'),('AIVehicleStateManager','gpAIVehicleStateManagerReg'),('TrafficStateManager','gpTrafficStateManagerReg'),('EmitterStateManager','gpEmitterStateManagerReg'),('GlobalStateManager','gpGlobalStateManagerReg'),('CollisionStateManager','gpCollisionStateManagerReg'),('PassbyStateManager','gpPassbyStateManagerReg'),('StateManager','gStateManagerTypeRegistration')]
expected_ids={'BrnStateManager':-1,'StreamingStateManager':6,'PlayerVehicleStateManager':1,'AIVehicleStateManager':2,'TrafficStateManager':3,'EmitterStateManager':7,'GlobalStateManager':0,'CollisionStateManager':5,'PassbyStateManager':4,'StateManager':-1}
basegetter,_,_=symbol('?GetStaticTypeInfo@StateManager@Logic@CgsSound@@')
leaf_details={}
for label,_ in patterns[:-1]:
 getter,_,_=symbol('?GetStaticTypeInfo@'+label+'@'); expected_id=expected_ids[label]
 # This exact emitted lazy-construction sequence stores id/name/factory/base at +0/+4/+C/+8.
 check(target(branch(getter+0x28))==basegetter,'base descriptor getter edge '+label)
 check(word(getter+0x3c)==(0x39200000|(expected_id&0xffff)),'descriptor ID initializer '+label)
 for off,expected in [(0x30,0x7c6b1b78),(0x48,0x91230000),(0x4c,0x91430004),(0x54,0x9143000c),(0x58,0x91630008)]:check(word(getter+off)==expected,'descriptor initializer '+label+' '+hex(off))
 nameptr=addrpair(getter+0x34,getter+0x40);check(textat(nameptr)==label,'descriptor name '+label)
 factory=addrpair(getter+0x44,getter+0x50);check(any(n.startswith('?CreateObject@'+label+'@') for n in names.get(factory,[])),'descriptor factory '+label)
 leaf_details[label]={'getter':hex(getter),'id':expected_id,'name':hex(nameptr),'factory':hex(factory),'base_getter':hex(basegetter),'initialization':'function-local lazy'}

all_addresses=sorted({x[0] for x in entries})
@functools.lru_cache(maxsize=None)
def edges_in(addr):
 end=next((q for q in all_addresses if q>addr),addr+0x200)
 if end-addr>0x1000:return []
 result=[]
 for pc in range(addr,end,4):
  w=word(pc)
  if (w&0xfc000000)==0x48000000:
   q=branch(pc)
   if q in names: result.append((pc,q))
 return result
@functools.lru_cache(maxsize=None)
def resolve(addr,goal,max_depth=2):
 seen=set();todo=[(addr,[])]
 while todo:
  current,path=todo.pop(0)
  if current in seen:continue
  seen.add(current)
  for pc,q in edges_in(current):
   if q==goal:return path+[{'at':hex(pc),'target':hex(q)}]
   if len(path)<max_depth and q in names:todo.append((q,path+[{'at':hex(pc),'target':hex(q)}]))
 return None
items=[]
for label,pat in patterns:
 # Named registration thunks, and wrappers with a manager name that reach both the
 # registry and that manager's typed descriptor getter within a bounded call graph.
 hits=[]
 for q,v in slots:
  nms=names.get(v,[])
  if not any(n.startswith('??__E') for n in nms):continue
  if label=='StateManager' and v!=entry:continue
  path=resolve(v,reg)
  if path is None:continue
  getterpath=None if label=='StateManager' else resolve(v,int(leaf_details[label]['getter'],16))
  if label!='StateManager' and getterpath is None:continue
  hits.append({'slot':hex(q),'target':hex(v),'symbols':nms,'registration_path':path,'getter_path':getterpath})
 items.append({'name':label,'thunks':hits})
actual=sorted((int(v['slot'],16),x['name']) for x in items for v in x['thunks']); original=[x for x,_ in patterns];missing=[x['name'] for x in items if not x['thunks']];duplicate=[x['name'] for x in items if len(x['thunks'])>1];order=[name for _,name in actual]
print(json.dumps({'descriptor_verified':True,'head':a.head,'tree':a.tree,'pe_sha256':a.pe_sha,'map_sha256':a.map_sha,'seal_sha256':a.seal_sha,'invocation_sha256':a.invocation_sha,'result_sha256':a.result_sha,'descriptor':hex(desc),'descriptor_name':hex(d1),'base_registration_thunk':hex(entry),'crt_range':[hex(ca),hex(cz)],'original_order':original,'linked_order':order,'missing':missing,'duplicate':duplicate,'order_matches_original':order==original and not missing and not duplicate,'registration_entries':items,'leaf_descriptors':leaf_details,'targeted_object_sha256':t['artifacts']['object']['sha256'],'selected_object_sha256':s['sha256'],'limitation':'Order scans named CRT initializers and recognizes registry and getter paths up to two direct-call wrapper hops. Unnamed initializers, indirect calls, and runtime execution remain unproven; leaf descriptors are lazily initialized.'},indent=2))
