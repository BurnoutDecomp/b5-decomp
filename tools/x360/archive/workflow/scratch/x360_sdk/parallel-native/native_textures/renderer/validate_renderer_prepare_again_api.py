from pathlib import Path
import hashlib,json,random,struct,sys
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(parent/'scratch/x360_sdk'));import native_atomic_fixture as F
out=parent/'b5-decomp/build/x360/parallel/native_textures/renderer-prepare-again-api1'
paths={Path(r['source']).name:Path(r['object']) for r in json.loads((out/'compile-results.json').read_text())['results']}
bundle=F.Bundle([F.Coff(paths['BrnRendererModule.cpp'],'?PrepareAgain@BrnRendererModule@@'),F.Coff(paths['BrnCoronaManager.cpp'],'?SetTextureAtlas@BrnCoronaManager@@')]);bundle.select('?PrepareAgain@BrnRendererModule@@')
for (oi,sec),base in bundle.bases.items():
 obj=bundle.objects[oi];body=bundle.images[base]
 for at,index,kind in obj.relocations(sec):
  name,_,_=obj.symbols[index];address=0x83011a78 if name.startswith('?gTextureScopeTable@') else (0x82fafc10 if 's_atlasUVs@BrnCoronaManager@@' in name else None)
  if address is None:continue
  w=struct.unpack_from('>I',body,at)[0]
  if kind in [0x10,0x11]:w=(w&0xffff0000)|(((address+0x8000)>>16 if kind==0x10 else address)&0xffff)
  elif kind==2:w=address
  else:raise AssertionError((name,kind))
  struct.pack_into('>I',body,at,w)
SELF=F.M.S.SELF;AUX=F.M.S.AUX;rng=random.Random(0x823ff8f8);errors=[]
for i in range(192):
 image=bytearray(rng.randbytes(0xca80));scope=bytearray(rng.randbytes(0xc4));atlasglobals=rng.randbytes(0x40);texture_inputs=[0 if (i+n)%11==0 else AUX+0x9000+n*0x100+i*4 for n in range(5)];state_returns=[0 if i%7==0 else AUX+0xa000+i*8,0 if i%5==0 else AUX+0xb000+i*8]
 for off,value in [(0x60,texture_inputs[4] if i%2 else 0x87654321),(0x64,state_returns[1] if i%3 else 0x12345678),(0x98,texture_inputs[4] if i%4 else 0xabc12345)]:struct.pack_into('>I',scope,off,value)
 both=[]
 for compiled in [False,True]:
  c=F.cpu(bundle);c.mem.add(0x83011000,0x1000);c.mem.add(0x82fab000,0x1000);c.mem.add(0x82f2c000,0x1000);c.mem.wr(SELF,image);c.mem.wr(0x83011a78,scope);c.mem.wr(0x82fab690,atlasglobals);F.put(c,SELF+0x3808,0 if i%2 else 0x12345678)
  for allocator in [0x82f2c814,AUX+0x1000,AUX+0x1200]:F.put(c,allocator,AUX+0x2000)
  F.put(c,AUX+0x2010,0x84803000);F.put(c,AUX+0x2014,0x84803010);F.put(c,SELF+0x394c,0x82f2c814 if i%3==0 else AUX+0x1000)
  sp=c.g(1)&0xffffffff;c.mem.wr(sp-0x2000,bytes([i])*0x2000);c.setg(3,SELF)
  for n,value in enumerate(texture_inputs):c.setg(n+4,value)
  trace=[];counts={'desc':0,'alloc':0,'init':0}
  def snapshot():return hashlib.sha256(c.mem.rd(SELF,0xca80)).hexdigest()
  def free(x):trace.append(('free',x.g(3)&0xffffffff,x.g(4)&0xffffffff,x.mem.rd(x.g(4)&0xffffffff,20).hex(),snapshot()))
  def descriptor(x):
   counts['desc']+=1;trace.append(('descriptor',counts['desc'],snapshot()));x.mem.wr(x.g(3)&0xffffffff,struct.pack('>10I',36,4,0,1,0,1,0,1,0,1))
   if counts['desc']==2:
    if i%4==1:F.put(x,SELF+0x394c,AUX+0x1200)
    if i%4==2:F.put(x,SELF+0x16b0+0x14,0xa1230000+i);F.put(x,SELF+0x16f8,0xb1230000+i)
  def allocate(x):
   counts['alloc']+=1;trace.append(('allocate',counts['alloc'],x.g(4)&0xffffffff,x.mem.rd(x.g(5)&0xffffffff,40).hex(),x.g(6)&0xffffffff,snapshot()));x.mem.wr(x.g(3)&0xffffffff,struct.pack('>5I',*(0xc0000000+i*16+counts['alloc']*8+j for j in range(5))))
  def initialize(x):
   n=counts['init'];counts['init']+=1;trace.append(('initialize',n,x.mem.rd(x.g(3)&0xffffffff,20).hex(),x.mem.rd(x.g(4)&0xffffffff,76).hex(),snapshot()))
   if n==1:
    if i%4==3:F.put(x,SELF+0x16a8,0xd1230000+i)
    if i%6==1:
     for purpose in [3,5]:F.put(x,0x83011a78+purpose*0x1c+0x14,0x91230000+i);F.put(x,0x83011a78+purpose*0x1c+0x18,0xa1230000+i)
   x.setg(3,state_returns[n])
  def publish(x):
   values=[x.g(n)&0xffffffff for n in range(3,6)];F.put(x,0x82fab6bc,values[0]);F.put(x,0x82fab6a8,values[1]);F.put(x,0x82fab6b8,values[2]);trace.append(('corona-publish',values,snapshot()))
  def observe(pc,word):
   if not compiled and pc==0x823ff944:trace.append(('corona-publish',[int.from_bytes(c.mem.rd(a,4),'big') for a in [0x82fab6bc,0x82fab6a8,0x82fab6b8]],snapshot()))
  c.instruction_observer=observe;c.hooks.update({0x84803000:allocate,0x84803010:free,0x82b635c8:descriptor,0x82b62720:initialize})
  for at,name in bundle.unresolved.items():
   if name.startswith('?GetGraphicsAllocator@'):c.hooks[at]=lambda x:x.setg(3,0x82f2c814)
   elif name.startswith('?GetResourceDescriptor@TextureState@'):c.hooks[at]=descriptor
   elif name.startswith('?Initialize@TextureState@'):c.hooks[at]=initialize
   elif name.startswith('?SetTextureAtlas@CoronaRenderer@'):c.hooks[at]=publish
   elif name.startswith('??0BaseResourceDescriptor@'):c.hooks[at]=lambda x:x.mem.wr(x.g(3)&0xffffffff,struct.pack('>II',0,1))
  c.run(bundle.entry if compiled else 0x823ff8f8);assert counts==dict(desc=2,alloc=2,init=2),counts
  both.append((c.g(3)&0xffffffff,c.mem.rd(SELF,0xca80),c.mem.rd(0x83011a78,0xc4),c.mem.rd(0x82fab690,0x40),trace))
 if both[0]!=both[1]:errors.append(dict(case=i,returns=[x[0] for x in both],byte_differences=[hex(n) for n,(a,b) in enumerate(zip(both[0][1],both[1][1])) if a!=b],scope_differences=[hex(n) for n,(a,b) in enumerate(zip(both[0][2],both[1][2])) if a!=b],traces=[x[-1] for x in both]))
result=dict(cases=192,errors=errors,objects={n:dict(path=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for n,p in paths.items()},scope='Whole actual bool PrepareAgain(const Texture* x5) versus ARTIST823FF8F8 with real Corona atlas body linked. All CA80 Renderer bytes, complete texture-purpose registry and atlas globals, two five-lane allocations and sampler parameter blocks, incoming-versus-live glass pointer distinction, late allocator/parameter/member mutations and return ABI compare.',limits='Allocator, TextureState descriptor/Initialize and existing three-store CoronaRenderer atlas publisher are controlled; the pure genuine graphics-allocator getter supplies the native global address. No full link, boot or GPU claim.')
assert not (out/'renderer-prepare-again-comparison.json').exists()
(out/'renderer-prepare-again-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print('192 whole PrepareAgain cases; errors',len(errors));print(errors[:2]);raise SystemExit(bool(errors))
