from pathlib import Path
import hashlib,json,random,struct,sys
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(parent/'scratch/x360_sdk'));import native_atomic_fixture as F
out=parent/'b5-decomp/build/x360/parallel/native_textures/renderer-backbuffer1';r=json.loads((out/'compile-results.json').read_text())['results'][0];p=Path(r['object']);bundle=F.Bundle([F.Coff(p,'?CreateBackBuffer@BrnRendererMemory@@')]);bundle.select('?CreateBackBuffer@BrnRendererMemory@@')
for (oi,sec),base in bundle.bases.items():
 obj=bundle.objects[oi];body=bundle.images[base]
 for at,index,kind in obj.relocations(sec):
  name,_,_=obj.symbols[index];word=struct.unpack_from('>I',body,at)[0]
  if name.startswith('??0CgsRenderTarget@@'):
   assert kind==6;delta=0x823F47A0-(base+at);word=(word&0xfc000003)|(delta&0x03fffffc)
  elif name.startswith('?m_renderTargetState@Device@renderengine@@'):
   address=0x83271614
   if kind in [0x10,0x11]:word=(word&0xffff0000)|(((address+0x8000)>>16 if kind==0x10 else address)&0xffff)
   elif kind==2:word=address
   else:raise AssertionError((name,kind))
  else:continue
  struct.pack_into('>I',body,at,word)
SELF=F.M.S.SELF;AUX=F.M.S.AUX;rng=random.Random(0x823F6F78);errors=[]
class AssertStop(Exception):pass
for i in range(128):
 state=bytearray(rng.randbytes(0x70));back=rng.randbytes(0x110);back_native=rng.randbytes(0x98);down_native=rng.randbytes(0x98);other_native=rng.randbytes(0x98);width=[0,1,31,640,1280,1920,0xffffffff][i%7];height=[0,1,15,480,720,1080,0xffffffff][(i//2)%7];default=rng.getrandbits(32) if i%7 else 0;down=0 if i%11==0 else AUX+0x5000;struct.pack_into('>I',state,0x10,down);both=[]
 for compiled in [False,True]:
  c=F.cpu(bundle);c.mem.add(0x83271000,0x1000);c.mem.wr(SELF,state);c.mem.wr(AUX+0x4000,back);c.mem.wr(AUX+0x6000,back_native);c.mem.wr(AUX+0x7000,down_native);c.mem.wr(AUX+0x7400,other_native);F.put(c,AUX+0x5108,AUX+0x7000);F.put(c,AUX+0x5308,AUX+0x7400);F.put(c,0x83271614,default);trace=[]
  def allocate(x):trace.append(('new',x.g(3)&0xffffffff));x.setg(3,AUX+0x4000)
  def construct(x):
   trace.append(('construct',x.g(3)&0xffffffff,x.g(4)&0xffffffff,x.mem.rd(AUX+0x4000,0x110).hex(),x.mem.rd(SELF,0x70).hex()));F.put(x,AUX+0x4108,AUX+0x6000)
   if i%5==1:F.put(x,SELF+0x10,AUX+0x5200)
   if i%3==1:F.put(x,0x83271614,default^0x12345678)
  def begin(x):trace.append(('assert_begin',))
  def fire(x):trace.append(('assert',F.cstring(x,x.g(3)&0xffffffff),F.cstring(x,x.g(4)&0xffffffff),x.g(5)&0xffffffff))
  def end(x):trace.append(('assert_end',));raise AssertStop()
  c.hooks.update({0x82C08DF0:allocate,0x827ECC28:construct,0x82817548:begin,0x82820810:fire,0x82817558:end})
  for at,name in bundle.unresolved.items():
   if name.startswith('??2@'):c.hooks[at]=allocate
   elif name.startswith('?Construct@CgsRenderTarget@@'):c.hooks[at]=construct
   elif name.startswith('?BeginAssert@'):c.hooks[at]=begin
   elif name.startswith('?FireAssert@'):c.hooks[at]=fire
   elif name.startswith('?EndAssert@'):c.hooks[at]=end
  c.setg(3,SELF);c.setg(4,AUX+0x1000);c.setg(5,width);c.setg(6,height);stopped=False
  try:c.run(bundle.entry if compiled else 0x823F6F78)
  except AssertStop:stopped=True
  both.append((stopped,c.mem.rd(SELF,0x70),c.mem.rd(AUX+0x4000,0x110),c.mem.rd(AUX+0x6000,0x98),c.mem.rd(AUX+0x7000,0x98),c.mem.rd(AUX+0x7400,0x98),trace))
 if both[0]!=both[1]:errors.append(dict(case=i,parent=[hex(n) for n,(a,b) in enumerate(zip(both[0][1],both[1][1])) if a!=b],cgs=[hex(n) for n,(a,b) in enumerate(zip(both[0][2],both[1][2])) if a!=b],backend=[hex(n) for n,(a,b) in enumerate(zip(both[0][3],both[1][3])) if a!=b],traces=[x[-1] for x in both]))
result=dict(cases=128,errors=errors,object=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),scope='whole actual Memory::CreateBackBuffer against ARTIST823F6F78:realCgsRenderTargetC++ctor,112parentbytes,272facadebytes,all3x152backendbytes,constructorarguments,latechangeddownsample/defaultstatepointers,assertrecordandstoretiming',alias='backend+2C is maColourTargets[0].mpTextureState,notsection4ordepthsurface;backend+1C copies genuineDevice::m_renderTargetState at83271614',limits='operatornewandresource realizationConstructcontrolled;physicalsurfaces/GPUanddefaultDeviceinitializationarenotproved')
(out/'backbuffer-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print('128 native BackBuffer factory cases; errors',len(errors));print(errors[:2]);raise SystemExit(bool(errors))
