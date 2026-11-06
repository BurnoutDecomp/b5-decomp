from pathlib import Path
import sys,json,struct,hashlib,itertools
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;r=json.loads((D/'compile-results.json').read_text())['results'];obj=Path(r[0]['object']);cases=0
for sym,original in [('?Resolve@Target@postfx@graphics@rw@@QAAXXZ',0x823f9118),('?Resolve@RenderTarget@postfx@graphics@rw@@QAAX_N0@Z',0x823f9338)]:
 b=F.Bundle([F.Coff(obj,sym)]);b.select(sym)
 for mask in range(16):
  trace=[]
  for native in (False,True):
   c=F.cpu(b);base=F.M.S.SELF;F.put(c,base+4,0x11223344);F.put(c,base+8,0x55667788 if mask&1 else 0);F.put(c,base+0x24,0x11223344);F.put(c,base+0x28,0x55667788 if mask&1 else 0);F.put(c,base+0x84,0x22334455);F.put(c,base+0x88,0x66778899 if mask&2 else 0);c.mem.wr(base+0x18,bytes([bool(mask&4)]));c.setg(3,base);c.setg(4,1 if mask&8 else 0);c.setg(5,1 if mask&1 else 0);calls=[]
   def resolve(x):calls.append((tuple(x.g(j)&0xffffffff for j in range(3,11)),struct.pack('>f',x.f(1)),x.mem.rd((x.g(1)&0xffffffff)+0x5c,4)));x.setg(3,1)
   c.hooks[0x82b62300]=resolve
   for a,n in b.unresolved.items():
    if '?Xbox2ResolveTo@PixelBuffer' in n:c.hooks[a]=resolve
   c.run(b.entry if native else original);trace.append(calls)
  assert trace[0]==trace[1],(sym,mask,trace);cases+=1
out={'passed':True,'cases':cases,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual Target::Resolve and RenderTarget::Resolve vs ARTIST 823F9118/823F9338 over texture/null, depth/colour enable combinations; compares full receiver/argument registers, clear depth and overflow clear-stencil word to controlled GPU resolve service. No GPU hardware or whole runtime claim.'};(D/'resolve-comparison.json').write_text(json.dumps(out,indent=2)+'\n');print(out)
