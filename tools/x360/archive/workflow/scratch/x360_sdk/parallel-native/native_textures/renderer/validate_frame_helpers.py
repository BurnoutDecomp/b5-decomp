from pathlib import Path
import hashlib,json,random,struct,sys
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0,str(parent/'scratch/x360_sdk'));import native_atomic_fixture as F
out=parent/'b5-decomp/build/x360/parallel/native_textures/renderer-frame1'
paths={Path(r['source']).name:Path(r['object']) for r in json.loads((out/'compile-results.json').read_text())['results']}
SELF=F.M.S.SELF;AUX=F.M.S.AUX;rng=random.Random(0x823fc678);errors=[];checks=0
class Stop(Exception):pass
def single(path,prefix):
 o=F.Coff(path,prefix);b=F.Bundle([o])
 for name,value,sec in o.symbols.values():
  if sec>0 and name==prefix:b.definitions[name]=(0,value,sec)
 b.select(prefix);return b
def bind_data(obj,addresses):
 for (oi,sec),base in obj.bases.items():
  body=obj.images[base]
  for at,index,kind in obj.objects[oi].relocations(sec):
   name,_,_=obj.objects[oi].symbols[index]
   matches=[a for n,a in addresses.items() if name.startswith(n)]
   if not matches:continue
   address=matches[0];w=struct.unpack_from('>I',body,at)[0]
   if kind in [0x10,0x11]:w=(w&0xffff0000)|(((address+0x8000)>>16 if kind==0x10 else address)&0xffff)
   elif kind==2:w=address
   else:raise AssertionError((name,kind))
   struct.pack_into('>I',body,at,w)
for name,original in [('Swap2',0x823f9850),('Swap3',0x823f9470),('SwapU',0x823f9558)]:
 obj=single(paths['native_renderer_frame_helpers.cpp'],name)
 bind_data(obj,{'?gpcMessageBuffer@':0x82f32260,'?giMessageBufferSize@':0x82f32264})
 for i in range(256):
  image=bytearray(rng.randbytes(0x44));initial=[0,1,2,3,0xffffffff,0x80000000,0x7fffffff,0xfffffffe][i%8];struct.pack_into('>I',image,0x28,initial);both=[]
  for compiled in [False,True]:
   c=F.cpu(obj);c.mem.add(0x82f32000,0x1000);F.put(c,0x82f32260,AUX+0x18000);F.put(c,0x82f32264,256);c.mem.wr(AUX+0x18000,b'X'*256);c.mem.wr(SELF,image);c.setg(3,SELF);trace=[]
   def begin(x):
    trace.append(('begin',))
    if i%3==1:F.put(x,SELF+0x28,0x87654321)
   def base_ctor(x):F.put(x,(x.g(3)+4)&0xffffffff,0)
   def ctor(x):
    a=x.g(3)&0xffffffff;F.put(x,a,AUX+0x17000);F.put(x,AUX+0x17004,0x84803000);F.put(x,a+4,0);F.put(x,a+8,x.g(4));F.put(x,a+12,x.g(5));x.mem.wr(x.g(4)&0xffffffff,b'\0')
   def append(x):
    a=x.g(3)&0xffffffff;text=F.cstring(x,x.g(4)&0xffffffff);dst=int.from_bytes(x.mem.rd(a+8,4),'big');before=F.cstring(x,dst);x.mem.wr(dst,(before+text).encode()+b'\0');trace.append(('append',text))
    if len(trace)==2 and i%3==2:F.put(x,SELF+0x28,0x12345678)
    x.setg(3,a)
   def integer(x):
    a=x.g(3)&0xffffffff;v=x.g(4)&0xffffffff;signed=(v&0x7fffffff)-(v&0x80000000);dst=int.from_bytes(x.mem.rd(a+8,4),'big');before=F.cstring(x,dst);x.mem.wr(dst,(before+str(signed)).encode()+b'\0');trace.append(('integer',signed));x.setg(3,a)
   def fire(x):trace.append(('assert',F.cstring(x,x.g(3)&0xffffffff),F.cstring(x,x.g(4)&0xffffffff),x.g(5)&0xffffffff))
   def end(x):
    trace.append(('end',))
    if i%2:F.put(x,SELF+0x28,(i//2)%2)
   c.hooks.update({0x82817548:begin,0x82820810:fire,0x82817558:end,0x82815e58:base_ctor,0x821f01a8:append,0x821f0e50:integer,0x84803000:append})
   for at,symbol in obj.unresolved.items():
    if symbol.startswith('?BeginAssert@'):c.hooks[at]=begin
    elif symbol.startswith('?FireAssert@'):c.hooks[at]=fire
    elif symbol.startswith('?EndAssert@'):c.hooks[at]=end
    elif symbol.startswith('??0StrStream@'):c.hooks[at]=ctor
    elif symbol.startswith('??6StrStreamBase@'):c.hooks[at]=integer
   c.run(obj.entry if compiled else original)
   both.append((c.mem.rd(SELF,0x44),c.mem.rd(AUX+0x18000,256),trace))
  checks+=1
  if both[0]!=both[1]:errors.append(dict(helper=name,case=i,initial=hex(initial),bytes=[hex(n) for n,(a,b) in enumerate(zip(both[0][0],both[1][0])) if a!=b],trace=[x[-1] for x in both]))
obj=single(paths['native_renderer_frame_helpers.cpp'],'BeginTextureFrame')
for i in range(128):
 image=rng.randbytes(0xc4);both=[]
 for compiled in [False,True]:
  c=F.cpu(obj);c.mem.add(0x83011000,0x1000);c.mem.wr(0x83011a78,image);c.setg(3,0x83011a78);c.setg(31,0)
  def observe(pc,word):
   if pc==0x823fc2f4:raise Stop()
  c.instruction_observer=observe
  try:c.run(obj.entry if compiled else 0x823fc2d0)
  except Stop:pass
  both.append(c.mem.rd(0x83011a78,0xc4))
 checks+=1
 if both[0]!=both[1]:errors.append(dict(helper='TextureScopeTable::BeginFrame',case=i))
obj=single(paths['BrnBlobbyShadowManager.cpp'],'?Swap@BrnBlobbyShadowManager@@')
for i in range(128):
 image=bytearray(rng.randbytes(0x2030));image[0x2021]=i%2;both=[]
 for compiled in [False,True]:
  c=F.cpu(obj);c.mem.wr(SELF,image);c.setg(3,SELF);c.setg(31,SELF-0x17c0);c.setg(30,0)
  def observe(pc,word):
   if pc==0x823fc774:raise Stop()
  c.instruction_observer=observe
  try:c.run(obj.entry if compiled else 0x823fc748)
  except Stop:pass
  both.append(c.mem.rd(SELF,0x2030))
 checks+=1
 if both[0]!=both[1]:errors.append(dict(helper='BlobbyShadowManager::Swap',case=i))
result=dict(cases=checks,errors=errors,objects={n:dict(path=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for n,p in paths.items()},scope='Actual header Swap bodies for all three vertex types:768 complete native cases including invalid indices, diagnostic string/file/line and late index mutations;128 texture BeginFrame cases preserving every cache+18;128 complete Blobby Swap storage cases.',limits='Assert and text stream operations are controlled services. Texture BeginFrame and Blobby Swap compare actual inlined ARTIST instruction regions. Buffer Clear is covered by the whole Renderer StartOfFrame fixture.')
(out/'frame-helper-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print(checks,'native helper cases; errors',len(errors));print(errors[:3]);raise SystemExit(bool(errors))
