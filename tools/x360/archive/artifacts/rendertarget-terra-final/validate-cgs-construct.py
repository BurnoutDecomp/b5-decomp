from pathlib import Path
import sys,json,hashlib,random,struct
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;r=json.loads((D/'compile-results.json').read_text())['results'];cgs=Path(r[1]['object']);rt=Path(r[0]['object']);sym='?Construct@CgsRenderTarget@@';init='?Initialize@RenderTarget@postfx@graphics@rw@@';b=F.Bundle([F.Coff(cgs,sym),F.Coff(rt,init)]);b.select(sym);nativeinit=[b.bases[(oi,s)]+v for n,(oi,v,s) in b.definitions.items() if n.startswith(init) and (oi,s) in b.bases];assert len(nativeinit)==1,nativeinit;rng=random.Random(293)
for i in range(96):
 initial=bytearray(0x110);struct.pack_into('>4I',initial,4,320+i,240+i,3,i%3);initial[0x14]=1+i%3;initial[0x15]=i%2
 for k in range(5):
  off=0x18+k*0x30;struct.pack_into('>I',initial,off,1+k%2);initial[off+4]=(i>>k)&1;initial[off+5]=(i+k)%2;initial[off+6]=(i>>(k+1))&1;struct.pack_into('>4I',initial,off+8,0x18280186,0x18280186,k+4,0xffffffff);initial[off+0x18]=0xfe;initial[off+0x19]=0xfd;struct.pack_into('>I',initial,off+0x1c,0xfffffffe);struct.pack_into('>I',initial,off+0x20,0x86006000 if k==4 and i%3==0 else 0);struct.pack_into('>I',initial,off+0x24,1+k%3);struct.pack_into('>I',initial,off+0x28,0x86007000 if (i+k)%4==0 else 0);struct.pack_into('>I',initial,off+0x2c,0x86008000+k*0x100)
 outs=[]
 for native in (False,True):
  c=F.cpu(b);base=F.M.S.SELF;c.mem.wr(base,bytes(initial));c.mem.wr(F.M.S.STACK,b'\xa5'*0x20000);c.setg(1,F.M.S.STACK+0x10000);c.setg(3,base);c.setg(4,0x86000000);trace=[]
  def initialize(x):trace.append(x.mem.rd(x.g(3)&0xffffffff,0x198));x.setg(3,0x86009000)
  c.hooks[0x82409b60]=initialize;c.hooks[nativeinit[0]]=initialize;c.run(b.entry if native else 0x827ecc28);outs.append((c.mem.rd(base+0x108,8),trace))
 assert outs[0]==outs[1],(i,outs[0][0].hex(),outs[1][0].hex(),[(hex(j),x,y) for j,(x,y) in enumerate(zip(outs[0][1][0],outs[1][1][0])) if x!=y][:40])
out={'passed':True,'cases':96,'cgs_object_sha256':hashlib.sha256(cgs.read_bytes()).hexdigest(),'render_target_object_sha256':hashlib.sha256(rt.read_bytes()).hexdigest(),'scope':'Actual CgsRenderTarget::Construct + real native Parameters constructor vs ARTIST 827ECC28/827E73B0 across four colour records, depth supplied/shared modes and input flags; compares full 0x198 parameter block handed to controlled RenderTarget::Initialize and resulting pointers. Stack prefilled identically; Initialize/GPU controlled. No full runtime claim.'};(D/'cgs-construct-comparison.json').write_text(json.dumps(out,indent=2)+'\n');print(out)
