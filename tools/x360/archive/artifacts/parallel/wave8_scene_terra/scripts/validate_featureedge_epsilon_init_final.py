import sys,random,json,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
sys.path.insert(0,r'D:\Reverse\BP-Decomp_Workflow\tools\re');import x360rd
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'FeatureEdge-final-wip.obj';provider=out/'vecfloat-provider.obj';b=F.Bundle([F.Coff(obj,'??__EVEC_EPSILON@'),F.Coff(provider,'??0VecFloat@')]);b.select('??__EVEC_EPSILON@');rng=random.Random(0x82c73bd0);bad=[]
for i in range(32):
 initial=rng.randbytes(16);res=[]
 for compiled in (False,True):
  c=F.cpu(b)
  if compiled:tab=next(b.bases[(oi,sec)]+value for oi,o in enumerate(b.objects) for name,value,sec in o.symbols.values() if name=='VEC_EPSILON' and (oi,sec) in b.bases)
  else:
   c.mem.add(0x8327eea0,0x10);c.mem.add(0x82180000,0x1000,x360rd.rd(0x82180000,0x1000));tab=0x8327eea0
  c.mem.wr(tab,initial);c.run(b.entry if compiled else 0x82c73bd0);res.append(c.mem.rd(tab,16))
 if res[0]!=res[1]:bad.append(i)
print('cases 32 mismatches',len(bad),bad);assert not bad
(out/'featureedge-epsilon-init-final-comparison.json').write_text(json.dumps({'cases':32,'mismatches':bad,'featureedge_object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'vecfloat_object_sha256':hashlib.sha256(provider.read_bytes()).hexdigest(),'scope':'actual FeatureEdge static initializer linked real VecFloat constructor vs ARTIST 82C73BD0, arbitrary initial bytes'},indent=2))
