from pathlib import Path
import hashlib,json,random,struct,sys
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(parent/'scratch/x360_sdk'));import native_atomic_fixture as F
out=parent/'b5-decomp/build/x360/parallel/native_textures/sampler-callers3';p=Path(next(x['object'] for x in json.loads((out/'compile-results.json').read_text())['results'] if x['source'].endswith('BrnCoronaManager.cpp')));bundle=F.Bundle([F.Coff(p,'?SetTextureAtlas@BrnCoronaManager@@')]);bundle.select('?SetTextureAtlas@BrnCoronaManager@@');SELF=F.M.S.SELF;AUX=F.M.S.AUX;errors=[];rng=random.Random(0x823FD000)
for (oi,sec),base in bundle.bases.items():
 obj=bundle.objects[oi];body=bundle.images[base]
 for at,index,kind in obj.relocations(sec):
  name,_,_=obj.symbols[index]
  if 's_atlasUVs@BrnCoronaManager@@' not in name:continue
  address=0x82FAFC10;w=struct.unpack_from('>I',body,at)[0]
  if kind in [0x10,0x11]:w=(w&0xffff0000)|(((address+0x8000)>>16 if kind==0x10 else address)&0xffff)
  elif kind==2:w=address
  else:raise AssertionError((name,kind))
  struct.pack_into('>I',body,at,w)
for i in range(128):
 image=bytearray(rng.randbytes(0x140));struct.pack_into('>I',image,8,0 if i%2==0 else AUX+0x5000);globals_=rng.randbytes(0x40);both=[]
 for compiled in [False,True]:
  c=F.cpu(bundle);c.mem.add(0x82fab000,0x1000);c.mem.wr(SELF,image);c.mem.wr(0x82fab690,globals_);F.put(c,AUX+0x1000,AUX+0x1100);F.put(c,AUX+0x1110,0x84803000);F.put(c,AUX+0x1114,0x84803010);sp=c.g(1)&0xffffffff;c.mem.wr(sp-0x1000,bytes([i])*0x1000);c.setg(3,SELF);c.setg(4,AUX+0x1000);c.setg(5,AUX+0x6000+i*4);trace=[]
  def free(x):trace.append(('free',x.g(3)&0xffffffff,x.g(4)&0xffffffff,x.mem.rd(x.g(4)&0xffffffff,20).hex(),x.mem.rd(SELF,0x140).hex()))
  def descriptor(x):trace.append(('descriptor',));x.mem.wr(x.g(3)&0xffffffff,struct.pack('>10I',36,4,0,1,0,1,0,1,0,1));x.setg(3,x.g(3))
  def allocate(x):
   trace.append(('allocate',x.g(4)&0xffffffff,x.mem.rd(x.g(5)&0xffffffff,40).hex(),x.g(6)&0xffffffff));x.mem.wr(x.g(3)&0xffffffff,struct.pack('>5I',AUX+0x7000,0x12345678+i,0x23456789+i,0x3456789a+i,0x456789ab+i));x.setg(3,x.g(3))
  def initialize(x):
   trace.append(('initialize',x.mem.rd(x.g(3)&0xffffffff,20).hex(),x.mem.rd(x.g(4)&0xffffffff,0x4c).hex(),x.mem.rd(SELF,0x140).hex()));x.setg(3,0 if i%7==0 else AUX+0x8000+i*4)
  def publish(x):
   a=[x.g(n)&0xffffffff for n in range(3,6)];trace.append(('publish',a));F.put(x,0x82fab6bc,a[0]);F.put(x,0x82fab6a8,a[1]);F.put(x,0x82fab6b8,a[2])
  c.hooks[0x84803000]=allocate;c.hooks[0x84803010]=free;c.hooks[0x82B635C8]=descriptor;c.hooks[0x82B62720]=initialize
  for at,name in bundle.unresolved.items():
   if name.startswith('?GetResourceDescriptor@TextureState@'):c.hooks[at]=descriptor
   elif name.startswith('?Initialize@TextureState@'):c.hooks[at]=initialize
   elif name.startswith('?SetTextureAtlas@CoronaRenderer@'):c.hooks[at]=publish
   elif name.startswith('??0BaseResourceDescriptor@'):c.hooks[at]=lambda x:x.mem.wr(x.g(3)&0xffffffff,struct.pack('>II',0,1))
  c.run(bundle.entry if compiled else 0x823FD000)
  # Native publisher is inline; express its same observable three fields as one event.
  if not compiled:trace.append(('publish',[int.from_bytes(c.mem.rd(0x82fab6bc,4),'big'),int.from_bytes(c.mem.rd(0x82fab6a8,4),'big'),int.from_bytes(c.mem.rd(0x82fab6b8,4),'big')]))
  both.append((c.mem.rd(SELF,0x140),c.mem.rd(0x82fab690,0x40),trace))
 if both[0]!=both[1]:errors.append(dict(case=i,bytes=[hex(n) for n,(a,b) in enumerate(zip(both[0][0],both[1][0])) if a!=b],traces=[x[2] for x in both]))
result=dict(cases=128,errors=errors,object=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),scope='whole actual SetTextureAtlas versus ARTIST823FD000:oldresourcefree,all76parameterbytes/padding,all5allocationlanes,descriptorandInitializeflow,persistentUVpublication,count4,all140managerbytes',limits='allocator,TextureState descriptor/init andexisting3storeCoronaRendererpublisherarecontrolled;otherCoronamethodsreceivedcompileclosurefixesonlyandarenotproved')
(out/'corona-atlas-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print('128 native Corona atlas cases; errors',len(errors));print(errors[:2]);raise SystemExit(bool(errors))
