from pathlib import Path
import sys,json,struct,hashlib,random
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;r=json.loads((D/'compile-results.json').read_text())['results'];obj=Path(r[0]['object']);sym='?Begin@RenderTarget@postfx@graphics@rw@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);rng=random.Random(54)
def addr(key):
 a=[a for a,n in b.unresolved.items() if key in n];assert len(a)==1,(key,a);return a[0]
defptr=addr('?m_renderTargetState@Device@renderengine');lastptr=addr('?mpRenderTargetState@Device@shadow');deviceptr=addr('?gpD3DDevice@@')
for i in range(96):
 mode=2 if i%2 else 1;section=(i//2)%3;prev=(0x11111111 if (i//6)%2 else 0x33333333);dims=[rng.randrange(1,8192),rng.randrange(1,8192)];outs=[]
 for native in (False,True):
  c=F.cpu(b);c.mem.add(0x83271000,0x1000);c.mem.add(0x83010000,0x1000);base=F.M.S.SELF;F.put(c,base+4,dims[0]);F.put(c,base+8,dims[1]);F.put(c,base+0x10,mode)
  for j in range(3):F.put(c,base+0x1c+4*j,0x11111111+j)
  F.put(c,0x83271614,0x11111111);F.put(c,0x83010a30,prev);F.put(c,0x83271608,0x44556677);F.put(c,defptr,0x11111111);F.put(c,lastptr,prev);F.put(c,deviceptr,0x44556677);trace=[]
  def setstate(x):trace.append(('state',x.g(3)))
  def viewport(x):trace.append(('viewport',x.g(3),x.mem.rd(x.g(4)&0xffffffff,28)))
  def scissor(x):trace.append(('scissor',x.g(3),x.mem.rd(x.g(4)&0xffffffff,16)))
  c.hooks[0x82b61b38]=setstate;c.hooks[0x8293df78]=viewport;c.hooks[0x8293d570]=scissor
  for a,n in b.unresolved.items():
   if '?SetState@Device@renderengine' in n:c.hooks[a]=setstate
   elif n=='D3DDevice_SetViewportF':c.hooks[a]=viewport
   elif n=='D3DDevice_SetScissorRect':c.hooks[a]=scissor
  c.setg(3,base);c.setg(4,section);c.run(b.entry if native else 0x823f9250);outs.append((trace,c.mem.rd(lastptr if native else 0x83010a30,4)))
 assert outs[0]==outs[1],(i,mode,section,dims,outs)
out={'passed':True,'cases':96,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual RenderTarget::Begin vs ARTIST 823F9250: default/per-section choice, repeated/new shadow state, finite unsigned dimensions; compares state bind, full 28-byte XDK viewport, full RECT, call order and resulting shared shadow. GPU/state setter services and device globals controlled. No real GPU/runtime claim.'};(D/'begin-comparison.json').write_text(json.dumps(out,indent=2)+'\n');print(out)
