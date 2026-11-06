import sys,random,struct,json,hashlib
from pathlib import Path
sys.path.insert(0,r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import native_atomic_fixture as F
out=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave8/repair-r7/caller-repair-terra');obj=out/'AABBox.obj';b=F.Bundle([F.Coff(obj,'?CreateFromTriangle@AABBoxBuilder@')]);b.select('?CreateFromTriangle@AABBoxBuilder@');rng=random.Random(0x82b57da0);bad=[];errors=[]
for i in range(64):
 gp=bytearray(rng.randbytes(0xc0));bbox=rng.randbytes(0x40)
 for off in (0,0x20,0x30):struct.pack_into('>4f',gp,off,*(rng.uniform(-20,20) for _ in range(4)))
 results=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(0x50000000,len(gp),gp);c.setg(3,0x50000000);c.setg(4,0x50000000)
  try:c.run(b.entry if compiled else 0x82b57da0)
  except Exception as ex:errors.append({'case':i,'compiled':compiled,'error':repr(ex)});break
  results.append((c.mem.rd(0x50000000,0xc0)))
 if len(results)==2 and results[0]!=results[1]:bad.append(i)
print('triangle alias',64,'bad',len(bad),'errors',len(errors),bad[:5],errors[:2]);(out/'triangle-alias-v1.json').write_text(json.dumps({'cases':64,'mismatches':bad,'errors':errors,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'original 82B57DA0 vs whole compiled object, finite inputs, all four lanes and full bbox/GP guards; bbox aliases GP at offset zero, no hardware claim'},indent=2)+'\n')
