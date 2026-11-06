from pathlib import Path
import json,struct,random,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');s=(P/'scratch/parallel-native/native_render_states/validate_hierarchy.py').read_text(encoding='utf8');s=s[:s.index('\nrows=')]+s[s.index('\ndef data_symbol'):s.index('\n# Decode')];__file__=str(P/'scratch/parallel-native/native_render_states/validate_hierarchy.py');exec(compile(s,__file__,'exec'))
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/device-storage-candidate');obj=out/'device.obj';co=Coff(obj,'?ShowPixelBuffer@Device@renderengine');rng=random.Random(0x36063298)
syms=['?gDeviceFrontTextureStorage83271580@','?gpDeviceFrontTexture83271618@','?gDeviceWord832716B8@','?gDeviceWord832716BC@','?gpD3DDevice@@','?guTilingEnabled@','?gDeviceCounter832716A8@','?gDeviceCounter832716AC@','?m_parameterCopy@Device@']
orig=[0x83271580,0x83271618,0x832716b8,0x832716bc,0x83271608,0x8327161c,0x832716a8,0x832716ac,0x82f87b00]
for case in range(192):
 count=[1,2,3][case%3];index=(case//3)%5;randomdata=rng.randbytes(128);param=rng.randbytes(100);old=rng.getrandbits(32);fence=rng.getrandbits(32);results=[]
 for compiled in (False,True):
  c=co.cpu();calls=[]
  if not compiled:
   c.mem.add(0x82f87b00,0x100,bytes(0x100));c.mem.add(0x83271000,0x1000,bytes(0x1000));addrs=orig
  else:addrs=[data_symbol(co,x) for x in syms]
  storage,front,idx,bc,dev,tiling,a8,ac,params=addrs
  def w(a,v):c.mem.wr(a,struct.pack('>I',v&0xffffffff))
  def r(a):return struct.unpack('>I',c.mem.rd(a,4))[0]
  for a,v in [(front,0),(idx,index),(bc,count),(dev,0x11110000),(tiling,0x1234),(a8,old),(ac,0x7777)]:w(a,v)
  c.mem.wr(storage,randomdata);c.mem.wr(params,param);c.setg(3,M.S.SELF)
  def norm(v):
   v &= 0xffffffff
   if storage<=v<storage+512:return 0x90000000+(v-storage)
   if params<=v<params+100:return 0x91000000+(v-params)
   return v
  def sync(cpu):
   calls.append(('sync',cpu.g(3)&0xffffffff,norm(r(front)),r(idx),r(bc),r(tiling),r(a8),r(ac)))
   if case & 16:w(front,storage+64)
  def resolve(cpu):
   calls.append(('resolve',cpu.g(3)&0xffffffff,norm(cpu.g(4)),*[cpu.g(i)&0xffffffff for i in range(5,11)],struct.pack('>d',cpu.f(1)),r((cpu.g(1)&0xffffffff)+0x5c)))
   if case & 32:w(idx,3);w(bc,2);w(a8,0x76543210)
  def insert(cpu):
   calls.append(('fence',cpu.g(3)&0xffffffff,r(idx),r(tiling),r(a8),r(ac)));cpu.setg(3,fence)
   if case & 64:w(bc,1)
  def swap(cpu):calls.append(('swap',cpu.g(3)&0xffffffff,norm(cpu.g(4)),norm(cpu.g(5)),cpu.mem.rd(cpu.g(5)&0xffffffff,28),r(idx),r(bc),r(tiling),r(a8),r(ac)))
  for addr,fn in [(0x82952500,sync),(0x82b62300,resolve),(0x82946bf8,insert),(0x82952540,swap)]:c.hooks[addr]=fn
  for a,n in co.unresolved.items():
   if n=='D3DDevice_SynchronizeToPresentationInterval':c.hooks[a]=sync
   elif n.startswith('?Xbox2ResolveTo@'):c.hooks[a]=resolve
   elif n=='D3DDevice_InsertFence':c.hooks[a]=insert
   elif n=='D3DDevice_Swap':c.hooks[a]=swap
  c.run(co.entry if compiled else 0x82b63298)
  results.append((calls,norm(r(front)),r(idx),r(bc),r(tiling),r(a8),r(ac),c.mem.rd(storage,128),c.mem.rd(params,100)))
 assert results[0]==results[1],(case,results)
report={'state':'pass','cases':192,'object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Original and actual compiled Show routine; compares full SDK call order and resolve arguments, fence/index/tiling/front globals, storage and scaler bytes. Counts 1..3, indices 0..4; resolve, fence, sync, swap are controlled and do not dereference invalid indexes. Controlled sync/resolve/fence also mutate front pointer, index/count and prior fence to check reloads. Zero-count trap not executed; no GPU, link or boot claim.'};(out/'show-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
