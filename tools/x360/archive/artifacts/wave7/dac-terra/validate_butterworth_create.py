import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');d=out/'butterworth-build';q=d/'Butterworth.obj';errors=[];rng=random.Random(0x2cb0);selfaddr=F.M.S.SELF;aux=F.M.S.AUX;rows=[]
for name,orig in [('HighPassButterworth',0x82ba2cb0),('LowPassButterworth',0x82ba2fa0)]:
 p=d/(name+'.obj');prefix='?CreateInstance@'+name+'@';b=F.Bundle([F.Coff(p,prefix),F.Coff(q,'?CreateInstance@Butterworth@')]);b.select(prefix)
 for i in range(128):
  initial=bytearray(rng.randbytes(0x400));voice=bytearray(rng.randbytes(0x100));initial[0x21]=1+i%6;struct.pack_into('>I',initial,4,aux+0x2000);struct.pack_into('>I',initial,8,aux+0x1000);struct.pack_into('>f',initial,0x18,(i%17)/2.0);struct.pack_into('>f',voice,0x28,(i%13)/2.0);both=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.wr(selfaddr,initial);c.mem.wr(aux+0x1000,voice);c.setg(3,selfaddr);trace=[]
   provider=F.address(b,'?CreateInstance@Butterworth@') if compiled else 0x82b6c420
   def observe(pc,word):
    if pc==provider:trace.append(('provider',*[c.g(z)&0xffffffff for z in (3,4,5)]))
   c.instruction_observer=observe
   def memset(x):
    dst,val,n=[x.g(z)&0xffffffff for z in (3,4,5)];trace.append((dst,val,n));x.mem.wr(dst,bytes([val&255])*n);x.setg(3,dst)
   c.hooks[0x82926fd0]=memset
   for at,n in b.unresolved.items():
    if n=='memset':c.hooks[at]=memset
   c.run(b.entry if compiled else orig);obj=bytearray(c.mem.rd(selfaddr,0x400));obj[:4]=bytes(4);both.append((obj,c.mem.rd(aux+0x1000,0x100),c.g(3)&0xffffffff,trace))
  if both[0]!=both[1]:errors.append({'name':name,'case':i,'objdiff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:24],'voicediff':[hex(j) for j,(a,z) in enumerate(zip(both[0][1],both[1][1])) if a!=z][:24], 'returns':[both[0][2],both[1][2]],'trace':[both[0][3],both[1][3]]})
 rows.append({'name':name,'original':hex(orig),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
r={'cases':256,'errors':errors,'rows':rows,'provider_sha256':hashlib.sha256(q.read_bytes()).hexdigest(),'limits':'Actual filter and Butterworth constructors/GetSize; memset controlled, native aligned storage and 1–6 channels; derived vtable normalized; no designer/audio/hardware claim.'};(out/'butterworth-create-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:2]);assert not errors
