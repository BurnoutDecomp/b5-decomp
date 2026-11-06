from pathlib import Path
import hashlib,json,random,struct,sys
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(parent/'scratch/x360_sdk'));sys.path.insert(0,str(parent/'tools/re'))
import native_atomic_fixture as F
import x360rd as R
out=parent/'b5-decomp/build/x360/parallel/native_textures/renderer-checkpoint1'
r=json.loads((out/'compile-results.json').read_text())['results'][0];p=Path(r['object']);o=F.Coff(p,'?MemoryUsage@BrnRendererModule@@');bundle=F.Bundle([o]);bundle.select('?MemoryUsage@BrnRendererModule@@')
globals_map={'?gxMessageFilterFlags@Message@CgsDev@@3_KA':0x82F31908,'?gpDebugPrint@Log@CgsDev@@3PAUStrStreamBase@2@A':0x82F31904}
for (oi,sec),base in bundle.bases.items():
 obj=bundle.objects[oi];body=bundle.images[base]
 for at,index,kind in obj.relocations(sec):
  name,_,_=obj.symbols[index]
  if name not in globals_map:continue
  address=globals_map[name];word=struct.unpack_from('>I',body,at)[0]
  if kind in [0x10,0x11]:word=(word&0xffff0000)|(((address+0x8000)>>16 if kind==0x10 else address)&0xffff)
  elif kind==2:word=address
  else:raise AssertionError((name,kind))
  struct.pack_into('>I',body,at,word)
def branch(at):
 w=R.u32(at);off=w&0x03fffffc;return (at+off-(1<<26 if off&(1<<25) else 0))&0xffffffff
originals=[branch(x) for x in [0x823FBDF4,0x823FBE24,0x823FBE30]]
SELF=F.M.S.SELF;AUX=F.M.S.AUX;rng=random.Random(0x823FBDE0);errors=[]
for i in range(128):
 image=rng.randbytes(0xca80);bases=rng.randbytes(20);usage=rng.randbytes(40);capacity=rng.randbytes(40);flags=rng.getrandbits(64);both=[]
 for compiled in [False,True]:
  c=F.cpu(bundle);c.mem.add(0x82f31000,0x1000);c.mem.wr(SELF,image);F.put(c,SELF+0x394c,AUX+0x1000);F.put(c,AUX+0x1004,AUX+0x1100);c.mem.wr(AUX+0x1200,bases);F.put(c,0x82F31908,flags,8);F.put(c,0x82F31904,AUX+0x1800)
  for stream in [AUX+0x1800,AUX+0x1a00]:F.put(c,stream,AUX+0x1900)
  F.put(c,AUX+0x1904,0x84803000);c.setg(3,SELF);trace=[];logs=[]
  def flip(x):
   if i%4==1 and len(logs)%3==1:F.put(x,0x82F31908,int.from_bytes(x.mem.rd(0x82F31908,8),'big')^1,8)
   if i%4==2 and len(logs)==2:F.put(x,0x82F31904,AUX+0x1a00)
  def text(x):
   receiver=x.g(3)&0xffffffff;logs.append(('text',receiver,F.cstring(x,x.g(4)&0xffffffff)));flip(x);x.setg(3,AUX+0x1a00 if i%3==1 else receiver)
  def scalar(x):
   receiver=x.g(3)&0xffffffff;logs.append(('u32',receiver,x.g(4)&0xffffffff));flip(x);x.setg(3,AUX+0x1a00 if i%3==2 else receiver)
  def resource(which):
   def run(x):
    receiver=x.g(3 if which=='base' else 4)&0xffffffff;assert receiver==(AUX+0x1100 if compiled else AUX+0x1000),(which,hex(receiver))
    trace.append(which)
    if which=='base':x.setg(3,AUX+0x1200)
    else:x.mem.wr(x.g(3)&0xffffffff,usage if which=='usage' else capacity)
   return run
  c.hooks[originals[0]]=resource('base');c.hooks[originals[1]]=resource('usage');c.hooks[originals[2]]=resource('capacity');c.hooks[0x821F0EC8]=scalar;c.hooks[0x84803000]=text
  for at,name in bundle.unresolved.items():
   if name.startswith('?GetLinearHeapBase@'):c.hooks[at]=resource('base')
   elif name.startswith('?GetCurrentUsage@'):c.hooks[at]=resource('usage')
   elif name.startswith('?GetCapacity@'):c.hooks[at]=resource('capacity')
   elif name.startswith('??6StrStreamBase@'):c.hooks[at]=scalar
  try:c.run(bundle.entry if compiled else 0x823FBDE0)
  except Exception:
   print("failure",i,compiled,"last",hex(c.last_pc),"g3",hex(c.g(3)),"ctr",hex(c.ctr),"trace",trace,"logs",logs);raise
  both.append((trace,logs,c.mem.rd(SELF,0xca80).hex(),c.mem.rd(0x82F31904,0x10).hex()))
 if both[0]!=both[1]:errors.append(dict(case=i,original=both[0][:2],compiled=both[1][:2]))
result=dict(cases=128,errors=errors,object=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),scope='whole actual Renderer::MemoryUsage versus ARTIST823FBDE0: all5resource lanes sampled before logging,32bit address/capacity arithmetic,8live filter checks,exact strings and scalarstream chaining,changing filter/globalstream and sinkreturn identity',limits='resource queries and stream output are controlled services; underlying allocator and formatting bodies are not re-proved')
(out/'renderer-memory-report-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print('128 native MemoryUsage comparisons; errors',len(errors));print(errors[:2]);raise SystemExit(bool(errors))
