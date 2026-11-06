import sys,random,struct,json,hashlib
from pathlib import Path
sys.path.insert(0,r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import native_atomic_fixture as F
out=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave8/repair-r7/caller-repair-terra');obj=out/'AABBox.obj';b=F.Bundle([F.Coff(obj,'?CreateFromBox@AABBoxBuilder@')]);b.select('?CreateFromBox@AABBoxBuilder@');rng=random.Random(0x82b57de0);bad=[];errors=[];dirs=struct.pack('>12f',1,0,0,0,0,1,0,0,0,0,1,0)
for i in range(64):
 gp=bytearray(rng.randbytes(0xc0));struct.pack_into('>I',gp,0xac,0x50008000);bbox=rng.randbytes(0x40);interval=bytearray(rng.randbytes(0x90))
 for j in range(3):
  for off in (0,16):struct.pack_into('>4f',interval,j*0x30+off,*(rng.uniform(-20,20) for _ in range(4)))
 results=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(0x50000000,len(gp),gp);c.mem.add(0x50001000,len(bbox),bbox);c.mem.add(0x83270f80,0x40,dirs+struct.pack('>I',1)+bytes(12));trace=[]
  def intervals(cpu):
   rr=cpu.g(3)&0xffffffff;pp=cpu.g(4)&0xffffffff;nn=cpu.g(5)&0xffffffff;oo=cpu.g(6)&0xffffffff;trace.append((rr,nn,cpu.mem.rd(pp,48).hex()));cpu.mem.wr(oo,interval)
  c.hooks[0x50008000]=intervals
  for a,n in b.unresolved.items():
   if n.startswith('__save'):c.hooks[a]=lambda cpu:None
  c.setg(3,0x50001000);c.setg(4,0x50000000)
  try:c.run(b.entry if compiled else 0x82b57de0)
  except Exception as ex:errors.append({'case':i,'compiled':compiled,'error':repr(ex)});break
  results.append((c.mem.rd(0x50001000,0x40),c.mem.rd(0x50000000,0xc0),trace))
 if len(results)==2 and results[0]!=results[1]:bad.append({'case':i,'original':results[0][0].hex(),'compiled':results[1][0].hex(),'trace_orig':results[0][2],'trace_new':results[1][2]})
print('box',64,'bad',len(bad),'errors',len(errors),bad[:1],errors[:2]);(out/'box-controlled-v1.json').write_text(json.dumps({'cases':64,'mismatches':bad,'errors':errors,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'original 82B57DE0 vs actual object; controlled member interval provider, distinct nonbroadcast lanes, directions/count/receiver and complete bbox/GP guards; finite values and no hardware claim'},indent=2)+'\n')
