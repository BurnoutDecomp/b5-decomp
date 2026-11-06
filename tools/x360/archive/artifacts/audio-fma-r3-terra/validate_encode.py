import sys,struct,json,random,hashlib,math
from pathlib import Path
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/audio-fma-r3-terra');sys.path.insert(0,str(out));from probe_original import WordCPU,F
root=Path('D:/Reverse/BP-X360-Experimental-Artifacts/audio-fma-r3-terra/native');rng=random.Random(0x360fade);SELF=F.M.S.SELF;AUX=F.M.S.AUX;INPUT=AUX+0x1000;OUTPUT=AUX+0x9000;errors=[];groups=[]
for kind,original in [('XasEnc',0x82ba6e98),('Xas1Enc',0x82ba74a8)]:
 p=root/(kind+'.obj');b=F.Bundle([F.Coff(p,'?EncodeBlock@'+kind+'@')]);b.select('?EncodeBlock@'+kind+'@');n=128
 for i in range(n):
  channels=1+(i%2);samples=128*channels;pattern=i%8
  if pattern==0: values=[0.0]*samples
  elif pattern==1:values=[0.1]*samples
  elif pattern==2:values=[((-1)**j)*0.99 for j in range(samples)]
  elif pattern==3:values=[(j%31-15)/17.0 for j in range(samples)]
  else:values=[rng.uniform(-0.98,0.98) for _ in range(samples)]
  raw=b''.join(struct.pack('>f',v)for v in values);output=bytes(rng.getrandbits(8) for _ in range(512));both=[]
  for compiled in (False,True):
   c=F.cpu(b);c.__class__=WordCPU;c.max_steps=3000000;c.mem.wr(INPUT,raw);c.mem.wr(OUTPUT,output);c.setg(3,SELF);c.setg(4,INPUT);c.setg(5,OUTPUT);c.setg(6,channels)
   for at,name in b.unresolved.items():
    if name.startswith(('__savefpr_','__restfpr_')):c.hooks[at]=lambda cpu:None
   try:c.run(b.entry if compiled else original)
   except Exception as e:raise RuntimeError((kind,i,compiled,hex(getattr(c,'last_pc',0)),str(e))) from e
   both.append((c.mem.rd(OUTPUT,512),c.mem.rd(INPUT,len(raw))))
  if both[0]!=both[1]:errors.append({'kind':kind,'case':i,'channels':channels,'offsets':[j for j,(a,z) in enumerate(zip(both[0][0],both[1][0]))if a!=z][:30]})
 groups.append({'name':kind,'cases':n,'object_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'original':hex(original)})
r={'groups':groups,'errors':errors,'limits':'Actual whole original/compiled EncodeBlock for finite mono/stereo inputs (silence, constant, alternating, ramps and random); checks output and guards, input preservation. Private emulator implements sraw and shared FPR service hooks; no hardware, exceptional float or stream/end-to-end claim.'};(out/'xas-encode-comparison.json').write_text(json.dumps(r,indent=2),encoding='utf-8');print('groups',groups,'errors',errors[:5]);assert not errors
