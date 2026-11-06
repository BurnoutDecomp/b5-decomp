import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'PrimitiveIntersect-native-v5.obj';b=F.Bundle([F.Coff(obj,'?PrimitiveBatchIntersect@')]);b.select('?PrimitiveBatchIntersect@');rng=random.Random(0x82babc78);O=0x50000000;GP=0x50002000;GR=0x50003000;R1=0x50004000;R2=0x50005000;V1=0x50006000;V2=0x50007000;D=0x50008000;M=0x50009000;H=0x85000000;bad=[]
def target(c,pc):
 w=c.fetch(pc);d=w&0x3fffffc
 if d&0x2000000:d-=0x4000000
 return (pc+d)&0xffffffff
for i in range(128):
 outbuf=rng.randbytes(0x1000);gp=rng.randbytes(0x200);group=bytearray(rng.randbytes(0x40));r1=bytearray(rng.randbytes(0x80));r2=bytearray(rng.randbytes(0x80));v1=bytearray(rng.randbytes(0x100));v2=bytearray(rng.randbytes(0x100));desc=bytearray(rng.randbytes(0x40));mtx=rng.randbytes(0x80);swapped=i&1;pad=.125
 struct.pack_into('>4I',group,0,R1,1,swapped,R2);struct.pack_into('>III',r1,0,V1,M,0);struct.pack_into('>I',r1,0x70,0x12345678);struct.pack_into('>III',r2,0,V2,M,0);struct.pack_into('>I',r2,0x70,0x87654321);struct.pack_into('>I',v1,0x40,D);struct.pack_into('>I',v2,0x40,D);struct.pack_into('>I',desc,0x14,H);r=[]
 for compiled in (False,True):
  c=F.cpu(b)
  for addr,img in [(O,outbuf),(GP,gp),(GR,group),(R1,r1),(R2,r2),(V1,v1),(V2,v2),(D,desc),(M,mtx)]:c.mem.add(addr,len(img),img)
  trace=[]
  def make(cpu):
   ptr=cpu.g(4)&0xffffffff;trace.append(('make',cpu.g(3)&0xffffffff,ptr,cpu.g(5)&0xffffffff));F.put(cpu,ptr+0x84,0xa0b0c0d0);cpu.setg(3,1);cpu.setf(1,77)
  def batch(cpu):trace.append(('batch',tuple(cpu.g(k)&0xffffffff for k in range(3,8)),cpu.f(1)));cpu.setg(3,0)
  c.hooks[H]=make
  if compiled:
   for fragment in ["?GPInstanceBatchIntersect1xN@", "?GPInstanceBatchIntersectNx1@"]:
    c.hooks[F.address(b,fragment)]=batch
  else:
   for pc in [0x82babd64,0x82babd78]:c.hooks[target(c,pc)]=batch
  c.setg(3,O);c.setg(4,2);c.setg(5,GP);c.setg(6,GR);c.setg(7,1);c.setf(1,pad);c.run(b.entry if compiled else 0x82babc78);r.append((c.mem.rd(GP,0x200),c.mem.rd(O,0x1000),trace,c.g(3)&0xffffffff))
 if r[0]!=r[1]:bad.append([i,r[0][2],r[1][2],r[0][0][:0x100].hex(),r[1][0][:0x100].hex()])
print('cases 128 mismatches',len(bad),bad[:1]);assert not bad
(out/'primitive-batch-whole-v5-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual whole PrimitiveIntersect CPP PrimitiveBatchIntersect vs ARTIST 82BABC78, two real PMF slot +0x14 calls and both swapped batch targets controlled, preserves callback volumeTag and writes VolRef tag into userTag +0x88, padding f1 across calls, poisoned buffers; controlled services only'},indent=2))
