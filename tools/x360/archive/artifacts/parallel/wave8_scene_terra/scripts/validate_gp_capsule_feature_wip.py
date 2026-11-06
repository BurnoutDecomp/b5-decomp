import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
sys.path.insert(0,r'D:\Reverse\BP-Decomp_Workflow\tools\re');import x360rd
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'GPCapsule-typed-wip.obj';edge=out/'FeatureEdge-final-wip.obj';b=F.Bundle([F.Coff(obj,'?GetMaximumFeature@GPCapsule@'),F.Coff(edge,'??0FeatureEdge@collision@rw@@QAA@ABVVector3')]);b.select('?GetMaximumFeature@GPCapsule@');rng=random.Random(0x82bafa80);A=0x50000000;O=0x50001000;bad=[]
def target(c,pc):
 w=c.fetch(pc);d=w&0x3fffffc
 if d&0x2000000:d-=0x4000000
 return (pc+d)&0xffffffff
for i in range(128):
 src=bytearray(rng.randbytes(0x100));initial=rng.randbytes(0x300);pos=[rng.uniform(-5,5) for _ in range(4)];axis=[1,0,0,rng.uniform(-2,2)];direction=[0,.01,.1,-.1,.05,-.05][i%6];v=tuple(struct.unpack('>I',struct.pack('>f',x))[0] for x in [direction,rng.uniform(-1,1),rng.uniform(-1,1),rng.uniform(-1,1)])
 src[:16]=struct.pack('>4f',*pos);src[0x40:0x50]=struct.pack('>4f',*axis);struct.pack_into('>f',src,0x70,rng.uniform(.1,3));r=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(0x44000000,bytes([i&255])*0x10000);c.mem.add(A,0x100,src);c.mem.add(O,0x300,initial)
  if compiled:tab=F.address(b,'VEC_EPSILON')
  else:
   c.mem.add(0x8327eea0,16);c.mem.add(0x82180000,0x1000,x360rd.rd(0x82180000,0x1000));c.mem.add(0x82001000,0x1000,x360rd.rd(0x82001000,0x1000));tab=0x8327eea0
  c.mem.wr(tab,bytes.fromhex('34000000')*4)
  def copy(cpu):
   a=cpu.g(3)&0xffffffff;z=cpu.g(4)&0xffffffff;n=cpu.g(5)&0xffffffff;cpu.mem.wr(a,cpu.mem.rd(z,n));cpu.setg(3,a)
  if compiled:
   for at,n in b.unresolved.items():
    if n=='memcpy':c.hooks[at]=copy
  else:c.hooks[target(c,0x82bafb10)]=copy
  c.setg(3,A);c.setg(4,i&1);c.setg(5,O);c.vr[1]=v;c.run(b.entry if compiled else 0x82bafa80);r.append((c.mem.rd(O,0x300),c.mem.rd(A,0x100)))
 if r[0]!=r[1]:bad.append([i,r[0][0][0x10:0x50].hex(),r[1][0][0x10:0x50].hex()])
print('cases 128 mismatches',len(bad),bad[:2]);assert not bad
(out/'gp-capsule-feature-wip-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'gp_object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'feature_object_sha256':hashlib.sha256(edge.read_bytes()).hexdigest(),'scope':'whole GPCapsule and real FeatureEdge ctor vs 82BAFA80, edge/point branches incl ±0.05 threshold, controlled identical stack poison and memcpy, full Feature guards; epsilon initialized, estimate emulator'},indent=2))
