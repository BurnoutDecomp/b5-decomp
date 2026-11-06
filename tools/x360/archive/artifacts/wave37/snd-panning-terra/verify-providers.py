import json,pathlib,hashlib,runpy,struct,concurrent.futures
root=pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave37/snd-panning-terra');W=pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave20'); tools=runpy.run_path(str(W/'r36-link/prepare_seal.py'));parse=tools['parse'];sha=lambda p:hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest();read=lambda p:json.loads(pathlib.Path(p).read_text()); gate=read(root/'native-composed/compile-results.json');assert gate['requested']==gate['passed']==4 and gate['failed']==0;own=[x for x in gate['results'] if pathlib.PurePosixPath(x['source']).name in ('snd_panning_x360.cpp','snd_startup_x360.cpp')]; need=set();defs=set();
for row in own:
 for n,k,*z in parse(pathlib.Path(row['object']).read_bytes()):
  if k=='undefined':need.add(n)
  if k in ('strong','comdat'):defs.add(n)
need-=defs; refs={n:[] for n in need};seal=read(W/'r38-link/seal.json')
def check(row):
 p=pathlib.Path(row['object']);b=p.read_bytes();hits=[n for n,k,*_ in parse(b) if n in need and k in ('strong','comdat')];
 if hits:assert sha(p)==row['sha256'];return (row['source'],hits)
with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
 for x in pool.map(check,seal['selected']):
  if x:
   for n in x[1]:refs[n].append('R38:'+x[0])
for pin in seal['libraries']:
 b=pathlib.Path(pin['path']).read_bytes();assert hashlib.sha256(b).hexdigest()==pin['sha256']
 for n in need:
  if n.encode()+b'\0' in b:refs[n].append('R38-library:'+pathlib.Path(pin['path']).name)
sol=pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r37-snd9-filters-sol');receipts=[read(sol/'STAGE_3_COMBINED_RECEIPT.json'),read(sol/'STAGE_4_VOICE_FREE_RECEIPT.json')]
for rc,name in [(receipts[0],'src/SDKs/EATech/include/snd/sndmix.cpp'),(receipts[1],'src/SDKs/EATech/include/snd/sndsys.cpp'),(receipts[1],'src/SDKs/EATech/include/snd/sndvoice.cpp')]:
 pin=rc['products'][name]['object'];assert sha(pin['path'])==pin['sha256'];
 for n,k,*_ in parse(pathlib.Path(pin['path']).read_bytes()):
  if n in refs and k in ('strong','comdat'):refs[n].append('Sol-private:'+name)
result={'base':seal['head'],'baseline_seal_sha256':sha(W/'r38-link/seal.json'),'own_undefined':sorted(need),'providers':refs,'missing':sorted(n for n,v in refs.items() if not v),'partner_receipts':{p.name:sha(p) for p in (sol/'STAGE_3_COMBINED_RECEIPT.json',sol/'STAGE_4_VOICE_FREE_RECEIPT.json')},'limits':'Provider existence only; partner is not integrated or linked. Transitive dependencies are not cleared.'};(root/'PROVIDER_BOUNDARY.json').write_text(json.dumps(result,indent=2)+'\n');print('refs',len(need),'missing',result['missing']);print('\n'.join(n+': '+','.join(refs[n]) for n in sorted(need) if 'SND' in n or 'gOptions' in n or 'Initialized' in n))
