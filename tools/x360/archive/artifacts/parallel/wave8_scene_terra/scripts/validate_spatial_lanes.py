from pathlib import Path
import sys,json,random,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/header-initial');r=next(x for x in json.loads((out/'compile-results.json').read_text())['results'] if x['source'].endswith('CgsSpatialPartition.cpp'));b=F.Bundle([F.Coff(Path(r['object']),'?AllocEntity@SpatialPartition@')]);b.select('?AllocEntity@SpatialPartition@');A=0x40000000;rng=random.Random(0x828B1078);bad=[]
def run(comp,idx,flags,vec,radius,initial):
 c=F.cpu(b);c.mem.add(A,0x40000);c.mem.wr(A,initial);c.setg(3,A);c.setg(4,idx);c.setg(5,flags);c.setvf(1,vec);c.setf(1,radius);c.run(b.entry if comp else 0x828B1078);return {'sphere':c.mem.rd(A+0x13900+16*idx,16).hex(),'flag':c.mem.rd(A+0x80+8*idx,4).hex(),'ret':c.g(3)&0xffffffff}
for i in range(128):
 idx=rng.randrange(0,10000);flags=rng.getrandbits(32);vec=[rng.uniform(-1e4,1e4) for _ in range(4)];radius=rng.uniform(-1e3,1e3);initial=rng.randbytes(0x40000);a=run(False,idx,flags,vec,radius,initial);z=run(True,idx,flags,vec,radius,initial)
 if a!=z:bad.append({'case':i,'original':a,'compiled':z})
res={'cases':128,'mismatches':bad,'scope':'actual original 828B1078 versus compiled AllocEntity for valid IDs; compare sphere xyz/radius lanes, type flag, returned record pointer; intentionally excludes other record bytes, assertions, whole class and runtime behavior','object_sha256':hashlib.sha256(Path(r['object']).read_bytes()).hexdigest()};(out/'alloc-lane-comparison.json').write_text(json.dumps(res,indent=2)+'\n');print('cases',len(range(128)),'mismatches',len(bad));print(bad[:1]);assert not bad
