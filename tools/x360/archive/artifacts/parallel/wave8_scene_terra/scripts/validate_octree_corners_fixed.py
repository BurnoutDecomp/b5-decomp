from pathlib import Path
import sys,json,random,hashlib,struct
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/octree-corners');row=json.loads((out/'compile-results.json').read_text())['results'][0];b=F.Bundle([F.Coff(Path(row['object']),'?CalcNodeCorners@LooseOctree@')]);b.select('?CalcNodeCorners@LooseOctree@');A=0x40000000;N=A+0x1000;O=A+0x2000;rng=random.Random(0x828B0D40);bad=[]
def run(comp,pos,hs,hh):
 c=F.cpu(b);c.mem.add(A,0x4000);c.mem.wr(N,bytes(0x60));c.mem.wr(O,b'\xa5'*128);c.mem.wr(N,struct.pack('>4f',*pos));c.mem.wr(N+0x2c,struct.pack('>f',hs));c.mem.wr(N+0x38,struct.pack('>f',hh));c.setg(3,A);c.setg(4,N);c.setg(5,O);c.run(b.entry if comp else 0x828B0D40);return c.mem.rd(O,128).hex()
for i in range(128):
 pos=[rng.uniform(-1e3,1e3) for _ in range(4)];hs=rng.uniform(0,100);hh=rng.uniform(0,100);x=run(False,pos,hs,hh);y=run(True,pos,hs,hh)
 if x!=y:bad.append({'case':i,'original':x,'compiled':y})
res={'cases':128,'mismatches':bad,'scope':'actual original 828B0D40 vs compiled CalcNodeCorners, finite random node centre/extents; all 8 full vector lanes compared; no NaN, parent class or complete tree claim','object_sha256':hashlib.sha256(Path(row['object']).read_bytes()).hexdigest()};(out/'corners-comparison.json').write_text(json.dumps(res,indent=2)+'\n');print('cases',128,'mismatches',len(bad));print(bad[:1]);assert not bad

