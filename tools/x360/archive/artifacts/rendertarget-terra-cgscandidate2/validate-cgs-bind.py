from pathlib import Path
import sys,json,struct,hashlib,random
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][0]['object']);rng=random.Random(86);total=0
for sym,orig in [('?SetRenderTargetState@CgsRenderTarget@@',0x827e7588),('?SetRenderTargetStateInvertDepth@CgsRenderTarget@@',0x827e7668)]:
 b=F.Bundle([F.Coff(obj,sym)]);b.select(sym)
 def addr(key):
  a=[a for a,n in b.unresolved.items() if key in n];assert len(a)==1,(key,a);return a[0]
 default=addr('?m_renderTargetState@Device@renderengine');last=addr('?mpRenderTargetState@Device@shadow');device=addr('?gpD3DDevice@@')
 for i in range(96):
  state=0 if i%2 else 0x11111111;prev=0x22222222 if (i//2)%2 else 0x33333333;dims=[rng.randrange(1,8192),rng.randrange(1,8192)];outs=[]
  for native in (False,True):
   c=F.cpu(b);c.mem.add(0x83271000,0x1000);c.mem.add(0x83010000,0x1000);base=F.M.S.SELF;target=F.M.S.AUX;F.put(c,base+4,dims[0]);F.put(c,base+8,dims[1]);F.put(c,base+0x108,target);F.put(c,target+0x1c,state);F.put(c,0x83271614,0x22222222);F.put(c,0x83010a30,prev);F.put(c,0x83271608,0x44556677);F.put(c,default,0x22222222);F.put(c,last,prev);F.put(c,device,0x44556677);trace=[]
   def setstate(x):trace.append(('state',x.g(3)))
   def viewport(x):trace.append(('viewport',x.g(3),x.mem.rd(x.g(4)&0xffffffff,28)))
   def scissor(x):trace.append(('scissor',x.g(3),x.mem.rd(x.g(4)&0xffffffff,16)))
   c.hooks[0x82b61b38]=setstate;c.hooks[0x8293df78]=viewport;c.hooks[0x8293d570]=scissor
   for a,n in b.unresolved.items():
    if '?SetState@Device@renderengine' in n:c.hooks[a]=setstate
    elif n=='D3DDevice_SetViewportF':c.hooks[a]=viewport
    elif n=='D3DDevice_SetScissorRect':c.hooks[a]=scissor
   c.setg(3,base);c.setg(4,i%4);c.run(b.entry if native else orig);outs.append((trace,c.mem.rd(last if native else 0x83010a30,4)))
  assert outs[0]==outs[1],(sym,i,outs);total+=1
out={'passed':True,'cases':total,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual CgsRenderTarget SetRenderTargetState and inverted-depth variant vs ARTIST 827E7588/7668: section ignored, default/state selection, prior shadow, dimensions, full 28-byte XDK viewport, full RECT and call order. State/GPU services and device globals controlled; no real hardware/runtime claim.'};(D/'cgs-bind-comparison.json').write_text(json.dumps(out,indent=2)+'\n');print(out)
