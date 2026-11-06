from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/mask4-consumer-v1');obj=out/'1.obj';n='?TestSphereTriangle4SOA@CgsGeometric@@';b=F.Bundle([F.Coff(obj,n)]);b.select(n);A=0x40000000;rng=random.Random(0x8283FD50);bad=[]
def run(comp,sphere,vs):
 c=F.cpu(b);c.mem.add(A,0x1000);c.mem.wr(A,struct.pack('>4f',*sphere));c.setg(3,A)
 for i,v in enumerate(vs):c.vr[1+i]=struct.unpack('>4I',struct.pack('>4f',*v))
 for addr,name in b.unresolved.items():
  if name.startswith('__save') or name.startswith('__rest'):c.hooks[addr]=lambda cpu:None
 c.run(b.entry if comp else 0x8283FD50);return list(c.vr[1])
for i in range(128):
 sx=float(rng.randrange(-8,9));sy=float(rng.randrange(-8,9));sz=float(rng.randrange(-5,6));r=float(rng.randrange(1,4));shift=[float(rng.randrange(-3,4)) for k in range(4)];vs=[shift,[0.]*4,[0.]*4,[x+4 for x in shift],[0.]*4,[0.]*4,shift,[4.]*4,[0.]*4];x=run(False,[sx,sy,sz,r],vs);y=run(True,[sx,sy,sz,r],vs)
 if x!=y:bad.append({'case':i,'original':x,'compiled':y})
r={'cases':128,'mismatches':bad,'scope':'original raw-vector TestSphereTriangle4SOA8283FD50 vs actual compiled provider, sphere r3, nine raw vector args v1-v9 and v1 result; finite integer simple right triangles with four translated lanes, VMX emulator estimate model; no arbitrary triangle/nonfinite/full consumer claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(out/'sphere-soa-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases',128,'mismatches',len(bad),bad[:1]);assert not bad
