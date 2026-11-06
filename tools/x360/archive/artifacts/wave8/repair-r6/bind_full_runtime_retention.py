"""Bind selected-object witnesses to final COFF runtime sections without rerunning models."""
from collections import Counter
from datetime import datetime,timezone
import hashlib,json,struct
from pathlib import Path
HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def coff(p):
 b=Path(p).read_bytes();machine,n,_,symbols,ns,opt,_=struct.unpack_from('<HHLLLHH',b)
 assert machine==0x1f2
 st=symbols+ns*18
 def wordname(x):
  if x[:4]==bytes(4):
   a=st+struct.unpack_from('<I',x,4)[0];return b[a:b.index(0,a)].decode('ascii',errors='replace')
  return x.split(bytes(1),1)[0].decode('ascii',errors='replace')
 names={};defs=[];i=0
 while i<ns:
  at=symbols+i*18;name=wordname(b[at:at+8]);value,sec,typ,storage,aux=struct.unpack_from('<IhHBB',b,at+8)
  names[i]=name
  if sec>0 and storage==2:defs.append({'name':name,'section':sec,'value':value,'type':typ,'storage':storage})
  i+=1+aux
 sections=[]
 for i in range(n):
  at=20+opt+i*40;name=b[at:at+8].split(bytes(1),1)[0].decode('ascii',errors='replace')
  if name.startswith('.debug'):continue
  _,_,size,raw,reloc,_,nr,_,flags=struct.unpack_from('<IIIIIIHHI',b,at+8)
  rel=[]
  for j in range(nr):
   where,target,kind=struct.unpack_from('<IIH',b,reloc+j*10);rel.append((where,kind,names[target]))
  data=b[raw:raw+size] if raw else b''
  sections.append({'name':name,'size':size,'flags':flags,'data_sha256':hashlib.sha256(data).hexdigest(),'relocations':rel})
 return sections,defs
selected=read(HERE/'selected-stage1/compile-results.json');selected['results']+=read(HERE/'named-native-gates/compile-results.json')['results'];full=read(HERE/'full-stage1/compile-results.json')
assert full['state']=='complete' and len(full['results'])==3615
index={r['source']:r for r in full['results']};rows=[]
for s in selected['results']:
 if not s['passed']:continue
 t=index[s['source']]
 if not t['passed']:
  rows.append({'source':s['source'],'selected_pass_lost_in_full':True});continue
 a,_=coff(s['object']);b,_=coff(t['object'])
 aa=Counter(json.dumps(x,sort_keys=True) for x in a);bb=Counter(json.dumps(x,sort_keys=True) for x in b)
 rows.append({'source':s['source'],'selected_object':s['object'],'selected_object_sha256':sha(s['object']),'full_object':t['object'],'full_object_sha256':sha(t['object']),'runtime_sections':len(a),'exact_runtime_data_and_named_relocations':aa==bb,'only_selected_sections':[json.loads(x) for x in (aa-bb).elements()],'only_full_sections':[json.loads(x) for x in (bb-aa).elements()]})
mainrow=next(r for r in full['results'] if Path(r['source']).name=='BrnMainX360.cpp')
main=None
if mainrow['passed']:
 _,defs=coff(mainrow['object']);main={'source':mainrow['source'],'object':mainrow['object'],'object_sha256':sha(mainrow['object']),'machine':'01F2','strong_external_main':[d for d in defs if d['name']=='main']}
 assert len(main['strong_external_main'])==1
result={'head':read(HERE/'native-parallel-wave8-r6-full-stage1-patch.json')['head'],'selected_raw_sha256':sha(HERE/'selected-stage1/compile-results.json'),'named_provider_raw_sha256':sha(HERE/'named-native-gates/compile-results.json'),'full_raw_sha256':sha(HERE/'full-stage1/compile-results.json'),'rows':rows,'all_selected_runtime_sections_retained':all(r.get('exact_runtime_data_and_named_relocations',False) for r in rows),'native_main_provider':main,'scope':'All221 selected-pass plus2 named-provider actual COFFs compared to final full objects: each non-debug section name/size/flags/data hash plus every relocation offset/type/target name, independent of section order. Object timestamps/debug not asserted equal. This is a runtime-section binding of selected bounded witnesses, not a fresh behavior rerun, strict link, whole image, hardware or launch proof.','created_utc':datetime.now(timezone.utc).isoformat()}
with (HERE/'full-selected-runtime-retention.json').open('x') as f:json.dump(result,f,indent=2)
print(json.dumps({'rows':len(rows),'runtime_retained':result['all_selected_runtime_sections_retained'],'main_provider':main is not None,'sha256':sha(HERE/'full-selected-runtime-retention.json')}))
raise SystemExit(0 if result['all_selected_runtime_sections_retained'] else 1)
