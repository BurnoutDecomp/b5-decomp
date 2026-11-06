from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/driven-point-header');obj=out/'probe.obj';names=['?DrivenSet@@','?DrivenOffset@@','?DrivenPosition@@','?DrivenDirection@@'];b=F.Bundle([F.Coff(obj,n) for n in names]);A=0x40000000;rng=random.Random(0x825C1264);bad=[]
for i in range(128):
 old=rng.randbytes(64);v=tuple(rng.getrandbits(32) for _ in range(4));b.select(names[0]);c=F.cpu(b);c.mem.add(A,0x1000);c.mem.wr(A,old);c.setg(3,A);c.vr[1]=v;c.run(b.entry);expected=struct.pack('>3I',*v[:3])+old[12:];
 if c.mem.rd(A,64)!=expected:bad.append(['set',i])
 vals=[tuple(struct.unpack('>4I',struct.pack('>4f',*[rng.uniform(-100,100) for j in range(4)]))) for k in range(2)];b.select(names[1]);c=F.cpu(b);c.mem.add(A,0x1000);c.mem.wr(A,struct.pack('>4I',*vals[0]));c.mem.wr(A+0x20,(A+0x100).to_bytes(4,'big'));c.mem.wr(A+0x100,struct.pack('>4I',*vals[1]));c.setg(3,A);c.setg(4,A+0x200);c.run(b.entry);d=F.cpu(b);d.vr[1]=vals[0];d.vr[2]=vals[1];d.run(0x82276868)
 if c.mem.rd(A+0x200,16)!=struct.pack('>4I',*d.vr[1]):bad.append(['sub',i])
for index,off in [(2,0),(3,16)]:
 b.select(names[index]);c=F.cpu(b);c.mem.add(A,0x1000);c.setg(3,A);c.run(b.entry)
 if c.g(3)!=A+off:bad.append(['ref',index])
r={'cases':128,'mismatches':bad,'scope':'actual compiled IKDrivenPoint inline accessors; SetPosition compares xyz input and preserved old W plus all guard bytes for arbitrary raw lanes (original vrlimi825B98F8); offset compares all finite xyzw lanes against original Subtract82276868; GetPosition/GetDirection reference identities +0/+10; no full IK solver or hardware claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(out/'comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases',128,'mismatches',len(bad),bad[:1]);assert not bad
