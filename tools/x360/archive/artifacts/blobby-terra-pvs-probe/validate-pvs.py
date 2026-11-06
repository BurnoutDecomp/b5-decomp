from pathlib import Path
import json,random,sys,hashlib,struct
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][0]['object']);sym='?ProbeTrafficPvsCellSize@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);rng=random.Random(77)
for i in range(128):
 data=struct.pack('>4I',*[rng.getrandbits(32) for _ in range(4)]);outs=[]
 for native in (False,True):
  c=F.cpu(b);base=F.M.S.SELF;c.mem.wr(base+0x10,data)
  if native:c.setg(3,base);c.run(b.entry);outs.append(c.vr[1])
  else:
   c.setg(11,base+0x10)
   for pc in (0x827602f0,0x827602f4):assert c.exec_vector(pc,c.fetch(pc),'v')
   outs.append(c.vr[0])
 assert outs[0]==outs[1],(i,outs)
out={'passed':True,'cases':128,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual Pvs header free-return probe vs ARTIST inlined GetCellSize window 827602F0..F8; arbitrary 128-bit patterns, compares all four result bits; v1 native return and original v0 inline result. Full TrafficData caller remains preexisting native compile failure; no runtime claim.'};(D/'pvs-cellsize-comparison.json').write_text(json.dumps(out,indent=2)+'\n');print(out)
