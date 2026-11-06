from pathlib import Path
import hashlib,json,random,struct,sys
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(parent/'scratch/x360_sdk'));sys.path.insert(0,str(parent/'tools/re'));import native_atomic_fixture as F;import x360rd as R
out=parent/'b5-decomp/build/x360/parallel/native_textures/state-provider-core2';paths={Path(r['source']).name:Path(r['object']) for r in json.loads((out/'compile-results.json').read_text())['results']}
bundle=F.Bundle([F.Coff(paths[n],p) for n,p in [('statehelper.cpp','?Initialize@StateHelper@'),('blendstate.cpp','?Initialize@BlendState@'),('depthstencilstate.cpp','?Initialize@DepthStencilState@'),('rasterizerstate.cpp','?Initialize@RasterizerState@'),('BaseResourceDescriptor.cpp','??0BaseResourceDescriptor@')]])
SELF=F.M.S.SELF;AUX=F.M.S.AUX;rng=random.Random(0x82b64108);errors=[];checks=0
addresses={'?m_blendStates@':0x8327187c,'?m_defaultDepthStencilState@':0x832718d0,'?m_defaultRasterizerState@':0x832718d4,'?m_defaultViewportParams@':0x82f87b64,'?m_defaultScissorRectParams@':0x832718d8,'?m_currentState@':0x83271600,'?gOriginalColorBuffer@':0x83271100}
def select(prefix):
 bundle.select(prefix)
 for (oi,sec),base in bundle.bases.items():
  obj=bundle.objects[oi];body=bundle.images[base]
  for at,index,kind in obj.relocations(sec):
   name,_,_=obj.symbols[index];matches=[a for n,a in addresses.items() if name.startswith(n)]
   if not matches:continue
   address=matches[0];w=struct.unpack_from('>I',body,at)[0]
   if kind in [0x10,0x11]:w=(w&0xffff0000)|(((address+0x8000)>>16 if kind==0x10 else address)&0xffff)
   elif kind==2:w=address
   else:raise AssertionError((name,kind))
   struct.pack_into('>I',body,at,w)
def branch(pc):
 w=R.u32(pc);d=w&0x3fffffc
 return (pc+d-(1<<26 if d&(1<<25) else 0))&0xffffffff
def mapped(c):c.mem.add(0x83271000,0x2000);c.mem.add(0x82f87000,0x1000)
def copy(x):x.mem.wr(x.g(3)&0xffffffff,x.mem.rd(x.g(4)&0xffffffff,x.g(5)&0xffffffff))
def snapshot(c):return c.mem.rd(0x8327187c,16).hex()+c.mem.rd(0x832718d0,24).hex()+c.mem.rd(0x82f87b64,28).hex()
select('?Initialize@StateHelper@')
cpp_desc={F.address(bundle,'?GetResourceDescriptor@'+t+'@'):t for t in ['BlendState','DepthStencilState','RasterizerState']};cpp_init={F.address(bundle,'?Initialize@'+t+'@'):t for t in ['BlendState','DepthStencilState','RasterizerState']}
for i in range(192):
 globals_=rng.randbytes(0x2000);viewport=rng.randbytes(28);blocks=rng.randbytes(0xc00);state=[0,1,2,0xffffffff,3,0x7fffffff,0x80000000,2][i%8];both=[]
 for compiled in [False,True]:
  c=F.cpu(bundle);mapped(c);c.mem.wr(0x83271000,globals_);c.mem.wr(0x82f87b64,viewport);c.mem.wr(AUX+0x8000,blocks);F.put(c,0x83271600,state);sp=c.g(1)&0xffffffff;c.mem.wr(sp-0x2000,bytes([i])*0x2000);trace=[];counts={'registry':0,'allocate':0,'initialize':0}
  for n in range(3):F.put(c,AUX+0x1000+n*0x100,AUX+0x2000)
  F.put(c,AUX+0x2010,0x84803000)
  def registry(x):
   counts['registry']+=1;trace.append(('registry',counts['registry'],snapshot(x)));x.setg(3,AUX+0x1000+0x100*((i+counts['registry'])%3))
  def allocate(x):
   n=counts['allocate'];counts['allocate']+=1;trace.append(('allocate',n,x.g(4)&0xffffffff,x.mem.rd(x.g(5)&0xffffffff,40).hex(),x.g(6)&0xffffffff,snapshot(x)));x.mem.wr(x.g(3)&0xffffffff,struct.pack('>5I',AUX+0x8000+n*0x200,0xa0000000+i+n,0xb0000000+i+n,0xc0000000+i+n,0xd0000000+i+n))
  def observe(pc,word):
   typ=cpp_desc.get(pc) if compiled else {0x82b636b8:'BlendState',0x82b636f8:'DepthStencilState',0x82b63738:'RasterizerState'}.get(pc)
   if typ:trace.append(('descriptor',typ,c.mem.rd(c.g(4)&0xffffffff,{'BlendState':0x38,'DepthStencilState':0x4c,'RasterizerState':0x20}[typ]).hex(),snapshot(c)))
   typ=cpp_init.get(pc) if compiled else {0x82b627c8:'BlendState',0x82b62890:'DepthStencilState',0x82b62958:'RasterizerState'}.get(pc)
   if typ:
    n=counts['initialize'];counts['initialize']+=1;trace.append(('initialize',typ,c.mem.rd(c.g(3)&0xffffffff,20).hex(),snapshot(c)))
    if n==5:
     if i%4==1:F.put(c,0x83271600,2)
     elif i%4==2:F.put(c,0x83271600,0)
     if i%3==1:F.put(c,0x83271124,(i*0x1234567)&0xffffffff)
  c.instruction_observer=observe;c.hooks.update({0x82bbd1e0:registry,0x84803000:allocate,branch(0x82b64460):copy,branch(0x82b644f4):copy,branch(0x82b63dc0):copy})
  for at,name in bundle.unresolved.items():
   if name.startswith('?GetDefaultAllocator@'):c.hooks[at]=registry
  c.run(bundle.entry if compiled else 0x82b64108);assert counts==dict(registry=6,allocate=6,initialize=6),counts
  both.append((c.g(3)&0xffffffff,c.mem.rd(0x83271000,0x2000),c.mem.rd(0x82f87b64,28),c.mem.rd(AUX+0x8000,0xc00),trace))
 checks+=1
 if both[0]!=both[1]:errors.append(dict(method='Initialize',case=i,returns=[x[0] for x in both],global_differences=[hex(0x83271000+n) for n,(a,b) in enumerate(zip(both[0][1],both[1][1])) if a!=b],traces=[x[-1] for x in both]))
select('?ResizeDefaultScissorRectAndViewportParameters@StateHelper@')
for i in range(128):
 globals_=rng.randbytes(0x2000);viewport=rng.randbytes(28);both=[]
 for compiled in [False,True]:
  c=F.cpu(bundle);mapped(c);c.mem.wr(0x83271000,globals_);c.mem.wr(0x82f87b64,viewport);c.hooks[branch(0x82b63dc0)]=copy;c.run(bundle.entry if compiled else 0x82b63d30);both.append((c.g(3)&0xffffffff,c.mem.rd(0x83271000,0x2000),c.mem.rd(0x82f87b64,28)))
 checks+=1
 if both[0]!=both[1]:errors.append(dict(method='Resize',case=i,returns=[x[0] for x in both]))
result=dict(cases=checks,errors=errors,objects={n:dict(path=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for n,p in paths.items()},scope='192 whole StateHelper Initialize and128 whole resize cases versus ARTIST. Actual blend/depth/raster/default-descriptor providers execute. All global bytes, allocated states, all five resource lanes, parameter defaults/padding,0/1/3/2 publication order, device-state branch/freshness, SDK surface dimensions, viewport/scissor and returns compare.',limits='Default allocator registry, DoAllocate and pure CRT memcpy are controlled services. Return type of resize is reconstructed as the original32bit destination-address result; known callers ignore it. Shared default-surface storage extent is still owned by Device worker; this helper only reads actual SDK fields.')
(out/'state-helper-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print(checks,'whole StateHelper cases; errors',len(errors));print(errors[:2]);raise SystemExit(bool(errors))
