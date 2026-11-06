import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'VolumeQuery-typed-wip6.obj';bbox=out/'VolumeBBoxQuery-enum-wip.obj';b=F.Bundle([F.Coff(obj,'?GetResourceDescriptor@VolumeLineQuery@'),F.Coff(bbox,'?GetResourceDescriptor@VolumeBBoxQuery@')]);rng=random.Random(0x82bb3838);A=0x50000000;R=0x50001000;bad=[]
for name,addr in [('?GetResourceDescriptor@VolumeLineQuery@',0x82bb3838),('?Initialize@VolumeLineQuery@',0x82bb3888),('?GetResourceDescriptor@VolumeVolumeQuery@',0x82bb3a20),('?Construct@VolumeVolumeQuery@',0x82bb38f0)]:
 b.select(name)
 for i in range(128):
  data=rng.randbytes(0x300);resource=bytearray(rng.randbytes(0x40));struct.pack_into('>I',resource,0,0 if (i%7==0 and 'Initialize@' in name) else A);volumes=i%13;results=(i*7)%19;r=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.add(A,0x300,data);c.mem.add(R,0x40,resource);c.setg(3,R if 'Initialize@' in name else A);c.setg(4,volumes);c.setg(5,results);c.run(b.entry if compiled else addr);r.append((c.mem.rd(A,0x300),c.mem.rd(R,0x40),c.g(3)&0xffffffff))
  if r[0]!=r[1]:bad.append([name,i,r[0][2],r[1][2]])
print('cases 512 mismatches',len(bad),bad[:3]);assert not bad
(out/'volume-query-resources-wip-enum-v6-comparison.json').write_text(json.dumps({'cases':512,'mismatches':bad,'query_object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'bbox_object_sha256':hashlib.sha256(bbox.read_bytes()).hexdigest(),'scope':'actual whole VolumeQuery and linked actual VolumeBBoxQuery descriptor provider vs four ARTIST resource methods, independent volumes/results, null Initialize, full poisoned buffers/guards; allocation capacity/runtime not tested'},indent=2))
