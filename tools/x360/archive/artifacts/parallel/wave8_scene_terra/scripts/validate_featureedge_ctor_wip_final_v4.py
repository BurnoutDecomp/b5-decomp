import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'FeatureEdge-wip-v4.obj';b=F.Bundle([F.Coff(obj,'??0FeatureEdge@collision@rw@@QAA@ABVVector3')]);b.select('??0FeatureEdge@collision@rw@@QAA@ABVVector3');rng=random.Random(0x82ba85b8);A=0x50000000;P=0x50001000;Q=0x50002000;bad=[]
for i in range(128):
 x=[rng.uniform(-25,25) for _ in range(4)]; y=[x[j]+rng.uniform(-2,2) for j in range(4)]
 if i%8==0:y=x[:]
 if i%8==1:y=[x[0]+0.00001,x[1],x[2],x[3]]
 src=b''.join(struct.pack('>f',z) for z in x)+rng.randbytes(0x30);dest=b''.join(struct.pack('>f',z) for z in y)+rng.randbytes(0x30);initial=rng.randbytes(0x100);r=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(A,0x100,initial);c.mem.add(P,0x40,src);c.mem.add(Q,0x40,dest)
  if compiled:tab=F.address(b,'VEC_EPSILON')
  else:c.mem.add(0x8327eea0,0x10);tab=0x8327eea0
  c.mem.wr(tab,bytes.fromhex('34000000')*4);c.setg(3,A);c.setg(4,P);c.setg(5,Q);c.run(b.entry if compiled else 0x82ba85b8);r.append((c.mem.rd(A,0x100),c.mem.rd(P,0x40),c.mem.rd(Q,0x40),c.g(3)&0xffffffff))
 if r[0]!=r[1]:bad.append([i,r[0][0][:64].hex(),r[1][0][:64].hex()])
print('cases 128 mismatches',len(bad),bad[:2]);assert not bad
(out/'featureedge-ctor-wip-v4-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual whole FeatureEdge CPP, finite distinct XYZW incl degenerate/near epsilon, initialized epsilon controlled, full object and guards; vector estimate emulator, not hardware'},indent=2))
