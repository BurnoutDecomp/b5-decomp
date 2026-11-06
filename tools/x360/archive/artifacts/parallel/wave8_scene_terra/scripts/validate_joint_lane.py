from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformation-header-v1');obj=out/'probe.obj';n='?ProbeSetJoint@@';b=F.Bundle([F.Coff(obj,n)]);b.select(n);A=0x40000000;rng=random.Random(0x825DF868);bad=[]
for i in range(256):
 raw=rng.randbytes(0x200);v=tuple(rng.getrandbits(32) for _ in range(4));c=F.cpu(b);c.mem.add(A,0x1000);c.mem.wr(A,raw);c.setg(3,A);c.vr[1]=v;c.run(b.entry);expected=raw[:0x17c]+struct.pack('>I',v[3])+raw[0x180:]
 if c.mem.rd(A,0x200)!=expected:bad.append(i)
r={'cases':256,'mismatches':bad,'scope':'compiled actual PhysicalBodyPart::SetJointVelocity, compares full 0x200-byte object and raw v1 lane3 against original vrlimi lane selection; arbitrary input bits inc NaNs, no full ResetJointVelocities or hardware claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(out/'joint-lane-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases',256,'mismatches',len(bad));assert not bad
