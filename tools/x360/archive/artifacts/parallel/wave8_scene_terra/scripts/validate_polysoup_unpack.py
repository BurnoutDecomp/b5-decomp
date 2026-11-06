from pathlib import Path
import sys,json,random,hashlib,struct
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/sphere13-polysoup');obj=out/'poly.obj';name='?UnpackPolygonSoupVertices@CgsGeometric@@';b=F.Bundle([F.Coff(obj,name)]);b.select(name);A=0x40000000;rng=random.Random(0x8283B480);bad=[]
class Cpu(F.AtomicCPU):
 def exec_vector(self,pc,word,mnemonic):
  if word==0x100c0080:
   self.vr[0]=tuple((x+y)&0xffffffff for x,y in zip(self.vr[12],self.vr[0]));return True
  return super().exec_vector(pc,word,mnemonic)
def run(comp,origin,scale,data,count):
 c=F.cpu(b);c.__class__=Cpu;c.mem.add(A,0x3000);c.mem.add(0x83039000,0x1000);c.mem.wr(0x830393D0,bytes.fromhex('00000001000002030000040500000000'));c.mem.wr(0x830391F0,bytes.fromhex('0000ffff0000ffff0000ffff00000000'));c.mem.wr(A+0x100,struct.pack('>3if',*origin,scale));c.mem.wr(A+0x11c,bytes([count]));c.mem.wr(A+0x400,data+b'\0'*32);c.mem.wr(A+0x800,b'\xA5'*160);calls=[]
 def vertex(cpu):calls.append([cpu.g(3)&0xffffffff,cpu.g(4)&0xffffffff]);cpu.setg(3,A+0x400)
 c.hooks[0x8283A9F0]=vertex
 for addr,sym in b.unresolved.items():
  if '?GetVertex@PolygonSoup' in sym:c.hooks[addr]=vertex
  elif sym.startswith('__save') or sym.startswith('__rest'):c.hooks[addr]=lambda cpu:None
 c.setg(3,A+0x800);c.setg(4,A+0x100);c.run(b.entry if comp else 0x8283B480);return calls,c.mem.rd(A+0x800,160).hex()
for i in range(128):
 origin=[rng.randrange(-1000,1000) for j in range(3)];scale=[.125,.5,1.,2.][i%4];count=i%6;data=b''.join(struct.pack('>3H',*[rng.randrange(65536) for j in range(3)]) for k in range(count));a=run(False,origin,scale,data,count);z=run(True,origin,scale,data,count)
 if a!=z:bad.append({'case':i,'original':a,'compiled':z})
r={'cases':128,'mismatches':bad,'scope':'actual original8283B480 vs actual compiled UnpackPolygonSoupVertices, controlled GetVertex and original initialized permutation/mask data, 0..5 big-endian vertices, finite power-of-two scales; compares all four lanes including original nonzero W and output guards. No arbitrary scale/rounding, full soup or runtime claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(out/'unpack-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases',128,'mismatches',len(bad),bad[:1]);assert not bad
