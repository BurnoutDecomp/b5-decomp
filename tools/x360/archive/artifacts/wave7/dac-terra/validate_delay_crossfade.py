import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'ifilter-native-build/DelayFilter.obj';prefix='?DelayFilterCrossFadeFunc@';b=F.Bundle([F.Coff(p,prefix)]);b.select(prefix);errors=[];rng=random.Random(0x7d40);aux=F.M.S.AUX;ptrs=[aux+0x1000+j*0x1000 for j in range(6)]
for i in range(128):
 count=[0,1,2,7,16,32][i%6];images=[]
 for j in range(6):images.append(b''.join(struct.pack('>f',rng.randrange(-16,17)/16.) for _ in range(64)))
 both=[]
 for compiled in (False,True):
  c=F.cpu(b)
  for addr,data in zip(ptrs,images):c.mem.wr(addr,data)
  c.setg(3,count);c.setf(1,(i%17-8)/16.);c.setf(2,((i*3)%17-8)/16.)
  for r,addr in zip((6,7,8,9,10),ptrs):c.setg(r,addr)
  F.put(c,c.g(1)+0x54,ptrs[5]);c.run(b.entry if compiled else 0x82b67d40);both.append(tuple(c.mem.rd(addr,256) for addr in ptrs))
 if both[0]!=both[1]:errors.append({'case':i,'diff':[sum(a!=z for a,z in zip(x,y)) for x,y in zip(both[0],both[1])]})
r={'cases':128,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual crossfade kernel original vs compiled, six pointer arrays and stack output pointer, finite randomized data and counts 0..32. No alias/special-float/hardware claim.'};(out/'delay-crossfade-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:1]);assert not errors
