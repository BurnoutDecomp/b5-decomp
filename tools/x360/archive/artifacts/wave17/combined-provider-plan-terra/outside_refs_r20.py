import json,pathlib,collections,struct
p=pathlib.Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave17/combined-provider-plan-terra');d=json.loads((p/'coff-records-r20-12libs.json').read_text());a=json.loads((p/'R20_COMBINED_PROVIDER_PROVISIONAL.json').read_text());obs=[x for x in d['records'] if x['kind']=='object'];exclude={x['source']:x['group'] for x in a['excluded']};names=collections.defaultdict(set)
for x in obs:
 if x['source'] in exclude:
  for n,k,*_ in x['symbols']:
   if k in ('strong','comdat','common','absolute'):names[n].add(exclude[x['source']])
refs=collections.defaultdict(list);count=0
for r in obs:
 own=exclude.get(r['source'])
 for n,k,s,c,v in r['symbols']:
  if n in names and k=='undefined':
   for group in names[n]-{own}:refs[group].append({'symbol':n,'source':r['source'],'object_id':r['id'],'caller_group':own,'kind':'undefined'})
 b=pathlib.Path(r['path']).read_bytes();_,ns,_,so,nn,opt,_=struct.unpack_from('<HHIIIHH',b);st=b[so+nn*18:];sym={};i=0
 while i<nn:
  raw,val,sec,typ,cl,aux=struct.unpack_from('<8sIhHBB',b,so+i*18);off=struct.unpack_from('<I',raw,4)[0] if raw[:4]==bytes(4) else 0;end=st.find(b'\0',off) if off else -1;n=st[off:end].decode('latin1') if off and end>=0 else raw.rstrip(bytes(1)).decode('latin1');sym[i]=(n,sec,cl);i+=1+aux
 for j in range(ns):
  h=struct.unpack_from('<8sIIIIIIHHI',b,20+opt+40*j);assert not h[9]&0x01000000
  for z in range(h[7]):
   addr,idx,typ=struct.unpack_from('<IIH',b,h[5]+10*z);v=sym.get(idx)
   if v and v[0] in names:
    for group in names[v[0]]-{own}:refs[group].append({'symbol':v[0],'source':r['source'],'object_id':r['id'],'caller_group':own,'kind':'relocation','section':j+1,'offset':addr,'type':typ,'target_section':v[1],'storage_class':v[2]})
summary={g:{'records':len(v),'undefined_names':len({x['symbol'] for x in v if x['kind']=='undefined'}),'undefined_names_from_retained':len({x['symbol'] for x in v if x['kind']=='undefined' and x['caller_group'] is None})} for g,v in refs.items()};(p/'R20_COHORT_FULL_OUTSIDE_REFERENCES.json').write_text(json.dumps({'scope':'All 3615 passing R20 objects; group-wise outside references include callers in other excluded groups. target_section>0 means locally defined in caller; undefined does not establish final live reference.','summary':summary,'references':refs},indent=2));print(json.dumps(summary,indent=2))
