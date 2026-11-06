from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/gamestate-gui-c0d-terra');j=json.loads((O/'apt-probe/compile-results.json').read_text());obj=Path(j['results'][0]['object']);A=F.M.S.AUX;H=A+0x1000;rng=random.Random(0x82854420)
for name,pc in [('AptHeightProbe',0x82854420),('AptWidthProbe',0x82851ee8)]:
 b=F.Bundle([F.Coff(obj,'?'+name+'@@')]);b.select('?'+name+'@@')
 for i in range(128):
  h=struct.pack('>f',rng.uniform(-100000,100000));w=struct.pack('>f',rng.uniform(-100000,100000));c=F.cpu(b);c.mem.wr(H+0x50,h+w);c.setg(3,H);c.run(b.entry);z=F.cpu(b);z.mem.wr(H+0x50,h+w);z.setg(28,H);z.step(pc,F.M.V.x360rd.u32(pc),pc+4,0);assert c.f(1)==z.f(1),(name,i,c.f(1),z.f(1))
r={'cases_per_getter':128,'scope':'Actual header getter probes versus original ARTIST scalar lfs instructions at 82854420 and 82851EE8, same handler +0x50/+0x54 offsets, finite randomized floats. Full Apt renderer not tested.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'apt-getters-comparison-wip.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
