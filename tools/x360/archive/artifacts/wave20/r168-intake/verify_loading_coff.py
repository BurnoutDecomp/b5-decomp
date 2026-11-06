import argparse,pathlib,json,hashlib,struct,io,contextlib
p=argparse.ArgumentParser();p.add_argument('--report',required=True);p.add_argument('--generated',required=True);p.add_argument('--reference',required=True);p.add_argument('--out',required=True);a=p.parse_args()
report=json.loads(pathlib.Path(a.report).read_text());row=next(r for r in report['results'] if r['source']=='src/GameSource/Game/BrnLoadingScreenTextures.cpp');assert row['passed'] and row['machine']==0x1f2
obj=pathlib.Path(row['object']);generated=pathlib.Path(a.generated).resolve();lines=pathlib.Path(row['log']).read_text(errors='replace').splitlines();traces=[]
for line in lines:
 if 'including file:' in line and '.generated.inc' in line:
  x=pathlib.Path(line.split('including file:',1)[1].strip());traces.append((x if x.is_absolute() else pathlib.Path.cwd()/x).resolve())
assert traces==[(generated/'BrnLoadingScreenTextures.generated.inc').resolve()], traces
buffer=io.StringIO()
with contextlib.redirect_stdout(buffer):
 b=obj.read_bytes(); machine,n,_,symoff,count,opt,_=struct.unpack_from('<HHIIIHH',b); assert machine==0x1f2
 sections=[struct.unpack_from('<8sIIIIIIHHI',b,20+opt+40*i) for i in range(n)]; data=next((i+1,x) for i,x in enumerate(sections) if x[0].rstrip(b'\0')==b'.data'); index,row=data; assert row[9]&0x80000000
 strings=symoff+count*18; symbols={}; i=0
 while i<count:
  raw,val,sec,typ,storage,aux=struct.unpack_from('<8sIhHBB',b,symoff+18*i);name=(b[strings+struct.unpack_from('<I',raw,4)[0]:].split(b'\0',1)[0] if raw[:4]==b'\0'*4 else raw.rstrip(b'\0')).decode('ascii','replace');symbols[name]=(val,sec,storage);i+=1+aux
 manifest=json.loads((generated/'BrnLoadingScreenTextures.manifest.json').read_text()); received={}
 for name,entry in manifest.items():
  key='?'+name+'@BrnGame@@3PAEA'; offset,sec,storage=symbols[key]; assert sec==index and storage==2 and offset%8==0
  size=entry['size']; raw=b[row[4]+offset:row[4]+offset+size];digest=hashlib.sha256(raw).hexdigest();assert digest==entry['sha256'];received[name]={'section_offset':offset,'size':size,'sha256':digest}
 print(json.dumps({'machine':hex(machine),'section':'.data writable','arrays':received,'obj_sha256':hashlib.sha256(b).hexdigest(),'include_sha256':hashlib.sha256((generated/'BrnLoadingScreenTextures.generated.inc').read_bytes()).hexdigest(),'manifest_sha256':hashlib.sha256((generated/'BrnLoadingScreenTextures.manifest.json').read_bytes()).hexdigest()},indent=2))
 
facts=json.loads(buffer.getvalue());ref=json.loads(pathlib.Path(a.reference).read_text());assert facts['arrays']==ref['arrays'] and facts['include_sha256']==ref['include_sha256'] and facts['manifest_sha256']==ref['manifest_sha256'];pathlib.Path(a.out).write_text(json.dumps(facts,indent=2)+'\n');print(json.dumps(facts,indent=2))
