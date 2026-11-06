from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/spiralling-r2-terra');obj=O/'vector_data_probes.obj';S=F.M.S.SELF;fields=[('angle',0),('speed',4),('height',8),('growth',12),('radius',16),('radial',20),('offset',24)];bundles={}
for name,off in fields:
 for kind in ['set','get']:
  sym='?'+kind+'_'+name;b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);bundles[(kind,name)]=b
rng=random.Random(0x8222c088)
for i in range(256):
 state=rng.randbytes(32);values=[0.0,-0.0,1.0,-1.0,struct.unpack('>f',bytes.fromhex('00800000'))[0]];v=values[i%len(values)] if i%2==0 else struct.unpack('>f',struct.pack('>f',rng.uniform(-1000,1000)))[0];raw=struct.pack('>f',v)
 for name,off in fields:
  b=bundles[('set',name)];c=F.cpu(b);c.mem.wr(S,state);c.setg(3,S);c.setf(1,v);c.run(b.entry);expect=state[:off]+raw+state[off+4:]
  if c.mem.rd(S,32)!=expect:raise AssertionError((i,name,'set'))
  b=bundles[('get',name)];c=F.cpu(b);c.mem.wr(S,expect);c.setg(3,S);c.run(b.entry)
  if struct.pack('>f',c.f(1))!=raw:raise AssertionError((i,name,'get'))
r={'cases':256,'checks':3584,'scope':'Actual header VecFloatRef accessors: each setter preserves 28 other bytes, each const getter returns the selected 32-bit finite/signed-zero lane. XDK static asserts VectorData 32/16, Parameters 0xB0 and behaviour 0x300. Validated SDK proxy code, no standalone original getter ABI asserted because accessors inline.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'vector-data-proxy-comparison-v1.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
