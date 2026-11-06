from pathlib import Path
import json,random,struct,sys,hashlib
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][next(i for i,x in enumerate(json.loads((D/'compile-results.json').read_text())['results']) if x['source'].endswith('BrnMapIconManager.cpp'))]['object']);rng=random.Random(672);cases=0
for name,orig,offset in [('?UpdateSatNavIcons@MapIconManager@BrnGui@@',0x82522cb0,0x62c),('?UpdateCrashNavIcons@MapIconManager@BrnGui@@',0x825219f8,0x638)]:
 o=F.Coff(obj,name);b=F.Bundle([o]);b.select(name)
 for i in range(96):
  vals=[struct.unpack('>4I',struct.pack('>4f',*[rng.uniform(-100,100) for _ in range(4)])) for j in range(2)];outs=[]
  for native in (False,True):
   c=F.cpu(b);c.vr[127]=vals[0];c.vr[0]=vals[1];addrs=[b.entry+offset+x for x in (0,4,8,12,16)] if native else [orig,orig+0x1c,orig+0x28,orig+0x2c,orig+0x30]
   for a in addrs:
    w=int.from_bytes(c.mem.rd(a,4),'big');assert c.exec_vector(a,w,'vector')
   outs.append(c.vr[0])
  assert outs[0]==outs[1],(i,outs);cases+=1
r={'passed':True,'cases':cases,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Both original proximity square/add windows 82522CB0..CE4 and 825219F8..21A2C versus actual native UpdateSatNavIcons +62C and UpdateCrashNavIcons +638; same all-lane inputs, compares full dot register. Finite random inputs only, branch and outer behavior excluded.'};(D/'proximity-window-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(r)
