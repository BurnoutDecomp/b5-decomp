import sys,random,json,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');v=out/'Volume-wip-v4.obj';d=out/'VolumeVTables-wip-v4.obj';b=F.Bundle([F.Coff(v,'?InitializeVTable@Volume@'),F.Coff(d,'?gVolumeHandler_82F91740@')]);b.select('?InitializeVTable@Volume@');rng=random.Random(0x82bb03a8);bad=[];original=[0,0x82f91740,0x82f918c0,0x82f919a4,0x82f9176c,0x82f91894,0x82f919d0];actual=[0]+[F.address(b,'?gVolumeHandler_%08X@'%x) for x in original[1:]];compiledTable=F.address(b,'?gVolumeVTable@collision@rw@@')
for i in range(128):
 initial=rng.randbytes(0x40);r=[]
 for compiled in (False,True):
  c=F.cpu(b)
  if compiled:table=compiledTable
  else:c.mem.add(0x8327eee0,0x40);table=0x8327eee0
  if compiled:
   c.mem.wr(table,initial[:28]);c.mem.add(table+28,36,initial[28:])
  else:c.mem.wr(table,initial)
  c.run(b.entry if compiled else 0x82bb03a8)
  data=c.mem.rd(table,28)+c.mem.rd(table+28,36);pointers=[int.from_bytes(data[j:j+4],'big') for j in range(0,28,4)];normalized=[(actual if compiled else original).index(x) if x in (actual if compiled else original) else -1 for x in pointers];r.append((normalized,data[28:],c.g(3)&0xffffffff))
 if r[0]!=r[1]:bad.append([i,r[0][0],r[1][0]])
print('cases 128 mismatches',len(bad),bad[:2]);assert not bad
(out/'volume-registry-wip-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'volume_object_sha256':hashlib.sha256(v.read_bytes()).hexdigest(),'records_object_sha256':hashlib.sha256(d.read_bytes()).hexdigest(),'scope':'actual compiled Volume::InitializeVTable with actual direct-PMF records, seven symbolic pointers mapped to ARTIST identities, poisoned array tail; unresolved method relocations not executed and no full link claim'},indent=2))
