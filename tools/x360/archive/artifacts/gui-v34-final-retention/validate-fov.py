from pathlib import Path
import json,random,struct,sys,hashlib,math
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][next(i for i,x in enumerate(json.loads((D/'compile-results.json').read_text())['results']) if x['source'].endswith('BrnMapIconManager.cpp'))]['object']);o=F.Coff(obj,'?GetSatNavIconStateForRival@MapIconManager@BrnGui@@');b=F.Bundle([o]);b.select('?GetSatNavIconStateForRival@MapIconManager@BrnGui@@');stat={}
for n,v,s in o.symbols.values():
 if (0,s) in b.bases and 'GetSatNavIconStateForRival' in n:
  if n.startswith('?sfRivalFov@'):stat['value']=b.bases[(0,s)]+v
  if n.startswith('?$S1@'):stat['flag']=b.bases[(0,s)]+v
assert len(stat)==2,stat
class Stop(Exception): pass
class WindowCPU(F.AtomicCPU):
 def exec_vector(self,a,w,mn):
  if w>>26==6 and (w&0x7f0)==0x670:
   vd=((w>>21)&31)|(((w>>2)&3)<<5);vb=((w>>11)&31)|((w&3)<<5)
   self.setvf(vd,[1.0/math.sqrt(float(x)) for x in self.vf(vb)])
   return True
  return super().exec_vector(a,w,mn)
rng=random.Random(512);tested=0
for i in range(96):
 angle=rng.uniform(-3.0,3.0);dist=[0,99,100,101,1000][i%5];icon=[rng.uniform(-dist,dist) for j in range(3)]+[rng.uniform(-10,10)];point=struct.pack('>4f',*icon);flags=(i//5)%4;vals=[]
 for native in (False,True):
  c=F.cpu(b);c.__class__=WindowCPU;c.mem.add(0x82fb5000,0x1000);base=F.M.S.SELF;other=F.M.S.AUX;c.mem.wr(other,point);c.mem.wr(base+0x960,bytes(16));c.mem.wr(base+0x978,struct.pack('>f',angle));c.setg(25,0);c.setg(31,14)
  if native:c.setg(27,other);c.setg(28,base);fl=stat['flag'];va=stat['value'];entry=b.entry+0x24c;stop=b.entry+0x3c4
  else:c.setg(26,other);c.setg(24,base);fl=0x82fb58ec;va=0x82fb58e8;entry=0x824fa60c;stop=0x824fa7c4
  F.put(c,fl,flags);c.mem.wr(va,struct.pack('>f',1.57));c.instruction_observer=lambda pc,w,stop=stop: (_ for _ in ()).throw(Stop()) if pc==stop else None
  try:c.run(entry)
  except Stop:pass
  else:raise AssertionError('did not stop')
  vals.append((c.g(3) if native else c.g(31),c.mem.rd(fl,4),c.mem.rd(va,4)))
 assert vals[0]==vals[1],(i,dist,angle,vals);tested+=1
r={'passed':True,'cases':tested,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Original 824FA60C..7C4 vs actual GetSatNavIconStateForRival object +24C..3C4, equal live icon/player and heading input, all 4 static flag states; compares return and lazy flag/value. Full outer function/control services and hardware estimate tables excluded.'};(D/'fov-window-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(r)
