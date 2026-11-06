from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformation-header-v1');obj=out/'probe.obj';n='?ProbeEvent@@';b=F.Bundle([F.Coff(obj,n)]);b.select(n);A=0x40000000;rng=random.Random(0x8260DCF0);bad=[]
class Stop(Exception):pass
def run(comp,rows,off0,off1):
 c=F.cpu(b);c.mem.add(A,0x3000);c.mem.wr(A,b''.join(struct.pack('>4f',*v) for v in rows));c.mem.wr(A+0x170,struct.pack('>4f',*off0));c.mem.wr(A+0x180,struct.pack('>4f',*off1));c.mem.wr(A+0x1dc,(A+0x600).to_bytes(4,'big'));c.mem.wr(A+0x608,(A+0x800).to_bytes(4,'big'));c.mem.wr(A+0x9c8,(123).to_bytes(4,'big'));c.mem.wr(A+0x400,b'\xA5'*80)
 if comp:c.setg(3,A);c.setg(4,A+0x400);c.run(b.entry);return c.mem.rd(A+0x400,64).hex()
 c.setg(31,A);c.setg(16,0x170);c.setg(15,0x180);c.setg(17,0x10);c.setg(18,0x20);c.setg(19,0x30);sp=c.g(1)&0xffffffff
 def observe(pc,word):
  if pc==0x8260DD68:raise Stop()
 c.instruction_observer=observe
 try:c.run(0x8260DCF0)
 except Stop:pass
 return c.mem.rd(sp+0x70,64).hex()
for i in range(128):
 rows=[[rng.uniform(-10,10) for j in range(4)] for k in range(4)];a=[rng.uniform(-10,10) for j in range(4)];z=[rng.uniform(-10,10) for j in range(4)];x=run(False,rows,a,z);y=run(True,rows,a,z)
 if x!=y:bad.append({'case':i,'original':x,'compiled':y})
r={'cases':128,'mismatches':bad,'scope':'actual original inline 8260DCF0..DD64 vs compiled real PhysicalBodyPart::GetEventRenderTransform, finite arbitrary transform/offset xyzw; compare all four output rows including W; related pointer reads backed by controlled mapped memory, stop before AddEvent; no enclosing method, exceptional floats or hardware claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(out/'event-transform-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases',128,'mismatches',len(bad),bad[:1]);assert not bad
