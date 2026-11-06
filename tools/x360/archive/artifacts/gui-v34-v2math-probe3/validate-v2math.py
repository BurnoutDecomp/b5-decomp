from pathlib import Path
import json,random,sys,hashlib,struct
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
class W(F.AtomicCPU):
 def exec_vector(self,pc,w,m):
  if pc==self.stop:raise F.CPU.run.__globals__['Return']()
  return super().exec_vector(pc,w,m)
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][0]['object']);rng=random.Random(439);aux=F.M.S.AUX
for prefix,stop in [('?ProbeV2Dot@@',0x8245ecd8),('?ProbeV2Magnitude@@',0x8245ed10)]:
 b=F.Bundle([F.Coff(obj,prefix)]);b.select(prefix)
 for i in range(96):
  vals=[rng.uniform(-1000,1000),rng.uniform(-1000,1000),rng.uniform(-1000,1000),rng.uniform(-1000,1000)];vals[:2]=[0.0,-0.0] if i==0 else vals[:2];raw=struct.pack('>4f',*vals);w=struct.unpack('>4I',raw);outs=[]
  for native in [False,True]:
   c=F.cpu(b);c.__class__=W;c.mem.wr(aux,raw);c.setg(8,aux);c.vr[1]=w;c.vr[2]=w;c.vr[127]=(0,)*4;c.vr[125]=(0x3f000000,)*4;c.vr[9]=(0x3f800000,)*4;c.vr[6]=(0x3f800000,)*4;c.stop=0xffffffff if native else stop;c.run(b.entry if native else 0x8245ecc4);outs.append(struct.pack('>4I',*c.vr[1]) if native else struct.pack('>4I',*c.vr[0]))
  assert outs[0]==outs[1],(prefix,i,vals,[x.hex() for x in outs])
report={'passed':True,'cases':192,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual Dot(v,v) and Magnitude probes vs ARTIST8245ECC4..ED10, 95 finite positive dot inputs and one signed-zero input each; by-value VPU2 v1/v2 inputs and full VecFloat v1 result, all four input lanes varied. vrsqrtefp uses model not Xenon hardware; NaN/denormal cases excluded.'};(D/'v2math-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
