import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'CylinderVolume-wip-v4.obj';edge=out/'FeatureEdge-wip-v4.obj';b=F.Bundle([F.Coff(obj,'?GetMaximumFeature@CylinderVolume@'),F.Coff(edge,'??0FeatureEdge@collision@rw@@QAA@ABVVector3')]);b.select('?GetMaximumFeature@CylinderVolume@');rng=random.Random(0x82bac988);A=0x50000000;O=0x50001000;P=0x50002000;bad=[]
def target(c,pc):
 w=c.fetch(pc);d=w&0x3fffffc
 if d&0x2000000:d-=0x4000000
 return (pc+d)&0xffffffff
for i in range(128):
 src=bytearray(rng.randbytes(0x100));initial=rng.randbytes(0x300)
 for j in list(range(0,0x40,4))+[0x44]:struct.pack_into('>f',src,j,rng.uniform(-10,10))
 if i%8==0:struct.pack_into('>f',src,0x44,0.0)
 poison=bytes([i&255])*0x10000;r=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(0x44000000,poison);c.mem.add(A,0x100,src);c.mem.add(O,0x300,initial);c.mem.add(P,0x40)
  if compiled:tab=F.address(b,'VEC_EPSILON')
  else:c.mem.add(0x8327eea0,0x10);tab=0x8327eea0
  c.mem.wr(tab,bytes.fromhex('34000000')*4)
  def copy(cpu):
   a=cpu.g(3)&0xffffffff;z=cpu.g(4)&0xffffffff;n=cpu.g(5)&0xffffffff;cpu.mem.wr(a,cpu.mem.rd(z,n));cpu.setg(3,a)
  if compiled:
   for at,n in b.unresolved.items():
    if n=='memcpy':c.hooks[at]=copy
  else:c.hooks[target(c,0x82baca08)]=copy
  c.setg(3,A);c.setg(4,i&1);c.setg(5,P);c.setg(6,O);c.run(b.entry if compiled else 0x82bac988);r.append((c.mem.rd(O,0x300),c.mem.rd(A,0x100),c.g(3)&0xffffffff))
 if r[0]!=r[1]:bad.append([i,r[0][0][0x10:0x50].hex(),r[1][0][0x10:0x50].hex()])
print('cases 128 mismatches',len(bad),bad[:2]);assert not bad
(out/'cylinder-maxfeature-whole-wip-v4-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'capsule_object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'featureedge_object_sha256':hashlib.sha256(edge.read_bytes()).hexdigest(),'scope':'actual whole Cylinder and FeatureEdge providers and original 82BAC988->82BA85B8, finite lanes incl zero, identical poisoned stack, epsilon initialized, memcpy controlled, full Feature/guards; emulator estimate not hardware'},indent=2))
