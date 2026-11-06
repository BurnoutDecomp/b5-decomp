from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/box-real-mask');obj=out/'box.obj';n3='?BoxOverlappingTest@CgsGeometric@@YA?AVMaskScalar@vpu@math@rw@@ABUBox@1@0VVector3@345@@Z';n2='?BoxOverlappingTest@CgsGeometric@@YA?AVMaskScalar@vpu@math@rw@@ABUBox@1@0@Z';b=F.Bundle([F.Coff(obj,n3),F.Coff(obj,n2)]);b.select(n2);target=F.address(b,n3);A=0x40000000;rng=random.Random(0x825BF090);bad=[]
def run(comp,raw,masks):
 c=F.cpu(b);c.mem.add(A,0x1000);c.mem.wr(A,raw[:80]);c.mem.wr(A+0x100,raw[80:]);calls=[]
 def hook(cpu):
  axis=tuple(cpu.vr[1]);calls.append((list(axis),cpu.g(3),cpu.g(4)));i=[tuple(struct.unpack('>4I',raw[j*16:j*16+16])) for j in [0,1,2,5,6,7]].index(axis);cpu.vr[1]=masks[i]
 c.hooks[0x825BEFA0]=hook;c.hooks[target]=hook;c.setg(3,A);c.setg(4,A+0x100);c.run(b.entry if comp else 0x825BF090);return list(c.vr[1]),sorted(calls)
for i in range(128):
 raw=rng.randbytes(160);m=[tuple(rng.getrandbits(32) for _ in range(4)) for j in range(6)];x=run(False,raw,m);y=run(True,raw,m)
 if x!=y:bad.append(i)
r={'cases':128,'mismatches':bad,'scope':'whole two-argument BoxOverlappingTest vs original 825BF090: six controlled 3-argument callee returns use independent arbitrary 128-bit masks; compares the six axis-register/receiver calls as a multiset and final v1 lane bits, using actual compiled MaskScalar And composition; native compiler reverses call order, and this controlled test does not assert child math or call order','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(out/'box-mask-compose-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('mismatches',len(bad));assert not bad
