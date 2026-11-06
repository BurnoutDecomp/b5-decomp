from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformation-header-v1');obj=out/'probe.obj';names=['?ProbeTag@@','?ProbeSpecA@@','?ProbeSpecB@@','?ProbeSpecInitial@@'];b=F.Bundle([F.Coff(obj,n) for n in names]);A=0x40000000;rng=random.Random(0x82625CB8);bad=[]
for i in range(128):
 words=[tuple(struct.unpack('>4I',struct.pack('>4f',*[rng.uniform(-100,100) for j in range(4)]))) for k in range(4)];b.select(names[0]);c=F.cpu(b);c.mem.add(A,0x1000);c.mem.wr(A,struct.pack('>4I',*words[0]));c.mem.wr(A+0x10,(A+0x100).to_bytes(4,'big'));c.mem.wr(A+0x100,b''.join(struct.pack('>4I',*x) for x in words[1:]));c.mem.wr(A+0x200,b'\xA5'*48);c.setg(3,A);c.setg(4,A+0x210);c.run(b.entry);actual=c.mem.rd(A+0x210,16);d=F.cpu(b);d.vr[1]=words[0];d.vr[2]=words[3];d.run(0x82276868);expect=struct.pack('>4I',*d.vr[1]);
 if actual!=expect or c.mem.rd(A+0x200,16)!=b'\xA5'*16 or c.mem.rd(A+0x220,16)!=b'\xA5'*16:bad.append(i)
for index,offset in enumerate([0,16,32]):
 b.select(names[index+1]);c=F.cpu(b);c.mem.add(A,0x1000);c.setg(3,A+0x100);c.run(b.entry)
 if c.g(3)!=(A+0x100+offset):bad.append('pointer'+str(index))
r={'cases':128,'mismatches':bad,'scope':'compiled TagPoint offset inline vs original native vsubfp provider82276868 for finite random xyzw including W and guards; three actual compiled TagPointSpec reference accessors return exact same member addresses +0,+16,+32; no full wheel/IK runtime claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(out/'tag-offset-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases',128,'mismatches',len(bad),bad[:1]);assert not bad
