import json,struct,pathlib,collections
base=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave16\native-link-providers-terra')
D=json.load(open(base/'coff-records-r19.json'))['records'];o=next(x for x in D if x.get('source','').endswith('/exception.cpp'))
keys={s[0] for s in o['symbols'] if s[1] in ('strong','comdat','common','absolute','weak')}
refs={n:[] for n in keys}; overflow=[]; errors=[]
for r in D:
 if r['kind']!='object' or r['id']==o['id']:continue
 try:
  b=pathlib.Path(r['path']).read_bytes(); machine,ns,ts,so,nn,opt,flags=struct.unpack_from('<HHIIIHH',b); st=b[so+nn*18:];sym={};i=0
  while i<nn:
   raw,val,sec,typ,cl,aux=struct.unpack_from('<8sIhHBB',b,so+i*18)
   if raw[:4]==b'\0'*4:
    off=struct.unpack_from('<I',raw,4)[0];end=st.find(b'\0',off);n=st[off:end].decode('latin1') if 4<=off<len(st) and end>=0 else ''
   else:n=raw.rstrip(b'\0').decode('latin1')
   sym[i]=n;i+=1+aux
  for j in range(ns):
   h=struct.unpack_from('<8sIIIIIIHHI',b,20+opt+40*j); count=h[7]; off=h[5]
   if h[9]&0x01000000: overflow.append([r['source'],j+1]);continue
   for k in range(count):
    address,index,typ=struct.unpack_from('<IIH',b,off+10*k); n=sym.get(index)
    if n in refs:refs[n].append({'source':r['source'],'section':j+1,'offset':address,'type':typ})
 except Exception as e:errors.append([r['source'],str(e)])
result={'scope':'All R19 passing objects except exception.cpp, all relocation sections without overflow; not runtime reachability','references':refs,'overflow':overflow,'errors':errors}
pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave16\native-entry-terra\crt-audit\relocations.json').write_text(json.dumps(result,indent=2))
print({n:len(x) for n,x in refs.items()},'overflow',len(overflow),'errors',len(errors))
