from pathlib import Path
import json,random,struct,sys,hashlib
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][next(i for i,x in enumerate(json.loads((D/'compile-results.json').read_text())['results']) if x['source'].endswith('BrnMapIconManager.cpp'))]['object']);o=F.Coff(obj,'?UpdateWorldIcons@MapIconManager@BrnGui@@');b=F.Bundle([o]);b.select('?UpdateWorldIcons@MapIconManager@BrnGui@@')
rng=random.Random(927);cases=0
for orig,off in [(0x825121dc,0x464),(0x8251225c,0x4d8)]:
 for i in range(96):
  vals=[[rng.uniform(-10000,10000) for _ in range(4)] for __ in range(2)];bits=[struct.unpack('>4I',struct.pack('>4f',*v)) for v in vals];outs=[]
  for native in (False,True):
   c=F.cpu(b);c.vr[13 if native else 0]=bits[0];c.vr[0 if native else 13]=bits[1];pc=b.entry+off if native else orig
   for a in (pc,pc+4):
    w=int.from_bytes(c.mem.rd(a,4),'big');assert c.exec_vector(a,w,"vsubfp" if a==pc else "vmsum3fp128")
   outs.append(c.vr[0])
  assert outs[0]==outs[1],(i,outs);cases+=1
report={'passed':True,'cases':cases,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Original 825121DC..1E4 and 8251225C..264 vs actual UpdateWorldIcons object instruction windows +464..46C and +4D8..4E0, matching register inputs normalized for compiler register allocation. Finite random vectors only; model vmsum3 rule 1, no surrounding caller/control flow or hardware behavior claimed.'};(D/'nearest-distance-window.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

