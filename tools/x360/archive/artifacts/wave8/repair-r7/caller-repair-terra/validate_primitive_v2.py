import sys,random,struct,json,hashlib
from pathlib import Path
sys.path.insert(0,r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import native_atomic_fixture as F
out=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave8/repair-r7/caller-repair-terra');obj=out/'Primitive-v2.obj';prefix='?AddPrimitive@PrimitivePairListBuilder@CgsCollision@CgsSceneManager@@QAAXPBVVolume@';b=F.Bundle([F.Coff(obj,prefix)]);b.select(prefix);kinds={1:('Sphere',0x82814508,16),2:('Capsule',0x82814600,32),4:('Box',0x82814570,80),5:('Cylinder',0x82814678,80)};rng=random.Random(0x82814ab8);bad=[];errors=[];counts={}
for i in range(128):
 kind=(1,2,4,5)[i%4];name,orig,size=kinds[kind];vol=bytearray(rng.randbytes(96));struct.pack_into('>I',vol,0x40,0x50003000)
 for off in (0x44,0x48,0x4c,0x50):struct.pack_into('>f',vol,off,rng.uniform(.1,3))
 desc=struct.pack('>I',kind)+bytes(36);mat=struct.pack('>16f',*(rng.uniform(-2,2) for _ in range(16)));builder=rng.randbytes(128);ret=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(0x50000000,128,builder);c.mem.add(0x50001000,96,vol);c.mem.add(0x50002000,64,mat);c.mem.add(0x50003000,40,desc);trace=[];c.rest=set()
  def leaf(cpu):trace.append(('leaf',cpu.g(3)&0xffffffff,cpu.mem.rd(cpu.g(4)&0xffffffff,size).hex(),cpu.f(1),cpu.g(6)&0xffffffff))
  def boxset(cpu):
   trace.append(('boxset',cpu.mem.rd(cpu.g(4)&0xffffffff,64).hex(),tuple(cpu.vr[1]),tuple(cpu.vr[2])));cpu.mem.wr(cpu.g(3)&0xffffffff,bytes(range(80)))
  leafprefix='?AddPrimitive@PrimitivePairListBuilder@CgsCollision@CgsSceneManager@@QAAXPAU'+name+'@';addr=F.address(b,leafprefix) if compiled else orig;c.hooks[addr]=leaf
  if kind==4:c.hooks[F.address(b,'?Set@Box@CgsGeometric@@') if compiled else 0x825e6918]=boxset
  for a,n in b.unresolved.items():
   if n.startswith('__save'):c.hooks[a]=lambda cpu:None
   elif n.startswith('__rest'):c.rest.add(a);c.hooks[a]=lambda cpu:None
  c.setg(3,0x50000000);c.setg(4,0x50001000);c.setg(5,0x50002000);c.setg(7,0x3344);c.setf(1,.125)
  try:c.run(b.entry if compiled else 0x82814ab8)
  except Exception as ex:errors.append({'case':i,'kind':kind,'compiled':compiled,'error':repr(ex)});break
  ret.append((trace,c.mem.rd(0x50000000,128),c.mem.rd(0x50001000,96),c.mem.rd(0x50002000,64)))
 if len(ret)==2:
  counts[name]=counts.get(name,0)+1
  if ret[0]!=ret[1]:bad.append({'case':i,'kind':kind,'original':ret[0][0],'compiled':ret[1][0]})
print('counts',counts,'mismatches',len(bad),'errors',len(errors),bad[:1],errors[:2]);(out/'primitive-controlled-v2.json').write_text(json.dumps({'counts':counts,'mismatches':bad,'errors':errors,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual 82814AB8 vs compiled source; controlled shape append and Box::Set calls, complete shape payload/transform/volume/builder guards, random finite fields; no unsupported type or hardware claim'},indent=2)+'\n')

