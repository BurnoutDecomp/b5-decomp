from pathlib import Path
import sys,json,struct,hashlib,random
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;r=json.loads((D/'compile-results.json').read_text())['results'];rt=Path(r[0]['object']);cgs=Path(r[1]['object']);rng=random.Random(55);total=0
for sym,provider,orig in [('?Begin@CgsRenderTarget@@','?Begin@RenderTarget@postfx@graphics@rw@@',0x827e7748),('?End@CgsRenderTarget@@','?End@RenderTarget@postfx@graphics@rw@@',0x827e7758)]:
 b=F.Bundle([F.Coff(cgs,sym),F.Coff(rt,provider)]);b.select(sym)
 for i in range(64):
  outs=[]
  for native in (False,True):
   c=F.cpu(b);c.mem.add(0x83271000,0x1000);c.mem.add(0x83010000,0x1000);base=F.M.S.SELF;target=F.M.S.AUX;F.put(c,base+0x108,target);F.put(c,target+4,320+i);F.put(c,target+8,240+i);F.put(c,target+0x10,2 if i%2 else 1);F.put(c,target+0x1c,0x11111111);F.put(c,target+0x24,0x22222222);F.put(c,target+0x28,0x33333333 if i&2 else 0);c.mem.wr(target+0x18,bytes([1 if i&4 else 0]));F.put(c,target+0x84,0x44444444);F.put(c,target+0x88,0x55555555 if i&8 else 0);F.put(c,0x83271614,0x11111111);F.put(c,0x83010a30,0x11111111 if i&16 else 0);F.put(c,0x83271608,0x44556677);trace=[];last=None
   def setstate(x):trace.append(('state',x.g(3)))
   def viewport(x):trace.append(('viewport',x.g(3),x.mem.rd(x.g(4)&0xffffffff,28)))
   def scissor(x):trace.append(('scissor',x.g(3),x.mem.rd(x.g(4)&0xffffffff,16)))
   def resolve(x):trace.append(('resolve',tuple(x.g(j)&0xffffffff for j in range(3,11)),struct.pack('>f',x.f(1)),x.mem.rd((x.g(1)&0xffffffff)+0x5c,4)))
   c.hooks[0x82b61b38]=setstate;c.hooks[0x8293df78]=viewport;c.hooks[0x8293d570]=scissor;c.hooks[0x82b62300]=resolve
   for a,n in b.unresolved.items():
    if '?m_renderTargetState@Device@renderengine' in n:F.put(c,a,0x11111111)
    elif '?mpRenderTargetState@Device@shadow' in n:F.put(c,a,0x11111111 if i&16 else 0);last=a
    elif '?gpD3DDevice@@' in n:F.put(c,a,0x44556677)
    elif '?SetState@Device@renderengine' in n:c.hooks[a]=setstate
    elif n=='D3DDevice_SetViewportF':c.hooks[a]=viewport
    elif n=='D3DDevice_SetScissorRect':c.hooks[a]=scissor
    elif '?Xbox2ResolveTo@PixelBuffer' in n:c.hooks[a]=resolve
   c.setg(3,base);c.run(b.entry if native else orig);outs.append((trace,c.mem.rd(last if native and last else 0x83010a30,4)))
  assert outs[0]==outs[1],(sym,i,outs);total+=1
out={'passed':True,'cases':total,'cgs_object_sha256':hashlib.sha256(cgs.read_bytes()).hexdigest(),'render_target_object_sha256':hashlib.sha256(rt.read_bytes()).hexdigest(),'scope':'Actual linked CgsRenderTarget Begin/End to native RenderTarget provider versus ARTIST 827E7748/7758, 128 cases; compares state bind, viewport, scissor, colour/depth resolve service registers and shared shadow. GPU/device services controlled; no real GPU/runtime claim.'};(D/'cgs-linked-begin-end-comparison.json').write_text(json.dumps(out,indent=2)+'\n');print(out)
