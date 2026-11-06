from pathlib import Path
import sys,json,random,hashlib,struct
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/bodypart-bounded');obj=out/'bounded.obj';name='?GetBoundingBox@PhysicalBodyPart@Deformation@BrnPhysics@@';b=F.Bundle([F.Coff(obj,name)]);b.select(name);setbox=F.address(b,'?Set@Box@CgsGeometric@@');A=0x40000000;rng=random.Random(0x825E7D28);bad=[]
class Stop(Exception):pass
def run(comp,body,bbox,dims):
 c=F.cpu(b);c.mem.add(A,0x1000);c.mem.wr(A,body);c.mem.wr(A+0x120,bbox);c.mem.wr(A+0x1a0,dims);capture=[]
 def stop(cpu):capture.append([cpu.g(3)&0xffffffff,cpu.mem.rd(cpu.g(4)&0xffffffff,64).hex(),list(cpu.vr[1]),list(cpu.vr[2])]);raise Stop()
 c.hooks[0x825E6918]=stop;c.hooks[setbox]=stop;c.setg(3,A);c.setg(4,A+0x500)
 try:c.run(b.entry if comp else 0x825E7D28)
 except Stop:pass
 assert capture;return capture[0]
for i in range(128):
 def rnd(n):return struct.pack('>'+str(n)+'f',*[rng.uniform(-8,8) for j in range(n)])
 body=rnd(16);bbox=rnd(16);dims=rnd(4);x=run(False,body,bbox,dims);y=run(True,body,bbox,dims)
 if x!=y:bad.append({'case':i,'original':x,'compiled':y})
r={'cases':128,'mismatches':bad,'scope':'original whole GetBoundingBox825E7D28 vs bounded actual source method compiled with real headers; controlled Box::Set at call boundary; compares box receiver, all64 composed transform bytes, v1 dimensions and v2 zero bits over finite random xyzw. No real Box validation, whole provider TU, arbitrary FP or runtime claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(out/'bounding-box-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases',128,'mismatches',len(bad),bad[:1]);assert not bad
