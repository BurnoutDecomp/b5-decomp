from pathlib import Path
import sys,json,random,hashlib,struct
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/committed-build');row=next(r for r in json.loads((out/'compile-results.json').read_text())['results'] if r['source'].endswith('CgsAxisAlignedBox.cpp'));b=F.Bundle([F.Coff(Path(row['object']),'?ContainsPoint@AxisAlignedBox@CgsGeometric@@')]);b.select('?ContainsPoint@AxisAlignedBox@CgsGeometric@@');A=0x40000000;rng=random.Random(0x828AA398);bad=[]
def run(comp,initial,v):
 c=F.cpu(b);c.mem.add(A,0x100);c.mem.wr(A,initial);c.setg(3,A);c.setvf(1,v);c.run(b.entry if comp else 0x828AA398);return c.g(3)&0xffffffff
for i in range(128):
 lo=[rng.uniform(-10,0) for _ in range(4)];hi=[rng.uniform(0,10) for _ in range(4)];v=[rng.uniform(-12,12) for _ in range(4)];initial=struct.pack('>8f',*(lo+hi))+rng.randbytes(224);x=run(False,initial,v);y=run(True,initial,v)
 if x!=y:bad.append({'case':i,'original':x,'compiled':y})
res={'cases':128,'mismatches':bad,'scope':'actual whole original 828AA398 and compiled ContainsPoint via v1, finite randomly sampled values; NaNs, full object and runtime excluded','object_sha256':hashlib.sha256(Path(row['object']).read_bytes()).hexdigest()};(out/'axis-contains-comparison.json').write_text(json.dumps(res,indent=2)+'\n');print('cases',128,'mismatches',len(bad));print(bad[:1]);assert not bad
