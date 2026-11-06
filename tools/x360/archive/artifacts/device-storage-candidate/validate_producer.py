from pathlib import Path
import json,struct,random,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');s=(P/'scratch/parallel-native/native_render_states/validate_hierarchy.py').read_text(encoding='utf8');s=s[:s.index('\nrows=')]+s[s.index('\ndef data_symbol'):s.index('\n# Decode')];__file__=str(P/'scratch/parallel-native/native_render_states/validate_hierarchy.py');exec(compile(s,__file__,'exec'))
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/device-storage-candidate');obj=out/'device.obj';co=Coff(obj,'?InitializeGlobalBuffers@Device@renderengine');rng=random.Random(36063840)
syms=['?m_parameterCopy@Device@','?gOriginalColorBufferStorage83271100@','?gOriginalDepthBufferStorage83271140@','?gDeviceFrontTextureStorage83271580@','?gpDeviceColorBuffer@','?gpDeviceDepthBuffer@','?gpDeviceFrontTexture83271618@','?gDeviceTileCount83271620@','?gDeviceTileRects83271628@','?guTilingEnabled@','?gDeviceWord832716B8@','?gDeviceWord832716BC@','?gpD3DDevice@@']
origaddr=[0x82f87b00,0x83271100,0x83271140,0x83271580,0x832715c0,0x8327160c,0x83271610,0x83271618,0x83271620,0x83271628,0x8327161c,0x832716b8,0x832716bc,0x83271608]
skipped=[]
passed=0
sizes=[(320,200),(640,480),(720,480),(1280,720),(1366,768),(1920,1080),(2560,1440),(3840,2160),(800,600),(1280,721)]
for case in range(len(sizes)*2*3):
 width,height=sizes[case%len(sizes)];depthOn=(case//len(sizes))%2;samples=(case//(2*len(sizes)))%3;data=bytearray(100);struct.pack_into('>IIIIII',data,0,width,height,0x18280186,width,height,0x18280106);struct.pack_into('>I',data,0x1c,samples);struct.pack_into('>I',data,0x24,0x2a200086);data[0x28]=depthOn;results=[]
 for compiled in (False,True):
  if compiled and struct.unpack('>I',results[0][2])[0] > 8:
   skipped.append(case);break
  c=co.cpu();calls=[];c.mem.add(0x84000000,0x1000,bytes(0x1000));c.mem.wr(0x84000000,struct.pack('>I',0x84000100));c.mem.wr(0x84000110,struct.pack('>I',0x85000000))
  if not compiled:c.mem.add(0x82f87b00,0x100,bytes(0x100));c.mem.add(0x83271000,0x1000,bytes(0x1000));addrs=origaddr
  else:
   addrs=[data_symbol(co,x) for x in syms];addrs.insert(4,addrs[3]+64)
  cp,cs,ds,tex,tex2,coaddr,deaddr,front,count,rect,tiling,b8,bc,device=addrs;c.mem.wr(tex,bytes([0xA5])*128);c.mem.wr(cs,bytes([0xA7])*64);c.mem.wr(ds,bytes([0xB8])*64);c.mem.wr(cp,data);c.mem.wr(device,struct.pack('>I',0x11110000));c.mem.wr(rect,bytes([0xaa])*128)
  def norm(v):
   v&=0xffffffff
   for i,a in enumerate([cs,ds,tex,tex2]):
    if v==a:return 0x90000000+i*0x100
   return v
  def surface(cpu):
   p=cpu.g(3)&0xffffffff;src=list(struct.unpack('>IIIII',cpu.mem.rd(p,20)));calls.append(('surface',[norm(v) for v in src],cpu.mem.rd(cpu.g(4)&0xffffffff,28)));cpu.mem.wr(src[0],bytes([0x42 if src[0]==cs else 0x53])*52);cpu.setg(3,src[0])
  def descriptor(cpu):
   calls.append(('descriptor',cpu.mem.rd(cpu.g(4)&0xffffffff,20)));cpu.mem.wr(cpu.g(3)&0xffffffff,struct.pack('>IIIIIIIIII',52,4,0,1,0x800,4096,0,1,0,1))
  def allocator(cpu):calls.append(('allocator',));cpu.setg(3,0x84000000)
  def allocate(cpu):calls.append(('allocate',cpu.mem.rd(cpu.g(5)&0xffffffff,40),cpu.g(6)&0xffffffff));cpu.mem.wr(cpu.g(3)&0xffffffff,struct.pack('>IIIII',0,0,0x44440000,0,0))
  def texture(cpu):
   src=list(struct.unpack('>IIIII',cpu.mem.rd(cpu.g(3)&0xffffffff,20)));calls.append(('texture',[norm(v) for v in src],cpu.mem.rd(cpu.g(4)&0xffffffff,20)));cpu.mem.wr(src[0],bytes([0x31 if src[0]==tex else 0x72])*52);cpu.setg(3,src[0])
  def setrt(cpu):
   v=struct.unpack('>IIIII',cpu.mem.rd(cpu.g(4)&0xffffffff,20));calls.append(('setrt',cpu.g(3)&0xffffffff,[norm(x) for x in v],cpu.g(5)&0xffffffff))
  handlers=[('?Initialize@PixelBuffer@',0x82b621a8,surface),('?GetResourceDescriptor@Texture2D@',0x823c4cb8,descriptor),('?GetDefaultAllocator@',0x82bbd1e0,allocator),('?Initialize@Texture2D@',0x82b62e78,texture),('D3DDevice_SetSurfaces',0x8293b9f0,setrt)]
  orig=json.load(open('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82B61A48.json',encoding='utf8'))
  for x in orig['xrefs_from']:
   if x['name']=='D3DDevice_SetSurfaces':c.hooks[int(x['address'],16)]=setrt
  for name,addr,fn in handlers:
   if name!='D3DDevice_SetSurfaces':c.hooks[addr]=fn
   for a,n in co.unresolved.items():
    if n.startswith(name):c.hooks[a]=fn
  c.hooks[0x85000000]=allocate
  c.run(co.entry if compiled else 0x82b63840)
  vals=[norm(struct.unpack('>I',c.mem.rd(x,4))[0]) for x in [coaddr,deaddr,front]]
  results.append((calls,vals,*[c.mem.rd(x,n) for x,n in [(count,4),(rect,128),(tiling,4),(b8,4),(bc,4),(cp,100),(tex,128),(cs,64),(ds,64)]]))
 if len(results)==2:
  assert results[0]==results[1],(case,results)
  passed+=1
report={'state':'pass','cases':passed,'excluded_tile_overflow_cases':skipped,'object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Original vs compiled tiling, texture/surface/allocator packets, resulting globals and default bind; SDK size calculation executed, allocation/render services controlled. No hardware or full startup.'};(out/'producer-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
