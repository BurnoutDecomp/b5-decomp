from pathlib import Path
import sys,json,random,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
base=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra'); out=base/'committed-build';row=next(r for r in json.loads((out/'compile-results.json').read_text())['results'] if r['source'].endswith('CgsSphere.cpp'));A=0x40000000;rng=random.Random(0x825B27F8);results={}
for name,addr in [('GetPosition',0x825B27F8),('GetRadius',0x825BD1F8)]:
 prefix='?'+name+'@Sphere@CgsGeometric@@';b=F.Bundle([F.Coff(Path(row['object']),prefix)]);b.select(prefix);bad=[]
 def run(comp,blob):
  c=F.cpu(b);c.mem.add(A,0x400);c.mem.wr(A,blob);c.setg(3,A+0x200);c.setg(4,A+0x100);c.run(b.entry if comp else addr);return c.mem.rd(A,0x400).hex(),c.gpr[3]
 for i in range(128):
  blob=rng.randbytes(0x400)
  if run(False,blob)!=run(True,blob):bad.append(i)
 results[name]={'cases':128,'mismatches':bad,'original':hex(addr)}
res={'results':results,'scope':'actual original and compiled full member getter, hidden r3 result/r4 this, arbitrary input/guard bytes; compare whole memory and return register','object_sha256':hashlib.sha256(Path(row['object']).read_bytes()).hexdigest()};(out/'sphere-getter-comparison.json').write_text(json.dumps(res,indent=2)+'\n');print(res);assert not any(x['mismatches'] for x in results.values())
