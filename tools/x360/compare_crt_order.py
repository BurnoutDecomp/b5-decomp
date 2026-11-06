"""Compare two pinned R174 links without linking, executing or editing them."""
import argparse,collections,hashlib,json,re,struct,sys
from pathlib import Path
import pefile
p=argparse.ArgumentParser()
for side in ('canonical','ordered'):
 for item in ('head','invocation','invocation-sha','result','result-sha','pe','pe-sha','map','map-sha'):p.add_argument('--'+side+'-'+item,required=True)
a=p.parse_args()
def require(v,msg):
 if not v:raise ValueError(msg)
def digest(b):return hashlib.sha256(b).hexdigest()
def pinned(path,sha):
 require(re.fullmatch('[0-9a-fA-F]{64}',sha) is not None,'invalid SHA-256');b=Path(path).read_bytes();require(digest(b)==sha.lower(),'hash mismatch '+path);return b
def norm(x):return str(x).replace('\\','/').lower()
owners={'BrnStateManager':'src/GameSource/Sound/Module/LogicModule/BrnStateManager.cpp','StreamingStateManager':'src/GameSource/Sound/Streaming/BrnStreamingStateManager.cpp','PlayerVehicleStateManager':'src/GameSource/Sound/Vehicles/BrnPlayerVehicleStateManager.cpp','AIVehicleStateManager':'src/GameSource/Sound/Vehicles/BrnAIVehicleStateManager.cpp','TrafficStateManager':'src/GameSource/Sound/Vehicles/Traffic/BrnTrafficStateManager.cpp','EmitterStateManager':'src/GameSource/Sound/Module/LogicModule/BrnEmitterStateManager.cpp','GlobalStateManager':'src/GameSource/Sound/Global/BrnGlobalStateManager.cpp','CollisionStateManager':'src/GameSource/Sound/Collision/BrnCollisionStateManager.cpp','PassbyStateManager':'src/GameSource/Sound/Passby/BrnPassbyStateManager.cpp','StateManager':'src/GameShared/GameClasses/Sound/Logic/CgsStateManager.cpp'}
artist=list(owners)
def load(side):
 head=getattr(a,side+'_head');require(re.fullmatch('[0-9a-fA-F]{40}',head) is not None,'head not full')
 ip=getattr(a,side+'_invocation'); inv=json.loads(pinned(ip,getattr(a,side+'_invocation_sha')));pb=pinned(getattr(a,side+'_pe'),getattr(a,side+'_pe_sha'));mb=pinned(getattr(a,side+'_map'),getattr(a,side+'_map_sha'))
 seal=json.loads(pinned(inv['sealed'],inv['sealed_sha256']));require(seal['head'].lower()==head.lower(),side+' seal head')
 result=json.loads(pinned(getattr(a,side+'_result'),getattr(a,side+'_result_sha')));require(result['exit_code']==0 and result['all_post_checks_pass'] is True,side+' failed link result')
 for suffix in ('pe','map'):
  path=getattr(a,side+'_'+suffix);wanted=getattr(a,side+'_'+suffix+'_sha');matches=[v for k,v in result['outputs'].items() if Path(k).name.lower()==Path(path).name.lower()];require(len(matches)==1 and matches[0]['sha256'].lower()==wanted.lower(),side+' result does not bind '+suffix)
 rows=inv['objects'];by={norm(x['source']):x for x in rows};require(len(by)==len(rows),side+' duplicate source')
 sealed={norm(x['source']):x for x in seal['selected']};require(set(sealed)==set(by),side+' seal selected set mismatch')
 for key,row in by.items():require(norm(sealed[key]['object'])==norm(row['object']) and sealed[key]['sha256']==row['sha256'],side+' selected/invoked mismatch '+key)
 for source in owners.values():
  require(norm(source) in by,side+' missing owner '+source);row=by[norm(source)];require(digest(Path(row['object']).read_bytes())==row['sha256'],side+' owner object sha '+source)
 # The exact linked response file is bound by the invocation hash.
 require(len(inv['command'])>=2 and inv['command'][1].startswith('@'),'missing response argument');rsp=Path(inv['command'][1][1:]);raw=rsp.read_bytes();require(digest(raw)==inv['rsp_sha256'],side+' rsp hash')
 lines=[v.strip().strip('"') for v in raw.decode(errors='strict').splitlines()];objects=[norm(x['object']) for x in rows];positions=[]
 for obj in objects:
  hits=[i for i,s in enumerate(lines) if norm(s)==obj];require(len(hits)==1,side+' response object multiplicity '+obj);positions.append(hits[0])
 require(positions==sorted(positions),side+' response order differs from invocation')
 entries=[]
 for line in mb.decode(errors='replace').splitlines():
  m=re.match(r'\s*[0-9a-fA-F]+:[0-9a-fA-F]+\s+(\S+)\s+([0-9a-fA-F]{8})\s+(.*)',line)
  if m: entries.append((int(m[2],16),m[1],m[3].split()[-1] if m[3].split() else ''))
 byaddr=collections.defaultdict(list);byname=collections.defaultdict(list)
 for addr,name,owner in entries:byaddr[addr].append((name,owner));byname[name].append((addr,owner))
 def symbolprefix(prefix):
  found=[(addr,name,owner) for addr,name,owner in entries if name.startswith(prefix)];require(len(found)==1,side+' missing/ambiguous '+prefix);return found[0]
 def sym(name):
  found=byname[name];require(len(found)==1,side+' missing/ambiguous '+name);return found[0][0]
 pe=pefile.PE(data=pb);base=pe.OPTIONAL_HEADER.ImageBase
 def read(address,n):
  rva=address-base;matches=[s for s in pe.sections if s.VirtualAddress<=rva and rva+n<=s.VirtualAddress+s.SizeOfRawData];require(len(matches)==1,side+' invalid raw address '+hex(address));s=matches[0];off=s.PointerToRawData+rva-s.VirtualAddress;return pb[off:off+n]
 def word(address):return struct.unpack('>I',read(address,4))[0]
 def branch(pc):
  w=word(pc);require((w&0xfc000000)==0x48000000,'branch expected');d=w&0x03fffffc;d=d-(1<<26) if d&0x2000000 else d;return (d if w&2 else pc+d)&0xffffffff
 ca=sym('__xc_a');cz=sym('__xc_z');require(ca<cz and (cz-ca)%4==0 and cz-ca<0x100000,'CRT bounds')
 sequence=[]
 for slot in range(ca+4,cz,4):
  address=word(slot);symbols=sorted(set(byaddr.get(address,[])));sequence.append({'slot':hex(slot),'address':hex(address),'symbols':[{'name':n,'owner':o} for n,o in symbols]})
 selected=[]
 for label,source in owners.items():
  path=by[norm(source)]['object'];obj=norm(Path(path).name);idx=next(i for i,x in enumerate(rows) if norm(x['source'])==norm(source));selected.append({'name':label,'source':source,'object':obj,'input_index':idx})
 # Resolve each registration by initializer symbol and owner, and trace direct
 # branch edges through up to two named wrappers to the registry and leaf getter.
 reg,_,_=symbolprefix('?AddToClassTypeInfoArray@StateManager@Logic@CgsSound@@');addrs=sorted(byaddr)
 import bisect,functools
 @functools.lru_cache(maxsize=None)
 def edges(addr):
  k=bisect.bisect_right(addrs,addr);end=addrs[k] if k<len(addrs) else addr+0x200
  if end-addr>0x1000:return []
  result=[]
  for pc in range(addr,end,4):
   if word(pc)&0xfc000000==0x48000000:
    q=branch(pc)
    if q in byaddr:result.append((pc,q))
  return result
 def path(start,target,depth=2):
  seen=set();todo=[(start,[])]
  while todo:
   node,route=todo.pop(0)
   if node in seen:continue
   seen.add(node)
   for pc,q in edges(node):
    if q==target:return route+[{'at':hex(pc),'target':hex(q)}]
    if len(route)<depth:todo.append((q,route+[{'at':hex(pc),'target':hex(q)}]))
  return None
 regs=[];unresolved=[]
 for label,source in owners.items():
  getter=None if label=='StateManager' else symbolprefix('?GetStaticTypeInfo@'+label+'@')[0];owner=norm(Path(by[norm(source)]['object']).name);matches=[]
  for e in sequence:
   associated=[x for x in e['symbols'] if norm(x['owner'])==owner and x['name'].startswith('??__E')]
   if not associated:continue
   address=int(e['address'],16);pr=path(address,reg);pg=path(address,getter) if getter is not None else None
   if pr and (getter is None or pg): matches.append({'slot':e['slot'],'address':e['address'],'symbols':associated,'registry_path':pr,'getter_path':pg})
  if len(matches)!=1:unresolved.append({'manager':label,'matches':matches})
  for x in matches:regs.append((int(x['slot'],16),label,x))
 regs.sort(); order=[x[1] for x in regs]
 # Stable initializer identities require exactly one map symbol/owner at an address.
 # ICF aliases remain explicit and are not silently treated as equal.
 for e in sequence:
  s=e['symbols'];e['stable_key']=[s[0]['name'],norm(s[0]['owner'])] if len(s)==1 else None
 return {'head':head,'inv':inv,'pe_sha256':getattr(a,side+'_pe_sha'),'map_sha256':getattr(a,side+'_map_sha'),'invocation_sha256':getattr(a,side+'_invocation_sha'),'result_sha256':getattr(a,side+'_result_sha'),'rsp_sha256':digest(raw),'rows':rows,'by':by,'selected':selected,'entries':entries,'byaddr':dict(byaddr),'sequence':sequence,'crt_range':[hex(ca),hex(cz)],'registration_order':order,'registrations':[x[2]|{'name':x[1]} for x in regs],'registration_unresolved':unresolved,'artist_order_match':order==artist and not unresolved}
c=load('canonical');o=load('ordered')
require(c['head'].lower()==o['head'].lower(),'different heads: comparison must be same integrated source')
require(set(c['by'])==set(o['by']),'selected source sets differ')
for key in c['by']:
 require(c['by'][key]['sha256']==o['by'][key]['sha256'],'object hash differs '+key)
ownerkeys={norm(x) for x in owners.values()}
c_other=[norm(x['source']) for x in c['rows'] if norm(x['source']) not in ownerkeys];o_other=[norm(x['source']) for x in o['rows'] if norm(x['source']) not in ownerkeys]
other_order_preserved=c_other==o_other
# Owners outside the LCS of the target-object permutations are the minimum set moved.
a0=[norm(x['source']) for x in c['rows'] if norm(x['source']) in ownerkeys];b0=[norm(x['source']) for x in o['rows'] if norm(x['source']) in ownerkeys]
dp=[[[] for _ in range(len(b0)+1)] for _ in range(len(a0)+1)]
for i in range(1,len(a0)+1):
 for j in range(1,len(b0)+1):dp[i][j]=dp[i-1][j-1]+[a0[i-1]] if a0[i-1]==b0[j-1] else max(dp[i-1][j],dp[i][j-1],key=len)
anchors=set(dp[-1][-1]);moved=[x for x in c['selected'] if norm(x['source']) not in anchors];movednames={x['object'] for x in moved}
def mk(seq):
 d=collections.defaultdict(list)
 for i,e in enumerate(seq):
  if e['stable_key'] is not None:d[tuple(e['stable_key'])].append(i)
 return d
cm=mk(c['sequence']);om=mk(o['sequence']);allkeys=set(cm)|set(om);unique={k:(cm[k][0],om[k][0]) for k in allkeys if len(cm.get(k,[]))==len(om.get(k,[]))==1}
ambiguous=[{'key':list(k),'canonical_positions':cm.get(k,[]),'ordered_positions':om.get(k,[])} for k in sorted(allkeys) if len(cm.get(k,[]))!=1 or len(om.get(k,[]))!=1]
crossings=[]
for k,(ci,oi) in sorted(unique.items(),key=lambda x:x[1][0]):
 if k[1] not in movednames:continue
 for j,(cj,oj) in unique.items():
  if k[1]==j[1]:continue
  if (ci-cj)*(oi-oj)<0:crossings.append({'moved_initializer':list(k),'crossed_initializer':list(j),'canonical_positions':[ci,cj],'ordered_positions':[oi,oj],'crossed_target_owner':j[1] in movednames})
def symbol_owners(x):
 d=collections.defaultdict(set)
 for addr,n,owner in x['entries']:d[n].add(norm(owner))
 return {n:sorted(v) for n,v in d.items()}
cs=symbol_owners(c);os=symbol_owners(o);provider_changes=[{'symbol':n,'canonical':cs.get(n,[]),'ordered':os.get(n,[])} for n in sorted(set(cs)|set(os)) if cs.get(n,[])!=os.get(n,[])]
def aliases(x):
 return {tuple(sorted((n,norm(owner)) for n,owner in names)) for addr,names in x['byaddr'].items() if len(set(names))>1}
ca=aliases(c);oa=aliases(o)
result={'canonical':{k:c[k] for k in ('head','pe_sha256','map_sha256','invocation_sha256','result_sha256','rsp_sha256','crt_range','selected','registration_order','registrations','registration_unresolved','artist_order_match')},'ordered':{k:o[k] for k in ('head','pe_sha256','map_sha256','invocation_sha256','result_sha256','rsp_sha256','crt_range','selected','registration_order','registrations','registration_unresolved','artist_order_match')},'other_object_order_preserved':other_order_preserved,'minimum_moved_owners':moved,'all_identified_initializer_crossings':crossings,'ambiguous_or_missing_initializer_identities':ambiguous,'unmapped_or_folded_canonical':[e for e in c['sequence'] if e['stable_key'] is None],'unmapped_or_folded_ordered':[e for e in o['sequence'] if e['stable_key'] is None],'symbol_owner_changes':provider_changes,'folded_alias_groups_removed':[[list(k) for k in g] for g in sorted(ca-oa)],'folded_alias_groups_added':[[list(k) for k in g] for g in sorted(oa-ca)],'limitations':['Only uniquely mapped (symbol, owner) initializers are paired for crossings; ambiguous/unmapped/ICF cases are listed, not assumed equivalent.','Manager wrapper paths follow at most two direct-call hops and do not establish indirect targets or runtime execution.','Map owner and alias changes are static evidence; any changed provider still requires semantic review.']}
print(json.dumps(result,indent=2))
