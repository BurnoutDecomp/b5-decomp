import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
sys.path.insert(0,r'D:\Reverse\BP-Decomp_Workflow\tools\re');import x360rd
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'thin-triangle-probe.obj';b=F.Bundle([F.Coff(obj,'?ThinTriangleLineSegIntersect@')]);b.select('?ThinTriangleLineSegIntersect@');rng=random.Random(0x82bb9eb8);O=0x50000000;bad=[];hits=0
for i in range(192):
 def bits(v):return tuple(struct.unpack('>I',struct.pack('>f',x))[0] for x in v)
 if i%3==0:
  w=lambda:rng.uniform(-2,2)
  vectors=[bits((rng.uniform(-0.2,1.2),rng.uniform(-0.2,1.2),0.5,w())),bits((0,0,-1,w())),bits((0,0,0,w())),bits((1,0,0,w())),bits((0,1,0,w()))]
 else:vectors=[bits([rng.uniform(-2,2) for _ in range(4)]) for k in range(5)]
 initial=rng.randbytes(0x100);r=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(O,0x100,initial)
  if not compiled:
   for a in [0x82001000,0x8200d000,0x82180000]:c.mem.add(a,0x1000,x360rd.rd(a,0x1000))
  c.setg(3,O)
  for k,v in enumerate(vectors):c.vr[k+1]=v
  c.run(b.entry if compiled else 0x82bb9eb8);r.append((c.mem.rd(O,0x100),c.g(3)&0xffffffff))
 if r[0][1]:hits+=1
 if r[0]!=r[1]:bad.append([i,r[0][1],r[1][1],r[0][0][:0x50].hex(),r[1][0][:0x50].hex()])
print('cases 192 hits',hits,'mismatches',len(bad),bad[:2]);assert not bad and hits>0
(out/'thin-triangle-probe-comparison.json').write_text(json.dumps({'cases':192,'hits':hits,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'temporary extracted thin triangle kernel, finite random and targeted hit/miss; full result and guards incl miss-path raw lineParam store; emulator reciprocal model, NOT whole provider'},indent=2))
