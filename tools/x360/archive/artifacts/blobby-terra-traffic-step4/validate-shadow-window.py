from pathlib import Path
import sys,json,struct,random,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;j=json.loads((D/'compile-results.json').read_text());obj=Path(j['results'][0]['object']);o=F.Coff(obj,'?RenderTrafficCar@TrafficEntityModule@BrnTraffic@@');b=F.Bundle([o]);b.select('?RenderTrafficCar@TrafficEntityModule@BrnTraffic@@')
class Stop(Exception):pass
rng=random.Random(82729554)
for case in range(256):
 physical=bool(case%2);vec=lambda:[rng.uniform(-50,50) for _ in range(4)];offset=vec();offset[0]=rng.uniform(-20,0.099);half=vec();axles=vec();normal=vec();mat=[vec() for _ in range(4)];outs=[]
 for native in (False,True):
  c=F.cpu(b);st=c.g(1);rt=F.M.S.SELF;phy=F.M.S.AUX;up=phy+0x1000;pack=lambda x:struct.pack('>4f',*x)
  for at,v in [(rt,offset),(rt+16,half),(rt+32,axles),(phy+0x520,normal),(up,(0.,1.,0.,0.))]:c.mem.wr(at,pack(v))
  c.mem.wr(st+(0xb0 if native else 0x190),b''.join(map(pack,mat)))
  if native:
   for r,v in [(28,rt),(30,16),(31,32),(23,int(physical)),(25,phy),(19,0x11112222),(11,0)]:c.setg(r,v)
   start=b.entry+0x344;stop=b.entry+0x458
  else:
   for r,v in [(30,rt),(15,32),(31,0),(23,int(not physical)),(20,phy),(28,up),(22,0x11112222)]:c.setg(r,v)
   c.vr[0]=struct.unpack('>4I',pack(mat[2]));c.setf(31,0.);start=0x827293bc;stop=0x82729554
  c.instruction_observer=lambda pc,w: (_ for _ in ()).throw(Stop()) if pc==stop else None
  try:c.run(start)
  except Stop:pass
  else:raise AssertionError(('no call',case,native))
  outs.append((c.g(3),tuple(c.vr[1]),tuple(c.vr[2]),tuple(c.vr[3]),c.mem.rd(c.g(4)&0xffffffff,64)))
 assert outs[0]==outs[1],(case,outs)
out={'passed':True,'cases':256,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'ARTIST 827293BC..82729554 and actual RenderTrafficCar COFF +344..458: randomized finite full lanes, alternating live physical/nonphysical, assert-success inputs. Independently initialized live window registers, stack and real runtime records; compare call receiver, v1/v2/v3 and all 64 affine bytes. VMX emulator; no prior control flow, replay, assert-failure, full caller or GPU claim.'}
(D/'traffic-shadow-window-comparison.json').write_text(json.dumps(out,indent=2)+'\n');print(out)
