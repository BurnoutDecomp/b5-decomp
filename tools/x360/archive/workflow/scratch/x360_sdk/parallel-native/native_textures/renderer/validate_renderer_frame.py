from pathlib import Path
import hashlib,json,random,struct,sys
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0,str(parent/'scratch/x360_sdk'));sys.path.insert(0,str(parent/'tools/re'));import native_atomic_fixture as F;import x360rd as R
out=parent/'b5-decomp/build/x360/parallel/native_textures/renderer-frame1'
paths={Path(r['source']).name:Path(r['object']) for r in json.loads((out/'compile-results.json').read_text())['results']}
bundle=F.Bundle([F.Coff(paths['BrnRendererModule.cpp'],'?StartOfFrame@BrnRendererModule@@'),F.Coff(paths['BrnBlobbyShadowManager.cpp'],'?Swap@BrnBlobbyShadowManager@@'),F.Coff(paths['BrnCoronaManager.cpp'],'?Clear@BrnCoronaManager@@')])
SELF=F.M.S.SELF;AUX=F.M.S.AUX;rng=random.Random(0x823fc160);errors=[];checks=0
bases=[0xaec,0x1094,0x1274,0x1554,0x159c,0x1730,0x1778]
def branch(pc):
 w=R.u32(pc);d=w&0x3fffffc
 return (pc+d-(1<<26 if d&(1<<25) else 0))&0xffffffff
def bind_data(addresses):
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
def digest(c):return hashlib.sha256(c.mem.rd(SELF,0xca80)).hexdigest()
bundle.select('?StartOfFrame@BrnRendererModule@@');bind_data({'?mShaderConstantTable@':0x830113d0,'?gTextureScopeTable@':0x83011a78})
for i in range(192):
 image=rng.randbytes(0xca80);aux=rng.randbytes(0x500);scope=rng.randbytes(0xc4);both=[]
 for compiled in [False,True]:
  c=F.cpu(bundle);c.mem.add(0x83011000,0x1000);c.mem.wr(SELF,image);c.mem.wr(AUX+0x8000,aux);c.mem.wr(0x83011a78,scope);F.put(c,SELF+0x2a8,AUX+0x1000);F.put(c,AUX+0x101c,0x84803000);F.put(c,AUX+0x1018,0x84803010);c.mem.wr(SELF+0x3930,bytes([i%2]));c.setg(3,SELF);trace=[]
  for n,b in enumerate(bases):F.put(c,SELF+b+0x20,SELF+b+16*((i+n)%2))
  for n in range(2):F.put(c,SELF+0x3850+n*0x70,AUX+0x8000+n*0x100)
  def frame(x):trace.append(('frame-for-write',x.g(3)&0xffffffff,digest(x)));x.setg(3,AUX+0x6000)
  def reset(x):
   trace.append(('reset',x.g(3)&0xffffffff,digest(x)))
   for n,b in enumerate(bases):
    if (i+n)%3==0:F.put(x,SELF+b+0x20,SELF+b+16*(1-(i+n)%2))
   x.setg(3,0x01234567)
  def bin_(x):trace.append(('bin-for-write',x.g(3)&0xffffffff,digest(x)));x.setg(3,AUX+0x6100)
  def begin(x):
   trace.append(('shader-begin',x.g(3)&0xffffffff,x.g(4)&0xffffffff,digest(x)))
   if i%3==1:x.mem.wr(SELF+0x3930,bytes([1-i%2]))
   for n in range(7):F.put(x,0x83011a78+n*0x1c+0x14,0xf1230000+i+n);F.put(x,0x83011a78+n*0x1c+0x18,0x81230000+i+n)
  c.hooks.update({0x84803000:frame,0x84803010:bin_,branch(0x823fc194):reset,branch(0x823fc2cc):begin})
  for at,name in bundle.unresolved.items():
   if name.startswith('?Reset@DispatchFrame@'):c.hooks[at]=reset
   elif name.startswith('?BeginFrame@ShaderConstantTable@'):c.hooks[at]=begin
  c.run(bundle.entry if compiled else 0x823fc160)
  both.append((c.mem.rd(SELF,0xca80),c.mem.rd(AUX+0x8000,0x500),c.mem.rd(0x83011a78,0xc4),trace))
 checks+=1
 if both[0]!=both[1]:errors.append(dict(method='StartOfFrame',case=i,byte_differences=[hex(n) for n,(a,b) in enumerate(zip(both[0][0],both[1][0])) if a!=b],traces=[x[-1] for x in both]))
bundle.select('?SwapBuffers@BrnRendererModule@@')
compiled_swaps={F.address(bundle,name):tag for name,tag in [('?Swap@?$ImRenderBuffer@UBasic2dColouredTexturedVertex','2d'),('?Swap@?$ImRenderBuffer@UBasicColouredTexturedVertex','3d'),('?Swap@?$ImRenderBuffer@UBasicColouredVertex','untex')]}
compiled_blobby=F.address(bundle,'?Swap@BrnBlobbyShadowManager@@');compiled_corona=F.address(bundle,'?Swap@BrnCoronaManager@@')
for i in range(192):
 image=bytearray(rng.randbytes(0xca80));image[0xad1]=i%2;image[0x37e1]=(i//2)%2;both=[]
 for compiled in [False,True]:
  c=F.cpu(bundle);c.mem.wr(SELF,image);F.put(c,SELF+0x2a8,AUX+0x1000);F.put(c,AUX+0x1010,0x84803000);c.setg(3,SELF);trace=[]
  for n,b in enumerate(bases):F.put(c,SELF+b+0x28,(i+n)%2)
  def ring(x):
   trace.append(('ring-swap',x.g(3)&0xffffffff,digest(x)))
   if i%3==1:x.mem.wr(SELF+0xad1,bytes([1-i%2]))
  def effects(x):
   trace.append(('effects-end',x.g(3)&0xffffffff,digest(x)))
   if i%5==1:x.mem.wr(SELF+0xad1,bytes([i%2]))
  def construct(x):
   address=x.g(3)&0xffffffff;trace.append(('frame-construct',address,digest(x)));x.mem.wr(address,bytes([(i+17)&255])*0x320)
   if i%4==1:x.mem.wr(SELF+0xad0,bytes([0,0]))
   if i%4==2:x.mem.wr(SELF+0xad0,bytes([1,0]))
   for n,b in enumerate(bases):
    if (i+n)%3==0:F.put(x,SELF+b+0x28,1-(i+n)%2)
  def observe(pc,word):
   tag=compiled_swaps.get(pc) if compiled else {0x823f9850:'2d',0x823f9470:'3d',0x823f9558:'untex'}.get(pc)
   if tag:trace.append(('buffer-swap',tag,c.g(3)&0xffffffff,digest(c)))
   if pc==(compiled_blobby if compiled else 0x823fc748):trace.append(('blobby-swap',SELF+0x17c0,digest(c)))
   if pc==(compiled_corona if compiled else 0x823fc774):trace.append(('corona-swap',SELF+0x3800,digest(c)))
  c.instruction_observer=observe
  c.hooks.update({0x84803000:ring,branch(0x823fc6b8):effects,branch(0x823fc6e4):construct})
  for at,name in bundle.unresolved.items():
   if name.startswith('?EndOfFrame@EffectsArbitrator@'):c.hooks[at]=effects
   elif name.startswith('?Construct@BrnShaderConstantsFrame@@'):c.hooks[at]=construct
  c.run(bundle.entry if compiled else 0x823fc678);both.append((c.mem.rd(SELF,0xca80),trace))
 checks+=1
 if both[0]!=both[1]:errors.append(dict(method='SwapBuffers',case=i,byte_differences=[hex(n) for n,(a,b) in enumerate(zip(both[0][0],both[1][0])) if a!=b],traces=[x[-1] for x in both]))
assert branch(0x823fc2a4)==branch(0x823fc6ac)==branch(0x823fc6b0)==0x8284cb38 and R.u32(0x8284cb38)==0x4e800020
result=dict(cases=checks,errors=errors,objects={n:dict(path=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for n,p in paths.items()},scope='192 whole StartOfFrame and192 whole SwapBuffers cases versus ARTIST. Actual seven Clear/Swap bodies, TextureScope cache reset, Blobby and Corona helpers execute. All CA80 bytes, registry bytes, cursor/index/flag/count stores, helper entry order, and late external service mutations compare.',limits='Buffered ring accessor/Swap, DispatchFrame Reset, ShaderConstantTable BeginFrame, EffectsArbitrator EndOfFrame and BrnShaderConstantsFrame Construct are controlled foreign service boundaries. No link, boot, GPU or complete frame claim.',icf_noop=dict(address='8284CB38',instruction='4E800020',call_sites=['823FC2A4','823FC6AC','823FC6B0']))
(out/'renderer-frame-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print(checks,'whole Renderer frame cases; errors',len(errors));print(errors[:2]);raise SystemExit(bool(errors))
