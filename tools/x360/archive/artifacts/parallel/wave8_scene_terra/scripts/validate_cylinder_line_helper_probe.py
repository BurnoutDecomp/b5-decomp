import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
sys.path.insert(0,r'D:\Reverse\BP-Decomp_Workflow\tools\re');import x360rd
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'cylinder-line-helper-probe.obj';b=F.Bundle([F.Coff(obj,'?rwcCylinderLineSegIntersect@')]);b.select('?rwcCylinderLineSegIntersect@');rng=random.Random(0x82baf8a0);O=0x50000000;bad=[];branches=set()
for i in range(256):
 def bits(v):return tuple(struct.unpack('>I',struct.pack('>f',x))[0] for x in v)
 if i%4==0:vec=[bits((0,0,0,1)),bits((1,0,0,2)),bits((.1,0,0,3)),bits((0,0,1,4))];lensq=1;radius=1
 elif i%4==1:vec=[bits((0,0,0,1)),bits((-1,0,0,2)),bits((2,0,0,3)),bits((0,0,1,4))];lensq=1;radius=.5
 elif i%4==2:vec=[bits((0,0,0,1)),bits((4,0,0,2)),bits((2,0,0,3)),bits((0,0,1,4))];lensq=1;radius=.5
 else:vec=[bits([rng.uniform(-3,3) for k in range(4)]) for j in range(4)];lensq=rng.uniform(.1,3);radius=rng.uniform(.1,3)
 inv=(i//4)&1;ignore=(i//8)&1;initial=rng.randbytes(0x40);r=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(O,0x40,initial)
  if not compiled:c.mem.add(0x82001000,0x1000,x360rd.rd(0x82001000,0x1000));c.mem.add(0x82003000,0x1000,x360rd.rd(0x82003000,0x1000))
  c.setg(3,O);c.setg(6,inv);c.setg(7,ignore);c.setf(1,lensq);c.setf(2,radius)
  for k,v in enumerate(vec):c.vr[k+1]=v
  c.run(b.entry if compiled else 0x82baf8a0);r.append((c.mem.rd(O,0x40),c.g(3)&0xffffffff))
 branches.add(r[0][1])
 if r[0]!=r[1]:bad.append([i,r[0][1],r[1][1],r[0][0][:8].hex(),r[1][0][:8].hex()])
print('cases 256 branches',branches,'mismatches',len(bad),bad[:2]);assert not bad and branches=={0,1,0xffffffff}
(out/'cylinder-line-helper-probe-comparison.json').write_text(json.dumps({'cases':256,'branches':sorted(branches),'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'temporary extracted rwcCylinder helper; actual XDK fmsubs/fmadds/fnmsubs/fsqrts, finite all return branches and both inversion/inside modes, poisoned fraction guards; NOT whole provider'},indent=2))
