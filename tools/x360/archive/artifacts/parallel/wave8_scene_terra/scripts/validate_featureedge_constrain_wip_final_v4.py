import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
sys.path.insert(0,r'D:\Reverse\BP-Decomp_Workflow\tools\re');import x360rd
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'FeatureEdge-wip-v4.obj';b=F.Bundle([F.Coff(obj,'?constrain_point@FeatureEdge@')]);b.select('?constrain_point@FeatureEdge@');rng=random.Random(0x82bb7728);A=0x50000000;P=0x50001000;bad=[];branches=set()
for i in range(192):
 edge=bytearray(rng.randbytes(0x100));p=bytearray(rng.randbytes(0x40));base=[0.0,0.0,0.0,rng.uniform(-2,2)];direction=[1.0,0.0,0.0,rng.uniform(-2,2)];pt=[[-1.0,1.0,3.0][i%3],rng.uniform(-2,2),rng.uniform(-2,2),rng.uniform(-2,2)]
 edge[:16]=struct.pack('>4f',*base);edge[16:32]=struct.pack('>4f',*direction);edge[48:64]=struct.pack('>4f',2,2,2,2);p[:16]=struct.pack('>4f',*pt);r=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(A,0x100,edge);c.mem.add(P,0x40,p)
  if not compiled:c.mem.add(0x82001000,0x1000,x360rd.rd(0x82001000,0x1000))
  c.setg(3,A);c.setg(4,P);c.run(b.entry if compiled else 0x82bb7728);r.append((c.mem.rd(P,0x40),c.mem.rd(A,0x100),c.g(3)&0xffffffff))
 branches.add(r[0][2])
 if r[0]!=r[1]:bad.append([i,r[0][2],r[1][2],r[0][0][:16].hex(),r[1][0][:16].hex()])
print('cases 192 branches',branches,'mismatches',len(bad),bad[:2]);assert not bad and branches=={1,2,3}
(out/'featureedge-constrain-wip-v4-comparison.json').write_text(json.dumps({'cases':192,'branches':sorted(branches),'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual whole FeatureEdge CPP, finite XYZ/W across all three branches, point/object guards; emulator, no hardware claim'},indent=2))
