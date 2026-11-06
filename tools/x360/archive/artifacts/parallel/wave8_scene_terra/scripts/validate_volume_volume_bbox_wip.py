import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'VolumeQuery-typed-wip6.obj';b=F.Bundle([F.Coff(obj,'?GetPrimitiveBBoxOverlaps@VolumeVolumeQuery@')]);b.select('?GetPrimitiveBBoxOverlaps@VolumeVolumeQuery@');rng=random.Random(0x82bb3ab0);A=0x50000000;V1=0x50001000;V2=0x50002000;D=0x50003000;I=0x50004000;QA=0x50005000;QB=0x50006000;B1=0x50007000;B2=0x50008000;ST=0x50009000;H=0x85000000;bad=[]
class RecordCPU(F.AtomicCPU):
 def step(self,pc,word,nxt,depth):
  if word>>26==31 and ((word>>1)&1023)==444 and word&1:
   src=(word>>21)&31;dst=(word>>16)&31;rhs=(word>>11)&31;value=self.g(src)|self.g(rhs);self.setg(dst,value);self.setcr(0,bool(value>>63),bool(value and not value>>63),value==0);return nxt
  return super().step(pc,word,nxt,depth)
for i in range(128):
 q=bytearray(rng.randbytes(0x100));v1=bytearray(rng.randbytes(0x100));v2=bytearray(rng.randbytes(0x100));d=bytearray(rng.randbytes(0x40));inp=bytearray(rng.randbytes(0x40));qa=bytearray(rng.randbytes(0x100));qb=bytearray(rng.randbytes(0x100));b1=bytearray(rng.randbytes(0x100));b2=bytearray(rng.randbytes(0x100));st=rng.randbytes(0x100);swapped=i&1
 for o,n in [(0,I),(4,0),(8,1),(12,0),(0x10,0),(0x18,ST),(0x20,4),(0x38,V1),(0x3c,0),(0x40,QA),(0x44,QB)]:struct.pack_into('>I',q,o,n)
 struct.pack_into('>f',q,0x14,.25);struct.pack_into('>I',inp,0,V2);struct.pack_into('>I',v1,0x40,D);struct.pack_into('>I',v2,0x40,D);struct.pack_into('>I',d,4,H);struct.pack_into('>I',qa,0xc8,B1);struct.pack_into('>I',qb,0xc8,B2);struct.pack_into('>I',b1,0,V1);struct.pack_into('>I',b2,0,V2)
 b1[0x50:0x70]=struct.pack('>8f',-1,-2,-3,9,1,2,3,11);b2[0x50:0x70]=struct.pack('>8f',-.8,-1.8,-2.8,7,.8,1.8,2.8,12)
 box1=struct.pack('>8f',-1,-1,-1,3,1,1,1,4);box2=struct.pack('>8f',*([-2,-2,-2,5,2,2,2,6] if swapped else [-.5,-.5,-.5,5,.5,.5,.5,6]));r=[]
 for compiled in (False,True):
  c=F.cpu(b);c.__class__=RecordCPU
  for addr,img in [(A,q),(V1,v1),(V2,v2),(D,d),(I,inp),(QA,qa),(QB,qb),(B1,b1),(B2,b2),(ST,st)]:c.mem.add(addr,len(img),img)
  trace=[]
  def box(cpu):
   v=cpu.g(3)&0xffffffff;trace.append(('box',v,cpu.g(4)&0xffffffff,cpu.g(5)&0xffffffff));cpu.mem.wr(cpu.g(6)&0xffffffff,box1 if v==V1 else box2);cpu.setg(3,1)
  def overlap(cpu):
   qaddr=cpu.g(3)&0xffffffff;mem=cpu.mem.rd(qaddr,0x100);trace.append(('overlap',qaddr,mem[0x10:0x30].hex()));cpu.setg(3,1)
  c.hooks[H]=box
  if compiled:
   for addr,n in b.unresolved.items():
    if 'GetOverlaps@VolumeBBoxQuery' in n:c.hooks[addr]=overlap
  else:c.hooks[0x82bbbde8]=overlap
  c.setg(3,A);c.run(b.entry if compiled else 0x82bb3ab0)
  qaout=bytearray(c.mem.rd(QA,0x100));qbout=bytearray(c.mem.rd(QB,0x100));qaout[:8]=b'\0'*8;qbout[:8]=b'\0'*8
  r.append((c.mem.rd(A,0x100),bytes(qaout),bytes(qbout),c.mem.rd(ST,0x100),trace,c.g(3)&0xffffffff))
 if r[0]!=r[1]:bad.append([i,r[0][4],r[1][4]])
print('cases 128 mismatches',len(bad),bad[:1]);assert not bad
(out/'volume-volume-bbox-wip-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual whole GetPrimitiveBBoxOverlaps vs ARTIST 82BB3AB0, two controlled real PMF bbox calls and two controlled BBoxQuery service returns, swapped/unswapped, finite XYZ/W, seeded VolRefs, full normalized query/group guards; local stack-pointer fields normalized; no full real service claim'},indent=2))
