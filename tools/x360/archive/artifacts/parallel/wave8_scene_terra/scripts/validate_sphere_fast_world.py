from pathlib import Path
import sys,json,random,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/sphere-wbt');obj=out/'out.obj';prefix='?ProcessSphereTestFast@SceneManagerModule@CgsSceneManager@@';b=F.Bundle([F.Coff(obj,prefix)]);b.select(prefix);A=0x40000000; rng=random.Random(0x828D4090);bad=[]
class Stop(Exception): pass
def run(comp,query):
 c=F.cpu(b);c.mem.add(A,0x400000);c.mem.wr(A+0x100,query);c.mem.wr(A+0x114,(2).to_bytes(4,'big'));cap=[]
 def stop(cpu):
  cap.append((cpu.mem.rd(cpu.g(4)&0xffffffff,16).hex(),cpu.g(3),cpu.g(5),cpu.g(6),cpu.g(7)));raise Stop()
 c.hooks[0x82812950]=stop
 for addr,n in b.unresolved.items():
  if n.startswith('?TestSphereAgainstPolySoupList@'):c.hooks[addr]=stop
 c.setg(3,A+0x1000);c.setg(4,A+0x200);c.setg(5,A+0x300);c.setg(6,A+0x100);c.setg(7,A+0x400);c.setg(8,A+0x500)
 try:c.run(b.entry if comp else 0x828D4090)
 except Stop:pass
 assert cap, 'call not reached'
 return cap[0]
for i in range(128):
 q=rng.randbytes(32);x=run(False,q);y=run(True,q)
 if x!=y:bad.append({'case':i,'original':x,'compiled':y})
res={'cases':128,'mismatches':bad,'scope':'actual original and compiled ProcessSphereTestFast world arm up to TestSphereAgainstPolySoupList call; arbitrary 128 sphere bits and compared receiver, sphere bytes, map pointer, tags; no downstream behavior claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(out/'world-sphere-boundary.json').write_text(json.dumps(res,indent=2)+'\n');print('cases',128,'mismatches',len(bad),bad[:1]);assert not bad
