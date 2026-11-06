from pathlib import Path
import sys,json,random,hashlib,struct
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
base=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra');out=base/'sphere-initial';rows=json.loads((out/'compile-results.json').read_text())['results']+json.loads((base/'axis-consumers-fixed/compile-results.json').read_text())['results'];by={x['source'].split('/')[-1]:x for x in rows};by['PolygonSoupTesterJob.cpp']=next(x for x in rows if x['source'].endswith('PolygonSoupTesterJob.cpp'));b=F.Bundle([F.Coff(Path(by['PolygonSoupTesterJob.cpp']['object']),'?FillTriangleCache@PolygonSoupTesterJob@@'),F.Coff(Path(by['CgsAxisAlignedBox.cpp']['object']),'?Set@AxisAlignedBox@CgsGeometric@@'),F.Coff(Path(by['CgsSphere.cpp']['object']),'?GetPosition@Sphere@CgsGeometric@@'),F.Coff(Path(by['CgsSphere.cpp']['object']),'?GetRadius@Sphere@CgsGeometric@@')]);b.select('?FillTriangleCache@PolygonSoupTesterJob@@');axis=F.address(b,'?Set@AxisAlignedBox@CgsGeometric@@');A=0x40000000;SPHERE=A+0x1D000;rng=random.Random(0x829161A0);bad=[]
class Stop(Exception):pass
def run(comp,lanes):
 c=F.cpu(b);c.mem.add(A,0x24000);c.mem.wr(SPHERE,struct.pack('>4f',*lanes));capture=[]
 def stop(cpu):capture.append([list(cpu.vr[1]),list(cpu.vr[2])]);raise Stop()
 def pos(cpu):
  dest=cpu.g(3)&0xffffffff;src=cpu.g(4)&0xffffffff;cpu.mem.wr(dest,cpu.mem.rd(src,16));cpu.setg(3,dest)
 def radius(cpu):
  dest=cpu.g(3)&0xffffffff;src=cpu.g(4)&0xffffffff;v=cpu.mem.rd(src+12,4);cpu.mem.wr(dest,v*4);cpu.setg(3,dest)
 c.hooks[0x823A6108]=stop;c.hooks[axis]=stop;c.hooks[0x8284CB38]=lambda cpu:None
 c.setg(3,A);c.setg(4,A+0x1C000);c.setg(5,SPHERE);c.setg(6,A+0x1E000);c.setg(7,10);c.setg(8,A+0x1F000)
 try:c.run(b.entry if comp else 0x82915FD0)
 except Stop:pass
 assert capture;return capture[0]
for i in range(128):
 lanes=[rng.uniform(-1e4,1e4) for _ in range(3)]+[rng.uniform(0,1000)];x=run(False,lanes);y=run(True,lanes)
 if x!=y:bad.append({'case':i,'original':x,'compiled':y})
res={'cases':128,'mismatches':bad,'scope':'actual whole PolygonSoupTesterJob::FillTriangleCache original 82915FD0 vs compiled caller, through AxisAlignedBox::Set invocation; all min/max v1/v2 lane bits and register order, finite sphere inputs. Compiled and original Sphere getters both executed from real providers. Captures at boundary, does not claim whole fill/query/provider or link parity.','polygon_object_sha256':hashlib.sha256(Path(by['PolygonSoupTesterJob.cpp']['object']).read_bytes()).hexdigest(),'axis_object_sha256':hashlib.sha256(Path(by['CgsAxisAlignedBox.cpp']['object']).read_bytes()).hexdigest()};(out/'polygon-axis-real-getters.json').write_text(json.dumps(res,indent=2)+'\n');print('cases',128,'mismatches',len(bad));print(bad[:1]);assert not bad
