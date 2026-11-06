import json,re,collections,pathlib,struct,hashlib
P=pathlib.Path;root=P(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave16');base=root/'native-link-providers-terra';out=root/'material-state-provider-terra';data=json.loads((base/'coff-records-r19.json').read_text()); rec=data['records']; plan=json.loads((base/'R19_GAME_PROVIDER_PLAN.json').read_text());msgs=json.loads((root/'r19-strict-game-link-diagnostic/diagnostic-parsed.json').read_text())['duplicate_messages']; names={re.search(r'already defined in (\S+)',s).group(1).lower() for s in msgs if 'xgraphics.lib(' in s.lower()}; objs=[r for r in rec if r['kind']=='object' and r['source'].startswith('src/SDKs/XGraphics/')];ids={r['id'] for r in objs}; selected={r['object'] for r in plan['selected']}; defs={'strong','comdat','common','absolute','import'};sdk=collections.defaultdict(list); objdefs=collections.defaultdict(list)
for r in rec:
 for n,k,s,c,v in r['symbols']:
  if k in defs:(objdefs if r['kind']=='object' else sdk)[n].append(r['id'])
unique={n for r in objs for n,k,*_ in r['symbols'] if k in defs and n not in sdk};refs={n:{'undefined':[],'weak':[],'relocations':[]} for n in unique};over=[];fail=[];checked=[]
def parse(b,r):
 try:
  machine,ns,ts,so,nn,opt,flags=struct.unpack_from('<HHIIIHH',b)
  if machine!=0x1f2 or not so:return
  st=b[so+nn*18:]; sy={};i=0
  while i<nn:
   raw,val,sec,typ,cl,aux=struct.unpack_from('<8sIhHBB',b,so+i*18)
   if raw[:4]==b'\0'*4:
    off=struct.unpack_from('<I',raw,4)[0];end=st.find(b'\0',off);name=st[off:end].decode('latin1') if 4<=off<len(st) and end>=0 else ''
   else:name=raw.rstrip(b'\0').decode('latin1')
   sy[i]=name;i+=1+aux
  for j in range(ns):
   h=struct.unpack_from('<8sIIIIIIHHI',b,20+opt+j*40);count=h[7];off=h[5]
   if h[9]&0x01000000:over.append([r['id'],j+1]);continue
   for k in range(count):
    addr,idx,t=struct.unpack_from('<IIH',b,off+10*k);n=sy.get(idx)
    if n in refs:refs[n]['relocations'].append({'record':r['id'],'section':j+1,'offset':addr,'type':t})
  checked.append(r['id'])
 except Exception as e:fail.append([r['id'],str(e)])
for r in rec:
 if r['kind']=='object' and r['path'] not in selected:continue
 if r['kind'] in ('object','archive'):
  for n,k,*_ in r['symbols']:
   if n in refs and k in ('undefined','weak'):refs[n][k].append(r['id'])
 if r['kind']=='object':parse(P(r['path']).read_bytes(),r)
# Extract every archive member without mutating the archive.
lookup={(r['archive'].lower(),r['member'].lower(),r['sha256']):r for r in rec if r['kind']=='archive'}
for lib in data['libraries']:
 path=P(lib['path']);b=path.read_bytes();assert hashlib.sha256(b).hexdigest()==lib['sha256'];pos=8;long=b''
 while pos+60<=len(b):
  h=b[pos:pos+60];size=int(h[48:58]);v=b[pos+60:pos+60+size];raw=h[:16].decode('ascii').strip();pos+=60+size+(size%2)
  if raw=='//':long=v;continue
  if raw in ('/','/SYM64/'):continue
  if raw.startswith('/') and raw[1:].isdigit():
   off=int(raw[1:]);end=long.find(b'\0',off);n=long[off:end if end>=0 else len(long)].decode('latin1').rstrip('/')
  else:n=raw.rstrip('/')
  r=lookup.get((str(path).lower(),n.lower(),hashlib.sha256(v).hexdigest()))
  if r:parse(v,r)
assert not fail
rows=[]
for r in objs:
 own=[n for n,k,*_ in r['symbols'] if k in defs];us=sorted(set(own)&unique);external={n:{k:[x for x in v if (x['record'] if k=='relocations' else x)!=r['id']] for k,v in refs[n].items()} for n in us};external={n:v for n,v in external.items() if any(v.values())};rows.append({'id':r['id'],'source':r['source'],'object':r['path'],'sha256':r['sha256'],'unique_names_no_sdk_exact':us,'external_references':external,'sdk_exact_definitions':{n:sdk[n] for n in own if n in sdk}})
result={'scope':'Complete XGRAPHICS reconstruction cohort; all 3596 selected game objects and all parseable PPC archive members in eight indexed SDK archives. Relocations include references to definitions as well as undefined names; not a reachability proof.','selected_object_count':len(selected),'affected_objects':rows,'unique_names_count':len(unique),'refs':refs,'record_lookup':{str(r['id']):{k:r[k] for k in ('kind','source','path','archive','member','sha256') if k in r} for r in rec if r['id'] in set(checked)},'checked_count':len(checked),'unchecked_overflow_sections':over,'parse_failures':fail,'caveat':'SDK exact decorated name is signature evidence, not body equivalence; no library extraction/relink or full binary parity proven.'}
path=out/'xgraphics-cohort-reference-audit.json';path.write_text(json.dumps(result,indent=2)+'\n');print('written',path,'sha256',hashlib.sha256(path.read_bytes()).hexdigest(),'checked',len(checked),'overflow',len(over),'unique',len(unique));
for x in rows:print(x['source'].split('/')[-1],len(x['unique_names_no_sdk_exact']),'outside',len(x['external_references']))
