from pathlib import Path
import json,random,sys,hashlib,struct
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][-1]['object']);o=F.Coff(obj,'?CalculateAlpha@MapIconManager@BrnGui@@');b=F.Bundle([o]);b.select('?CalculateAlpha@MapIconManager@BrnGui@@');stat={}
for n,v,s in o.symbols.values():
 for k in ['sfFarAlpha','sfNearAlpha','suAlphaInitialised']:
  if k in n and (0,s) in b.bases:stat[k]=b.bases[(0,s)]+v
rng=random.Random(330);aux=F.M.S.AUX;base=F.M.S.SELF
for i in range(96):
 distance=[0.,849.0,850.0,1200.0,2150.0,2151.0][i%6] if i<36 else rng.uniform(0,3000);iv=[0.,rng.uniform(-50,50),0.,rng.uniform(-50,50)];cv=[distance,0.,rng.uniform(-50,50),rng.uniform(-50,50)];vb1=struct.unpack('>4I',struct.pack('>4f',*iv));vb2=struct.unpack('>4I',struct.pack('>4f',*cv));flags=(i//6)%4;far=struct.pack('>f',33.);near=struct.pack('>f',77.);outs=[]
 for native in [False,True]:
  c=F.cpu(b);c.mem.add(0x82fb5000,0x1000);orig=[0x82fb5918,0x82fb5914,0x82fb591c];loc=[stat['sfFarAlpha'],stat['sfNearAlpha'],stat['suAlphaInitialised']] if native else orig;c.mem.wr(loc[0],far);c.mem.wr(loc[1],near);F.put(c,loc[2],flags);c.setg(3,base);c.vr[1]=vb1;c.vr[2]=vb2;c.run(b.entry if native else 0x82502940);outs.append((struct.pack('>f',c.f(1)),c.mem.rd(loc[0],4),c.mem.rd(loc[1],4),c.mem.rd(loc[2],4)))
 assert outs[0]==outs[1],(i,distance,flags,[[v.hex() for v in x] for x in outs])
report={'passed':True,'cases':96,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Whole actual CalculateAlpha vs ARTIST82502940: V3 v1/V2 v2, zero, threshold-adjacent and finite distances, all four initial lazy-static flag states. Compares f1 and both alpha/flag stores. vrsqrtefp modeled, not Xenon tables; exceptional inputs and frame excluded.'};(D/'alpha-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
