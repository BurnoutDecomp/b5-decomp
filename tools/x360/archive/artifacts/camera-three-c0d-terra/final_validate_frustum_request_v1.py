from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/camera-three-c0d-terra');j=json.loads((O/'camera-final-v1/compile-results.json').read_text());obj=Path(next(x['object'] for x in j['results'] if x['source'].endswith('BrnCollisionPolicyAttachedToVehicle.cpp')));sym='?RequestFrustumLineTests@FrustrumCollisionResolver';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);S=F.M.S.SELF;A=F.M.S.AUX

def run(compiled,box,vec):
 c=F.cpu(b);c.mem.wr(S,box);c.setg(3,S);c.setg(4,A);trace=[]
 for i,v in enumerate(vec):c.vr[i+1]=v
 if compiled:
  for n,(oi,off,sec) in b.definitions.items():
   if n.startswith('?sTestStartWidthPadding@'):c.mem.wr(b.bases[(oi,sec)]+off,struct.pack('>4f',*[.1]*4))
   if n.startswith('?sTestLengthPadding@'):c.mem.wr(b.bases[(oi,sec)]+off,struct.pack('>4f',*[1.]*4))
 else:
  c.mem.add(0x82faa000,0x1000);c.mem.wr(0x82faab50,struct.pack('>4f',*[.1]*4));c.mem.wr(0x82faa910,struct.pack('>4f',*[1.]*4))
 def effect(cc,n):
  if 'LineTestNearest' in n:trace.append((cc.g(3)-A,cc.g(4)-S,cc.g(5),cc.g(6),cc.g(7),cc.g(8),tuple(cc.vr[1]),tuple(cc.vr[2])))
  elif n.startswith('__save') or n.startswith('__rest'):pass
  else:raise AssertionError(n)
 if compiled:
  for at,n in b.unresolved.items():c.hooks[at]=lambda cc,name=n:effect(cc,name)
  c.run(b.entry)
 else:c.hooks[0x82233048]=lambda cc:effect(cc,'LineTestNearest');c.run(0x8223fd70)
 return trace,c.mem.rd(S,len(box))
rng=random.Random(0x8223fd70)
for i in range(128):
 box=rng.randbytes(0x180);vec=[struct.unpack('>4I',struct.pack('>4f',*[rng.uniform(-4,4) for _ in range(4)])) for k in range(5)];a=run(False,box,vec);z=run(True,box,vec)
 if a!=z:print('case',i,'orig',a[0],'actual',z[0]);raise AssertionError(i)
r={'cases':128,'scope':'ARTIST 8223FD70 vs actual compiled RequestFrustumLineTests; four controlled Director LineTestNearest calls, raw v1/v2 lanes, scalar argument registers, full resolver state; finite randomized inputs and VMX emulator estimate model.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'frustum-request-comparison-final-v1.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))

