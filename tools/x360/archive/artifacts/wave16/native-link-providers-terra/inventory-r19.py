import json,struct,hashlib,collections,pathlib
P=pathlib.Path
out=P(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave16/native-link-providers-terra'); root=P(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp'); sdk=root/'build/x360/sdk-6534.4/portable/XDK/lib/xbox'
report=P(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave8/repair-r19-intake-20261103/composite-active-native.json'); rows=json.loads(report.read_text())['results']; sha=lambda b:hashlib.sha256(b).hexdigest()
libs=['d3d9i.lib','xapilibi.lib','xboxkrnl.lib','libcMT.lib','libcpMT.lib','xnet.lib','xgraphics.lib','xbdm.lib']
def parse(b):
 if len(b)<20:return None
 machine,nsec,ts,so,ns,opt,flags=struct.unpack_from('<HHIIIHH',b)
 if machine!=0x1f2 or not so or so+ns*18+4>len(b): return None
 st=b[so+ns*18:]; sections={}; symbols={}; selection={}
 for j in range(nsec):
  o=20+opt+j*40
  if o+40>len(b):return None
  h=struct.unpack_from('<8sIIIIIIHHI',b,o);sections[j+1]=h
 def name(raw):
  if raw[:4]==b'\0'*4:
   off=struct.unpack_from('<I',raw,4)[0]; end=st.find(b'\0',off);return st[off:end].decode('latin1') if 4<=off<len(st) and end>=0 else '<bad>'
  return raw.rstrip(b'\0').decode('latin1')
 i=0
 while i<ns:
  raw,val,sec,typ,cl,aux=struct.unpack_from('<8sIhHBB',b,so+18*i);n=name(raw); a=b[so+18*(i+1):so+18*(i+1+aux)]
  symbols[i]=(n,val,sec,typ,cl,aux,a)
  if cl==3 and sec>0 and aux and sec in sections and sections[sec][9]&0x1000:
   selection[sec]=a[14]
  i+=1+aux
 result=[]
 for idx,(n,val,sec,typ,cl,aux,a) in symbols.items():
  if cl==2:
   if sec>0: kind='comdat' if sections[sec][9]&0x1000 else 'strong';result.append((n,kind,sec,selection.get(sec),val))
   elif sec==0: result.append((n,'common' if val else 'undefined',sec,None,val))
   elif sec==-1:result.append((n,'absolute',sec,None,val))
  elif cl==105 and aux:
   target,character=struct.unpack_from('<II',a);result.append((n,'weak',0,character,symbols.get(target,('<bad-index>',))[0]))
 return result
def archive(path):
 b=path.read_bytes();assert b[:8]==b'!<arch>\n'; pos=8;long=b''
 while pos+60<=len(b):
  h=b[pos:pos+60];assert h[58:60]==b'`\n'; size=int(h[48:58]);data=b[pos+60:pos+60+size];raw=h[:16].decode('ascii').strip();pos+=60+size+(size%2)
  if raw=='//':long=data;continue
  if raw in ('/','/SYM64/'):continue
  if raw.startswith('/') and raw[1:].isdigit():
   off=int(raw[1:]);end=long.find(b'\0',off);name=long[off:end if end>=0 else len(long)].decode('latin1').rstrip('/')
  else:name=raw.rstrip('/')
  yield name,data
records=[]; failures=[]
for r in rows:
 if not r['passed']:continue
 p=P(r['object'])
 if not p.exists():failures.append(str(p));continue
 b=p.read_bytes();sy=parse(b)
 if sy is None:failures.append('parse '+str(p));continue
 records.append({'id':len(records),'kind':'object','source':r['source'],'path':str(p),'sha256':sha(b),'symbols':sy})
for lib in libs:
 p=sdk/lib
 for name,b in archive(p):
  sy=parse(b)
  if sy is not None:records.append({'id':len(records),'kind':'archive','archive':str(p),'member':name,'sha256':sha(b),'symbols':sy})
  elif len(b)>=20 and b[:4]==b'\0\0\xff\xff':
   # short import object: symbol and DLL follow the 20-byte header
   strings=b[20:].split(b'\0'); sym=strings[0].decode('latin1');dll=strings[1].decode('latin1') if len(strings)>1 else ''
   records.append({'id':len(records),'kind':'import','archive':str(p),'member':name,'sha256':sha(b),'dll':dll,'symbols':[(sym,'import',0,None,0),('__imp_'+sym,'import',0,None,0)]})
print('objects',sum(r['kind']=='object' for r in records),'archives',sum(r['kind']=='archive' for r in records),'imports',sum(r['kind']=='import' for r in records),'failures',len(failures))
(out/'coff-records-r19.json').write_text(json.dumps({'report':str(report),'report_sha256':sha(report.read_bytes()),'libraries':[{'path':str(sdk/l),'sha256':sha((sdk/l).read_bytes())} for l in libs],'failures':failures,'records':records},separators=(',',':')))


