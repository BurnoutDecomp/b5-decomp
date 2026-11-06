from pathlib import Path
import sys,json,random,hashlib,struct
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
p=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/bodypart-limit');o=F.Coff(p/'limit_lazy.obj','?LimitVelocities@PhysicalBodyPart@');b=F.Bundle([o]);b.select('?LimitVelocities@PhysicalBodyPart@');base=F.address(b,'sAngularSlope');A=0x40000000;rng=random.Random(825)
def run(comp,initial,part,t):
 c=F.cpu(b);c.mem.add(A,0x1000);c.mem.add(0x82FB9000,0x1000);c.mem.wr(A,part);ga=base if comp else 0x82FB9FB4;c.mem.wr(ga,initial);calls=[]
 for n,a in [('BeginAssert',0x82817548),('FireAssert',0x82820810),('EndAssert',0x82817558)]:
  fn=lambda cpu,n=n:calls.append(n);c.hooks[a]=fn
  for adr,sym in b.unresolved.items():
   if n in sym:c.hooks[adr]=fn
 c.setg(3,A);q=struct.unpack('>I',struct.pack('>f',t))[0];c.vr[1]=(q,q,q,q);c.run(b.entry if comp else 0x825BA5C0);return c.g(3)&0xffffffff,c.mem.rd(A,0x200).hex(),c.mem.rd(ga,12).hex(),calls
bad=[]
for i in range(128):
 part=bytearray(rng.randbytes(0x200));part[0x1e4]=0;part[0x40:0x60]=struct.pack('>8f',*[rng.uniform(-120,120) for _ in range(8)]);initial=struct.pack('>ffI',rng.uniform(-6000,-1000),rng.uniform(-6000,-1000),i%4);t=rng.uniform(0,.1);x=run(False,initial,part,t);y=run(True,initial,part,t)
 if x!=y:bad.append((i,x,y))
print('mismatch',len(bad),bad[:1])
(p/'lazy-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'scope':'bounded method actual source candidate, controlled assertions, finite velocities/time and poisoned lazy state; no whole TU, nonfinite or hardware claim','object_sha256':hashlib.sha256((p/'limit_lazy.obj').read_bytes()).hexdigest()},indent=2))

