from pathlib import Path
import ast,hashlib,json,random,struct,sys
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(parent/'scratch/x360_sdk'));sys.path.insert(0,str(parent/'tools/re'))
import native_atomic_fixture as F
import x360rd as R
out=parent/'b5-decomp/build/x360/parallel/native_textures/renderer-construct10'
def object_in(directory,name):return Path(next(x['object'] for x in json.loads((directory/'compile-results.json').read_text())['results'] if x['passed'] and x['source'].endswith(name)))
p=object_in(out,'BrnRendererModule.cpp');device=object_in(out.parent/'renderer-construct-dependencies1','device.cpp');blobby=object_in(out.parent/'renderer-blobby-construction3','BrnBlobbyShadowManager.cpp')
bundle=F.Bundle([F.Coff(p,'?Construct@BrnRendererModule@@'),F.Coff(device,'?Initialize@Parameters@Device@'),F.Coff(blobby,'?Construct@BrnBlobbyShadowManager@@')]);bundle.select('?Construct@BrnRendererModule@@')
# Extra integer instructions belong to the test interpreter, not production source.
tree=ast.parse((notes:=(parent/'scratch/x360_sdk/parallel-native/native_textures/validate_rendercore_dataflow.py')).read_text());exec(compile(ast.Module(body=[n for n in tree.body if isinstance(n,ast.ClassDef) and n.name=='RenderCPU'],type_ignores=[]),'render-integer-fixture','exec'),globals())
class ConstructCPU(RenderCPU):
 def step(self,pc,word,nxt,depth):
  if word>>26==31 and ((word>>1)&1023) in [8,136]:
   xo=(word>>1)&1023;rd=(word>>21)&31;ra=(word>>16)&31;rb=(word>>11)&31
   a=self.g(ra);b=self.g(rb);total=((~a)&0xffffffffffffffff)+b+(1 if xo==8 else getattr(self,'xer_ca',0));self.xer_ca=int(total>>64!=0);value=total&0xffffffffffffffff;self.setg(rd,value)
   if word&1:self.setcr(0,bool(value>>63),bool(value and not value>>63),value==0)
   return nxt
  return super().step(pc,word,nxt,depth)
globals_map={'gpD3DDevice':0x83271608,'gTextureScopeTable':0x83011A78,'msPostFx':0x82FAF0F0,'mShaderConstantTable':0x830113D0,'NULLResourceHandle':0x82FAFF20,'KU32_SIZEOFDISPATCHBIN_DOUBLEBUFFERED':0x82F24234,'KU32_SIZEOFDISPATCHBIN_SINGLEBUFFERED':0x82F24238}
for (oi,sec),base in bundle.bases.items():
 obj=bundle.objects[oi];body=bundle.images[base]
 for at,index,kind in obj.relocations(sec):
  name,_,_=obj.symbols[index];address=next((a for n,a in globals_map.items() if n in name),None)
  if address is None:continue
  word=struct.unpack_from('>I',body,at)[0]
  if kind in [0x10,0x11]:word=(word&0xffff0000)|(((address+0x8000)>>16 if kind==0x10 else address)&0xffff)
  elif kind==2:word=address
  else:raise AssertionError((name,kind))
  struct.pack_into('>I',body,at,word)
def branch(at):
 w=R.u32(at);off=w&0x03fffffc;return (at+off-(1<<26 if off&(1<<25) else 0))&0xffffffff
SELF=F.M.S.SELF;AUX=F.M.S.AUX;GRAPH=0x82F2C814;rng=random.Random(0x8240A778);errors=[];cases=int(sys.argv[1]) if len(sys.argv)>1 else 64
native={name:branch(at) for name,at in [('effects',0x8240A7C8),('shader',0x8240A7D0),('base',0x8240A800),('video',0x8240A86C),('aspect',0x8240A8B8),('memory',0x8240A8FC),('single',0x8240A940),('states',0x8240A944),('interpreters',0x8240A9A0),('new',0x8240A9A8),('interpreter_ctor',0x8240A9BC),('im2d',0x8240A9D8),('im2d_untex',0x8240A9E4),('im3d',0x8240A9F0),('im3d_untex',0x8240AA00),('sky_renderer',0x8240AA0C),('zonly',0x8240AA1C),('corona',0x8240ACE8),('cpu_monitor',0x8240ADA0),('gpu_monitor',0x8240B43C),('postfx',0x8240B78C),('shader_constant',0x8240B7AC),('shader_array',0x8240B8B8),('debug',0x8240BC94),('depth_texture',0x8240BD38),('texture_descriptor',0x8240BD4C),('texture_init',0x8240BD9C),('texture',0x8240BDCC),('gamma',0x8240BED0),('sun',0x8240BF18),('occlusion',0x8240BF64),('loading',0x8240BF70)]}
for i in range(cases):
 image=rng.randbytes(0xca80);initial=F.cpu(bundle);initial.__class__=ConstructCPU;initial.mem.wr(SELF,image);initial.setg(3,SELF)
 def ctor_service(x):x.setg(3,x.g(3))
 initial.hooks[0x82B43CA0]=ctor_service;initial.hooks[0x82BCB0C0]=ctor_service;initial.run(0x827DFF48);image=initial.mem.rd(SELF,0xca80)
 registry=rng.randbytes(0xc4);monitors=rng.randbytes(0x200);video=bytearray(rng.randbytes(48));struct.pack_into('>I',video,16,[0,1,2,0xffffffff][i%4]);budget0=rng.getrandbits(32);budget1=rng.getrandbits(32);both=[]
 for compiled in [False,True]:
  c=F.cpu(bundle);c.__class__=ConstructCPU;c.mem.add(0x82f24000,0x1000);c.mem.add(0x82f2c000,0x2000);c.mem.add(0x82faf000,0x2000);c.mem.add(0x83011000,0x2000);c.mem.add(0x83271000,0x1000);c.mem.wr(SELF,image);c.mem.wr(0x83011a78,registry);c.mem.wr(AUX+0x4000,monitors);F.put(c,0x82f24234,budget0);F.put(c,0x82f24238,budget1);F.put(c,0x83271608,AUX+0x5000);c.mem.wr(0x82faff20,struct.pack('>II',0xabc00000+i,0xdef00000+i))
  for graph in [GRAPH,AUX+0x1000,AUX+0x1200]:F.put(c,graph,AUX+0x3000)
  F.put(c,AUX+0x3010,0x84803000);sp=c.g(1)&0xffffffff;c.mem.wr(sp-0x1000,bytes([i&255])*0x1000);c.mem.wr(AUX+0x6000,b'\xcd'*32);c.setg(3,SELF);c.setg(4,AUX+0x4000);c.setg(5,AUX+0x6004);c.setg(6,AUX+0x6010);trace=[];depth_calls=[0];texture_calls=[0];allocations=[0];initializations=[0]
  def snapshot():return hashlib.sha256(c.mem.rd(SELF,0xca80)).hexdigest()
  def args(x,n):return [x.g(k)&0xffffffff for k in range(3,3+n)]
  def change_graph(x,point):
   if i%3==1 and point in ['shader','im2d','gpu_monitor','texture_descriptor']:F.put(x,SELF+0x394c,[AUX+0x1000,AUX+0x1200][len(trace)&1])
  def simple(name,n=2):
   def run(x):
    a=args(x,n);trace.append((name,a,snapshot()))
    if name=='effects':x.mem.wr(a[0],bytes([0x31])*16)
    elif name=='shader':x.mem.wr(a[0],bytes([0x42])*0x320)
    elif name=='base':F.put(x,a[0]+8,0x12345678)
    elif name in ['im2d','im2d_untex','im3d','im3d_untex','sky_renderer','zonly']:F.put(x,a[0]+4,0x71000000+len(trace));F.put(x,a[0]+8,0x72000000+len(trace))
    elif name=='debug':x.mem.wr(a[0]+4,bytes([0x63])*20)
    elif name=='occlusion':F.put(x,a[0]+0x33c,AUX+0xf000)
    elif name=='loading':F.put(x,a[0]+0x38,0x73900000)
    change_graph(x,name)
   return run
  def video_mode(x):
   trace.append(('video',));x.mem.wr(x.g(3)&0xffffffff,video)
   if i%5==1:x.mem.wr(SELF+0xc408,struct.pack('>f',0.75))
  def aspect(x):trace.append(('aspect',x.mem.rd(x.g(3)&0xffffffff,36).hex(),snapshot()))
  def memory(x):
   a=args(x,1)[0];params=b''.join((x.g(n)&0xffffffffffffffff).to_bytes(8,'big') for n in range(4,11))+x.mem.rd((x.g(1)&0xffffffff)+0x58,44);trace.append(('memory',a,params.hex(),int.from_bytes(x.mem.rd((x.g(1)&0xffffffff)+0x8c,4),'big'),x.mem.rd((x.g(1)&0xffffffff)+0x97,1).hex(),snapshot()))
   for slot in range(11):F.put(x,a+slot*4,AUX+0x8000+slot*0x100)
   if i%3==1:F.put(x,SELF+0x394c,AUX+0x1200)
  def frame(name):
   def run(x):trace.append((name,args(x,4),snapshot()));F.put(x,x.g(3)+4,0x74700000)
   return run
  def noargs(name):
   def run(x):trace.append((name,snapshot()))
   return run
  def interpreters(x):
   trace.append(('interpreters',args(x,1),snapshot()));x.mem.wr(x.g(3)&0xffffffff,struct.pack('>4I',1,2,3,4));x.setg(3,x.g(3))
  def new(x):trace.append(('new',x.g(3)&0xffffffff));x.setg(3,0 if i%8==0 else AUX+0xb000)
  def interpreter_ctor(x):
   a=args(x,3);trace.append(('interpreter_ctor',a,snapshot()));x.mem.wr(a[0],struct.pack('>4I',a[1],a[2],0,0));x.setg(3,a[0])
  def cpu_monitor(x):
   trace.append(('cpu_monitor',F.cstring(x,x.g(3)&0xffffffff),x.g(4)&0xffffffff,x.g(5)&0xffffffff,struct.pack('>f',x.f(1)).hex(),x.g(7)&0xffffffff,snapshot(),x.mem.rd(SELF,0xca80).hex()));n=sum(t[0]=='cpu_monitor' for t in trace);x.setg(3,0x80000000+n)
  def gpu_monitor(x):
   trace.append(('gpu_monitor',F.cstring(x,x.g(3)&0xffffffff),x.mem.rd(x.g(4)&0xffffffff,4).hex(),struct.pack('>f',x.f(1)).hex(),snapshot()));n=sum(t[0]=='gpu_monitor' for t in trace);x.setg(3,0x90000000+n);change_graph(x,'gpu_monitor')
  def rgba(x):x.mem.wr(x.g(3)&0xffffffff,bytes(x.g(n)&255 for n in range(4,8)));x.setg(3,x.g(3))
  def shader_constant(array):
   def run(x):trace.append(('shader_array' if array else 'shader_constant',x.g(3)&0xffffffff,x.g(4)&0xffffffff,F.cstring(x,x.g(5)&0xffffffff),x.g(6)&255,(x.g(7)&255) if array else None,snapshot()))
   return run
  def depth_texture(x):depth_calls[0]+=1;trace.append(('depth_texture',args(x,1)));x.setg(3,AUX+0xc000+depth_calls[0]*0x20)
  def texture(x):texture_calls[0]+=1;trace.append(('texture',args(x,2)));x.setg(3,AUX+0xd000+texture_calls[0]*0x20)
  def texture_descriptor(x):
   trace.append(('texture_descriptor',snapshot()));x.mem.wr(x.g(3)&0xffffffff,struct.pack('>10I',36,4,0,1,0,1,0,1,0,1));x.setg(3,x.g(3));change_graph(x,'texture_descriptor')
  def allocate(x):
   a=args(x,4);trace.append(('allocate',a[1],x.mem.rd(a[2],40).hex(),a[3],snapshot()));allocations[0]+=1;n=allocations[0];x.mem.wr(a[0],struct.pack('>5I',AUX+0xe000+n*0x100,0x800+n,0x900+n,0xa00+n,0xb00+n));x.setg(3,a[0])
  def texture_init(x):
   a=args(x,2);trace.append(('texture_init',x.mem.rd(a[0],20).hex(),x.mem.rd(a[1],0x4c).hex(),snapshot()));initializations[0]+=1;x.setg(3,AUX+0xe000+initializations[0]*0x80)
  def gamma(x):trace.append(('gamma',x.g(3)&0xffffffff,x.g(4)&0xffffffff,hashlib.sha256(x.mem.rd(x.g(5)&0xffffffff,1536)).hexdigest(),snapshot()))
  def sky(x):
   a=x.g(3)&0xffffffff
   for offset in [0,4,0x18,0x1c,0x30,0x34]:F.put(x,a+offset,0)
  def shadow(x):a=x.g(3)&0xffffffff;F.put(x,a,x.g(4)+1);F.put(x,a+4,x.g(4));x.mem.wr(a+8,b'\1')
  def memcpy(x):a=args(x,3);x.mem.wr(a[0],x.mem.rd(a[1],a[2]));x.setg(3,a[0])
  services={'effects':simple('effects'),'shader':simple('shader',1),'base':simple('base',1),'video':video_mode,'aspect':aspect,'memory':memory,'single':frame('single'),'states':noargs('states'),'interpreters':interpreters,'new':new,'interpreter_ctor':interpreter_ctor,'im2d':simple('im2d'),'im2d_untex':simple('im2d_untex'),'im3d':simple('im3d'),'im3d_untex':simple('im3d_untex'),'sky_renderer':simple('sky_renderer'),'zonly':simple('zonly'),'corona':simple('corona'),'cpu_monitor':cpu_monitor,'gpu_monitor':gpu_monitor,'postfx':simple('postfx'),'shader_constant':shader_constant(False),'shader_array':shader_constant(True),'debug':simple('debug'),'depth_texture':depth_texture,'texture':texture,'texture_descriptor':texture_descriptor,'texture_init':texture_init,'gamma':gamma,'sun':simple('sun'),'occlusion':simple('occlusion',5),'loading':simple('loading',1)}
  for name,at in native.items():c.hooks[at]=services[name]
  c.hooks[branch(0x8240A8D8)]=memcpy
  c.hooks[R.u32(0x820D5174)]=frame('double')
  for name,vt in [('blend',0x820CDC1C),('raster',0x820CDC28),('depth',0x820CDC34)]:c.hooks[R.u32(vt)]=simple(name)
  c.hooks[0x84803000]=allocate;c.hooks[0x823FBDE0]=noargs('report');c.hooks[F.address(bundle,'?MemoryUsage@BrnRendererModule@@')]=noargs('report')
  for at,symbol in bundle.unresolved.items():
   fn=None
   if symbol.startswith('?GetGraphicsAllocator@'):fn=lambda x:x.setg(3,GRAPH)
   elif symbol.startswith('?Construct@EffectsArbitrator@'):fn=services['effects']
   elif symbol.startswith('?Construct@BrnShaderConstantsFrame@'):fn=services['shader']
   elif symbol.startswith('?Construct@ModuleSingleBuffered@'):fn=services['base']
   elif symbol=='XGetVideoMode':fn=video_mode
   elif symbol.startswith('?SetAspectCorrectionMatrix@'):fn=aspect
   elif symbol=='memcpy':fn=memcpy
   elif symbol.startswith('?Construct@BrnRendererMemory@'):fn=memory
   elif symbol.startswith('?Construct@DispatchFrame@'):fn=services['single']
   elif symbol.startswith('?Initialize@StateHelper@'):fn=services['states']
   elif symbol.startswith('?SetupBuiltinInterpreters@'):fn=interpreters
   elif symbol.startswith('??2@'):fn=new
   elif symbol.startswith('??0DispatchPacketInterpreter@'):fn=interpreter_ctor
   elif symbol.startswith('?Construct@Im2d@'):fn=services['im2d']
   elif symbol.startswith('?Construct@Im2dUntex@'):fn=services['im2d_untex']
   elif symbol.startswith('?Construct@Im3d@'):fn=services['im3d']
   elif symbol.startswith('?Construct@Im3dUntex@'):fn=services['im3d_untex']
   elif symbol.startswith('?Construct@Im3dSkyDome@'):fn=services['sky_renderer']
   elif symbol.startswith('?Construct@Im3dZOnly@'):fn=services['zonly']
   elif symbol.startswith('?Construct@BrnCoronaManager@'):fn=services['corona']
   elif symbol.startswith('?AddMonitor@PerfMonCpu@'):fn=cpu_monitor
   elif symbol.startswith('?AddMonitor@PerfMonGpu@'):fn=gpu_monitor
   elif symbol.startswith('??0RGBA@'):fn=rgba
   elif symbol.startswith('?Construct@BrnPostFx@'):fn=services['postfx']
   elif symbol.startswith('?AddShaderConstant@'):fn=services['shader_constant']
   elif symbol.startswith('?AddShaderConstantArray@'):fn=services['shader_array']
   elif symbol.startswith('?Construct@DebugComponent@BrnGraphics@'):fn=services['debug']
   elif symbol.startswith('?GetShadowMapBuffer@'):fn=lambda x:x.setg(3,int.from_bytes(x.mem.rd((x.g(3)&0xffffffff)+4,4),'big'))
   elif symbol.startswith('?GetDepthTexture@'):fn=depth_texture
   elif symbol.startswith('??0BaseResourceDescriptor@'):fn=lambda x:x.mem.wr(x.g(3)&0xffffffff,struct.pack('>II',0,1))
   elif symbol.startswith('?GetResourceDescriptor@TextureState@'):fn=texture_descriptor
   elif symbol.startswith('?Initialize@TextureState@'):fn=texture_init
   elif symbol.startswith('?GetTexture@CgsRenderTarget@'):fn=texture
   elif symbol=='D3DDevice_SetGammaRamp':fn=gamma
   elif symbol.startswith('?Construct@BrnSkyDomeManager@'):fn=sky
   elif symbol.startswith('?Construct@BrnSunCorona@'):fn=services['sun']
   elif symbol.startswith('?Construct@ShadowMapRenderManager@'):fn=shadow
   elif symbol.startswith('?Construct@OcclusionCullManager@'):fn=services['occlusion']
   elif symbol.startswith('?Construct@LoadingScreenRenderer@'):fn=services['loading']
   if fn:c.hooks[at]=fn
  try:c.run(bundle.entry if compiled else 0x8240A778)
  except Exception:
   print('execution failure',i,compiled,hex(c.last_pc),'lasttrace',trace[-2:]);raise
  both.append((c.mem.rd(SELF,0xca80),c.mem.rd(AUX+0x6000,32),c.mem.rd(0x83011a78,0xc4),trace))
 if both[0]!=both[1]:
  errors.append(dict(case=i,different_bytes=[hex(n) for n,(a,b) in enumerate(zip(both[0][0],both[1][0])) if a!=b],output_differences=both[0][1]!=both[1][1],registry_differences=both[0][2]!=both[1][2],first_trace_difference=next((n for n,(a,b) in enumerate(zip(both[0][3],both[1][3])) if a!=b),None),traces=[v[3] for v in both]))
result=dict(cases=cases,errors=errors,objects={str(q):hashlib.sha256(q.read_bytes()).hexdigest() for q in [p,device,blobby]},global_bindings={n:hex(a) for n,a in globals_map.items()},scope='whole actual Renderer::Construct versus ARTIST8240A778:allCA80bytes,aspect/HD outputs,resource ABI/all5lanes,mutablebudgets/allocatorreloads,59monitors,27shaderregistrations,exactgamma and typedregistryeffects;realDeviceParameters,streamresetsandBlobbyCtor execute',limits='foreignsubsystemConstruct,SDK,allocation,texture descriptor/init and render-target access are controlled services;Sky/Shadow constructor field effects are controlled boundaries;notlink/GPU/bootproof')
(out/'renderer-engine-construct-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print(cases,'native Renderer Construct comparisons; errors',len(errors));print([{k:v for k,v in e.items() if k!='traces'} for e in errors[:3]]);raise SystemExit(bool(errors))
