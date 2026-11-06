import hashlib,json,pathlib,re,struct
out=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave18\ctor-provider-terra')
report=json.loads((out/'native41/compile-results.json').read_text()); found=[]; includes={}
for row in report['results']:
 b=pathlib.Path(row['object']).read_bytes(); machine,n,_,off,count,opt,_=struct.unpack_from('<HHIIIHH',b); st=off+count*18
 assert machine==0x1f2
 i=0
 while i<count:
  x=off+i*18; raw=b[x:x+8]
  if raw[:4]==bytes(4):
   o=struct.unpack_from('<I',raw,4)[0]; name=b[st+o:b.index(b'\0',st+o)].decode('ascii','replace')
  else: name=raw.rstrip(b'\0').decode('ascii','replace')
  val,sec,typ,cls,aux=struct.unpack_from('<IhHBB',b,x+8)
  if name.startswith(('??0FlaptManager@BrnFlapt@@','??0ViewModule@BrnGui@@')):
   flags=struct.unpack_from('<I',b,20+opt+(sec-1)*40+36)[0] if sec>0 else 0
   found.append({'source':row['source'],'symbol':name,'section':sec,'storage_class':cls,'comdat':bool(flags&0x1000)})
  i+=1+aux
 for line in pathlib.Path(row['log']).read_text(encoding='utf8',errors='replace').splitlines():
  m=re.search(r'Note: including file:\s*(.*)',line,re.I)
  if m:
   p=pathlib.Path(m.group(1).strip())
   if p.is_file(): includes[str(p)]=hashlib.sha256(p.read_bytes()).hexdigest()
(out/'coff-ctors.json').write_text(json.dumps(found,indent=2))
(out/'actual-includes-sha256.json').write_text(json.dumps(includes,indent=2))
for x in found: print(x)
print('include files',len(includes))
