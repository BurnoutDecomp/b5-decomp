from pathlib import Path
import json,struct,random,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');s=(P/'scratch/parallel-native/native_render_states/validate_hierarchy.py').read_text(encoding='utf8');s=s[:s.index('\nrows=')]+s[s.index('\ndef data_symbol'):s.index('\n# Decode')];__file__=str(P/'scratch/parallel-native/native_render_states/validate_hierarchy.py');exec(compile(s,__file__,'exec'))
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/device-storage-candidate');obj=out/'device.obj';co=Coff(obj,'?Initialize@Device@renderengine@@SA_NXZ');rng=random.Random(0x36061850)
syms=['?gDeviceWord83271604@','?gpD3DDevice@@','?gpDeviceColorBuffer@','?gpDeviceDepthBuffer@','?gpDeviceFrontTexture83271618@','?guTilingEnabled@','?gDeviceTileCount83271620@','?gDeviceTileRects83271628@','?gDeviceFrameCounter83271624@','?gDeviceCounter832716A8@','?gDeviceCounter832716AC@','?gDevicePerformanceCounter832716B0@','?gDeviceWord832716B8@','?gDeviceWord832716BC@','?gOriginalColorBufferStorage83271100@','?gOriginalDepthBufferStorage83271140@','?gDeviceFrontTextureStorage83271580@','?m_parameterCopy@Device@','?m_currentState@Device@']
orig=[0x83271604,0x83271608,0x8327160c,0x83271610,0x83271618,0x8327161c,0x83271620,0x83271628,0x83271624,0x832716a8,0x832716ac,0x832716b0,0x832716b8,0x832716bc,0x83271100,0x83271140,0x83271580,0x82f87b00,0x83271600];sizes=[4,4,4,4,4,4,4,128,4,4,4,8,4,4,64,64,128,100,4]
for case in range(128):
 init=[rng.randbytes(n) for n in sizes];param=rng.randbytes(100);results=[]
 for compiled in (False,True):
  c=co.cpu();calls=[]
  if not compiled:c.mem.add(0x82f87b00,0x100,bytes(0x100));c.mem.add(0x83271000,0x1000,bytes(0x1000));addrs=orig
  else:addrs=[data_symbol(co,x) for x in syms]
  for a,v in zip(addrs,init):c.mem.wr(a,v)
  def norm(v):
   v &=0xffffffff
   for i,a in enumerate(addrs):
    if v==a:return 0x90000000+i*0x100
   return v
  def memset(cpu):
   dst=cpu.g(3)&0xffffffff;val=cpu.g(4)&0xff;size=cpu.g(5)&0xffffffff;calls.append(('memset',norm(dst),val,size));cpu.mem.wr(dst,bytes([val])*size);cpu.setg(3,dst)
  def video(cpu):
   dst=cpu.g(3)&0xffffffff;calls.append(('video',));cpu.mem.wr(dst,struct.pack('>II',1280+case,720+case)+bytes(40))
  c.hooks[0x82c0b180]=memset;c.hooks[0x82c75b7c]=video
  for a,n in co.unresolved.items():
   if n=='memset':c.hooks[a]=memset
   elif n=='XGetVideoMode':c.hooks[a]=video
  c.run(co.entry if compiled else 0x82b61850)
  results.append((calls,[c.mem.rd(a,n) for a,n in zip(addrs,sizes)],c.g(3)&0xffffffff))
 assert results[0]==results[1],(case,results)
report={'state':'pass','cases':128,'object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Full original-vs-compiled Device::Initialize calls, reset extents, output scalars, both surface backings, front backing and parameter bytes; memset and XGetVideoMode are controlled; native Parameters::Initialize executes (inlined in compiled caller). Does not validate tile overflow overlap, D3D, full startup or actual GPU.'};(out/'initialize-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
