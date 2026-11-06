from pathlib import Path
import sys,json,random,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/committed-build');row=next(r for r in json.loads((out/'compile-results.json').read_text())['results'] if r['source'].endswith('CgsAxisAlignedBox.cpp'));b=F.Bundle([F.Coff(Path(row['object']),'?Set@AxisAlignedBox@CgsGeometric@@')]);b.select('?Set@AxisAlignedBox@CgsGeometric@@');A=0x40000000;rng=random.Random(0x823A6108);bad=[]
def run(comp,blob,v1,v2):
 c=F.cpu(b);c.mem.add(A,0x100);c.mem.wr(A,blob);c.setg(3,A);c.vr[1]=v1;c.vr[2]=v2;c.run(b.entry if comp else 0x823A6108);return c.mem.rd(A,0x100).hex()
for i in range(128):
 initial=rng.randbytes(0x100);a=tuple(rng.getrandbits(32) for _ in range(4));z=tuple(rng.getrandbits(32) for _ in range(4));x=run(False,initial,a,z);y=run(True,initial,a,z)
 if x!=y:bad.append(i)
res={'cases':128,'mismatches':bad,'scope':'actual whole original 823A6108 and compiled AxisAlignedBox::Set via v1/v2; arbitrary raw lane words including NaNs, compare all object/guard bytes','object_sha256':hashlib.sha256(Path(row['object']).read_bytes()).hexdigest()};(out/'axis-set-comparison.json').write_text(json.dumps(res,indent=2)+'\n');print('cases',128,'mismatches',len(bad));assert not bad
