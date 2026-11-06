from pathlib import Path
import json,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/camera-three-c0d-terra');j=json.loads((O/'render-metrics-v4/compile-results.json').read_text());obj=Path(j['results'][0]['object']);sym='??__E?saAngles@BehaviourRenderMetrics';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);print(b.unresolved);key=next(k for k in b.definitions if k.startswith('?saAngles@BehaviourRenderMetrics'));oi,off,sec=b.definitions[key];addr=b.bases[(oi,sec)]+off
c=F.cpu(b);c.mem.wr(addr,b'\xa5'*128);c.run(b.entry);actual=c.mem.rd(addr,128)
r=F.cpu(b);r.mem.add(0x82faa000,0x1000);r.mem.wr(0x82faab60,b'\xa5'*128);r.run(0x82c495a8);original=r.mem.rd(0x82faab60,128)
if actual!=original:print('differing offsets',[i for i,(x,y) in enumerate(zip(actual,original)) if x!=y]);raise AssertionError('initializer')
out={'cases':1,'scope':'ARTIST 82C495A8 vs actual CRT angle initializer; full 128 bytes including all fourth lanes after poisoned storage.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'render-metrics-angles-comparison-v1.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
