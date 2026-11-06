from pathlib import Path
import sys,json,struct
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path('D:/Reverse/BP-X360-Experimental-Artifacts/blobby-terra-traffic-step4');j=json.loads((D/'compile-results.json').read_text());o=F.Coff(Path(j['results'][0]['object']),'?RenderTrafficCar@TrafficEntityModule@BrnTraffic@@');b=F.Bundle([o]);b.select('?RenderTrafficCar@TrafficEntityModule@BrnTraffic@@')
class Stop(Exception):pass
for native in (False,True):
 c=F.cpu(b); st=c.g(1);rt=F.M.S.SELF;phy=F.M.S.AUX;up=phy+0x1000
 pack=lambda x:struct.pack('>4f',*x)
 for at,v in [(rt,(-1.,2.,3.,4.)),(rt+16,(2.,3.,4.,5.)),(rt+32,(6.,7.,8.,9.)),(phy+0x520,(0.,1.,0.,0.5)),(up,(0.,1.,0.,0.))]:c.mem.wr(at,pack(v))
 mat=[(1.,0.,0.,0.),(0.,1.,0.,0.),(0.,0.,1.,0.),(10.,20.,30.,1.)]
 at=st+(0xb0 if native else 0x190)
 c.mem.wr(at,b''.join(map(pack,mat)))
 if native:
  for r,v in [(28,rt),(30,16),(31,32),(23,1),(25,phy),(19,0x11112222),(11,0)]:c.setg(r,v)
  start=b.entry+0x344;stop=b.entry+0x458
 else:
  for r,v in [(30,rt),(15,32),(31,0),(23,0),(20,phy),(28,up),(22,0x11112222)]:c.setg(r,v)
  c.vr[0]=struct.unpack('>4I',pack(mat[2]));start=0x827293bc;stop=0x82729554
 c.instruction_observer=lambda pc,w: (_ for _ in ()).throw(Stop()) if pc==stop else None
 try:c.run(start)
 except Stop:print(native,hex(c.g(3)),hex(c.g(4)),c.vr[1:4],c.mem.rd(c.g(4)&0xffffffff,64).hex())
 except Exception as e:print(native,hex(c.last_pc),repr(e));raise
