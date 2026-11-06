from pathlib import Path
import sys,json,struct,random,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/traffic-plus-abi-terra');j=json.loads((O/'wip1/compile-results.json').read_text());obj=Path(next(x['object'] for x in j['results'] if x['source'].endswith('BrnTrafficLightTrigger.cpp')));sym='?GetTransform@LightTrigger@BrnTraffic@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);A=F.M.S.AUX;TRIGGER=A+0x1000;OUT=A+0x2000
class Stop(Exception):pass
def run(compiled,data,result):
 c=F.cpu(b);c.mem.wr(TRIGGER+0x10,data);c.mem.wr(OUT,bytes(64));seen=[]
 def hook(cc,n):
  if 'ExpandPosPlusYRotToTransform' not in n:raise AssertionError(n)
  if compiled:
   assert cc.g(3)==OUT;seen.append(tuple(cc.vr[1]));cc.mem.wr(OUT,result)
  else:
   assert cc.g(3)==cc.g(1)+0xd0;seen.append(tuple(cc.vr[1]));raise Stop()
 if compiled:
  c.setg(3,OUT);c.setg(4,TRIGGER)
  for at,n in b.unresolved.items():c.hooks[at]=lambda cc,name=n:hook(cc,name)
  c.run(b.entry);assert c.mem.rd(OUT,64)==result
 else:
  c.setg(31,TRIGGER);c.setg(23,16);c.setg(28,0);c.setg(29,0);pc=0x82747800;w=F.M.V.x360rd.u32(pc);d=w&0x03fffffc;d=d-0x04000000 if d&0x02000000 else d;c.hooks[(pc+d)&0xffffffff]=lambda cc:hook(cc,'ExpandPosPlusYRotToTransform')
  try:c.run(0x827477f0)
  except Stop:pass
 assert len(seen)==1
 return seen[0]
rng=random.Random(0x827477f0)
for i in range(128):
 data=rng.randbytes(16);result=rng.randbytes(64);assert run(False,data,result)==run(True,data,result),i
r={'cases':128,'scope':'Original ManageTriggers inline 827477F0..82747800 versus compiled actual LightTrigger::GetTransform. All four raw input lanes in v1 and each caller\'s proper matrix sret pointer verified; transform provider controlled and candidate full 64-byte return copied.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'trigger-caller-comparison-wip.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
