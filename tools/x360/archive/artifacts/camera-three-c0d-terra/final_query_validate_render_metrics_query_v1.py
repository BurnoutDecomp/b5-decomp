from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/camera-three-c0d-terra');j=json.loads((O/'final-query-v1/compile-results.json').read_text());obj=Path(next(x['object'] for x in j['results'] if x['source'].endswith('BrnBehaviourRenderMetrics.cpp')));sym='?RequestPosition@BehaviourRenderMetrics';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);S=F.M.S.SELF;A=F.M.S.AUX;SH=A+0x1000;REQ=A+0x2000

def run(compiled,state,pos):
 c=F.cpu(b);c.mem.wr(S,state);c.mem.wr(SH+0x5d8,struct.pack('>I',REQ));c.setg(3,S);c.setg(4,SH);c.vr[1]=pos;trace=[]
 def hook(cc):trace.append((cc.g(3)-REQ,cc.g(4)-S,cc.g(5),cc.g(6),cc.g(7)&0xffffffff,cc.g(8),tuple(cc.vr[1]),tuple(cc.vr[2])))
 if compiled:
  for at,n in b.unresolved.items():c.hooks[at]=hook
  c.run(b.entry)
 else:c.hooks[0x82233048]=hook;c.run(0x82248ee8)
 return trace,c.mem.rd(S,len(state))
rng=random.Random(0x82248ee8)
for i in range(128):
 state=rng.randbytes(0x100);pos=struct.unpack('>4I',struct.pack('>4f',*[rng.uniform(-200,200) for _ in range(4)]));a=run(False,state,pos);z=run(True,state,pos)
 if a!=z:print('case',i,a[0],z[0]);raise AssertionError(i)
r={'cases':128,'scope':'ARTIST 82248EE8 vs actual RequestPosition; finite randomized four-lane positions, controlled Director query, scalar registers, all query lanes and full behavior state.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'render-metrics-query-comparison-final-query-v1.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
