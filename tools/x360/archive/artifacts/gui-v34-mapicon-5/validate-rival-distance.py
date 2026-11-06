from pathlib import Path
import json,hashlib,random,struct,sys
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][0]['object']);o=F.Coff(obj,'?GetSatNavIconStateForRival@MapIconManager@BrnGui@@');b=F.Bundle([o]);b.select('?GetSatNavIconStateForRival@MapIconManager@BrnGui@@');rng=random.Random(420)
for i in range(96):
 inputs=[struct.unpack('>4I',struct.pack('>4f',*[rng.uniform(-10000,10000) for _ in range(4)])) for k in range(2)];outs=[]
 for native in (False,True):
  c=F.cpu(b);c.vr[0]=inputs[0];c.vr[13]=inputs[1]
  for a in ((b.entry+0x26c,b.entry+0x284) if native else (0x824fa61c,0x824fa62c)):
   w=int.from_bytes(c.mem.rd(a,4),'big');assert c.exec_vector(a,w,'vsubfp' if a in (b.entry+0x26c,0x824fa61c) else 'vmsum3fp128')
  outs.append(c.vr[0])
 assert outs[0]==outs[1],(i,outs)
r={'passed':True,'cases':96,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Original 824FA61C and 824FA62C vs actual GetSatNavIconStateForRival object +26C and +284; all four lanes of finite inputs, modeled vmsum3 rule1. Threshold branch and subsequent FOV behavior excluded.'};(D/'rival-distance-window.json').write_text(json.dumps(r,indent=2)+'\n');print(r)
