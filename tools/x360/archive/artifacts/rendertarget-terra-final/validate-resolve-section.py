from pathlib import Path
import sys,json,struct,hashlib,random
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][0]['object']);sym='?Resolve@Target@postfx@graphics@rw@@QAAXI@Z';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);device=next(a for a,n in b.unresolved.items() if '?gpD3DDevice@@' in n);rng=random.Random(111)
for i in range(96):
 width=rng.randint(1,16000);height=rng.randint(1,32000);sections=[0,1,2,4][i%4];section=(i//4)%6;hastex=(i//24)%2;outs=[]
 for native in (False,True):
  c=F.cpu(b);c.mem.add(0x83271000,0x1000);base=F.M.S.SELF;surface=F.M.S.AUX;F.put(c,base+4,surface);F.put(c,base+8,0x12345678 if hastex else 0);c.mem.wr(base+0x14,bytes([sections]));F.put(c,surface+0x24,((width-1)<<18)|((height-1)<<3)|5);F.put(c,0x83271608,0x44556677);F.put(c,device,0x44556677);trace=[]
  def pix(x):trace.append(('pixel',tuple(x.g(j)&0xffffffff for j in range(3,11)),struct.pack('>f',x.f(1)),x.mem.rd((x.g(1)&0xffffffff)+0x5c,4)))
  def gpu(x):trace.append(('gpu',x.g(3),x.g(4),x.mem.rd(x.g(5)&0xffffffff,16),x.g(6),x.mem.rd(x.g(7)&0xffffffff,8),tuple(x.g(j)&0xffffffff for j in (8,9,10)),struct.pack('>f',x.f(1)),x.mem.rd((x.g(1)&0xffffffff)+0x5c,4),x.mem.rd((x.g(1)&0xffffffff)+0x64,4)))
  c.hooks[0x82b62300]=pix;c.hooks[0x82948eb0]=gpu
  for a,n in b.unresolved.items():
   if '?Xbox2ResolveTo@PixelBuffer' in n:c.hooks[a]=pix
   elif n=='D3DDevice_Resolve':c.hooks[a]=gpu
  c.setg(3,base);c.setg(4,section);c.run(b.entry if native else 0x823f9170);outs.append(trace)
 assert outs[0]==outs[1],(i,sections,section,width,height,outs)
out={'passed':True,'cases':96,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual Target::Resolve(section) vs ARTIST 823F9170 across missing texture, one-section face and multi-section atlas, varying native XDK surface dimensions and section. Compares full GPU/PixelBuffer call registers, rectangle, destination, float depth and stack arguments; services/device global controlled. No real GPU/runtime claim.'};(D/'resolve-section-comparison.json').write_text(json.dumps(out,indent=2)+'\n');print(out)
