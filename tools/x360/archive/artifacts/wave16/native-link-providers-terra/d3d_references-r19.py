import json,collections,pathlib,struct,hashlib
P=pathlib.Path;out=P(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave16/native-link-providers-terra');records=json.loads((out/'coff-records-r19.json').read_text())['records'];analysis=json.loads((out/'provider-analysis.json').read_text());objs=[r for r in records if r['kind']=='object'];d3d=[r for r in objs if r['source'].startswith('src/SDKs/D3D/')];ids={x['id'] for x in d3d};sdk=[r for r in records if r['kind']!='object'];defs={'strong','comdat','common','absolute','import'};sdkindex=collections.defaultdict(list)
for r in sdk:
 for n,k,s,c,v in r['symbols']:
  if k in defs:sdkindex[n].append([r['id'],k,c])
unique=collections.defaultdict(list)
for r in d3d:
 for n,k,s,c,v in r['symbols']:
  if k in defs and n not in sdkindex:unique[n].append({'id':r['id'],'kind':k,'section':s,'selection':c})
refs={n:{'undefined_objects':[],'relocations':[],'archive_undefined':[],'same_stem_sdk_candidates':[]} for n in unique}; overflow=[]; bad=[]
for r in objs:
 if r['id'] in ids:continue
 for n,k,s,c,v in r['symbols']:
  if n in refs and k=='undefined':refs[n]['undefined_objects'].append(r['id'])
 b=P(r['path']).read_bytes(); machine,ns,ts,so,nn,opt,flags=struct.unpack_from('<HHIIIHH',b);st=b[so+nn*18:];sym={};i=0
 while i<nn:
  raw,val,sec,typ,cl,aux=struct.unpack_from('<8sIhHBB',b,so+i*18)
  if raw[:4]==b'\0'*4:
   off=struct.unpack_from('<I',raw,4)[0];end=st.find(b'\0',off);n=st[off:end].decode('latin1') if 4<=off<len(st) and end>=0 else ''
  else:n=raw.rstrip(b'\0').decode('latin1')
  sym[i]=n;i+=1+aux
 for j in range(ns):
  h=struct.unpack_from('<8sIIIIIIHHI',b,20+opt+40*j); count=h[7]; off=h[5]
  if h[9]&0x01000000:overflow.append([r['id'],j+1]);continue
  for k in range(count):
   address,index,typ=struct.unpack_from('<IIH',b,off+10*k);n=sym.get(index)
   if n in refs:refs[n]['relocations'].append({'object':r['id'],'section':j+1,'offset':address,'type':typ,'target_symbol_index':index})
for r in sdk:
 for n,k,s,c,v in r['symbols']:
  if n in refs and k=='undefined':refs[n]['archive_undefined'].append(r['id'])
for n in refs:
 # Restrict candidate matching to same function+class prefix up to @@. String equality is not proof of ABI parity.
 stem=n.split('@@')[0]+'@@'
 for x in sdk:
  if not x.get('archive','').lower().endswith('d3d9i.lib'):continue
  for m,k,s,c,v in x['symbols']:
   if m.startswith(stem) and k in defs:refs[n]['same_stem_sdk_candidates'].append({'symbol':m,'id':x['id'],'kind':k,'selection':c})
output={'scope':'All 3613 R19 passing objects except eight SDK D3D objects; eight selected SDK archives. Two failed R19 TUs not represented. Undefined and relocations are static records, not reachability proof.','d3d_objects':[{'id':x['id'],'source':x['source'],'path':x['path'],'sha256':x['sha256']} for x in d3d],'unique_definitions':dict(unique),'references':refs,'relocation_overflow_sections':overflow,'count':len(unique)}
(out/'d3d-unique-external-references-r19.json').write_text(json.dumps(output,indent=2))
print('unique',len(unique),'outside undefined',sum(len(x['undefined_objects']) for x in refs.values()),'relocs',sum(len(x['relocations']) for x in refs.values()),'archive undefined',sum(len(x['archive_undefined']) for x in refs.values()),'overflow',len(overflow))

