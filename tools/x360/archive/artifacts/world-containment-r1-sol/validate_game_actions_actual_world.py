"""Actual HandleGameActions prefix at identical native World tail addresses.

Original/product queue traversal and actions7/8/34 execute through the first PerfMon
call; getter/assert services are controlled. The original index-1 assert continuation
and index8 writes remain. No named World location normalization is used. Parent size
and checked offsets match; independent WorldEntity size, later bridges and action23
remain outside this bounded prefix proof.
"""
from pathlib import Path
import hashlib,json,random,struct,sys
OUT=Path(__file__).resolve().parent
sys.path.insert(0,'D:/Reverse/BP-X360-Experimental-Artifacts/job-provider-526adc');from validate_startup_phase import F,Obj,Bundle
WORLD=0x50000000;QUEUE=0x58000000;INPUT=0x59000000;STACK=0x40000000;MASK=0xffffffff
class Stop(Exception):pass
def make_queue(index):
 rng=random.Random(0xC44D8+index);data=bytearray(rng.randbytes(0x3500));data[0]=1
 actions=[(7,),(7,8),(8,7),(34,7),(7,34),(8,)][(index//10)%6];data_base=1;position=15;control=index%3
 for action in actions:
  struct.pack_into('>ii',data,data_base+position,action,4);struct.pack_into('>i',data,data_base+position+16,control);position+=32
 struct.pack_into('>iii',data,0x3404,position,len(actions),15)
 return bytes(data),actions,control

def execute(b,compiled,index,layout):
 m=F.M.V.Mem();m.add(STACK,0x10000);m.add(QUEUE,0x3500);m.add(0x84800000,0x10000)
 assert layout['world_size']==0x5e3c80 and layout['player_index_offset']==0x5e1ae8
 off=0x5e1ae8;page=(WORLD+off)&~0xfff;m.add(page,0x1000)
 if compiled:
  for a,data in b.images.items():m.add(a,len(data),data)
 rng=random.Random(0xC44D800+index);tail=bytearray(rng.randbytes(0x100));car_index=[-1,0,1,2,3,4,5,6,7,8][index%10];struct.pack_into('>i',tail,0x20,car_index);tail[0x48]=index%2;m.wr(WORLD+off-0x20,tail)
 queue,actions,control=make_queue(index);m.wr(QUEUE,queue)
 c=F.AtomicCPU(m);c.setg(1,STACK+0x8000);c.max_steps=30000;c.atomic_trace=[];c.fence_trace=[];c.msr=0x8000;c.reservation=None;c.failures=0;trace=[]
 def getter(cpu):assert cpu.g(3)&MASK==INPUT;trace.append('getter');cpu.setg(3,QUEUE)
 def begin(cpu):trace.append('begin')
 def fire(cpu):trace.append(['assert',F.cstring(cpu,cpu.g(3)&MASK)])
 def end(cpu):trace.append('end')
 def stop(cpu):raise Stop()
 def reject(cpu):raise AssertionError(('unexpected provider',hex(getattr(cpu,'last_pc',0))))
 if compiled:
  for a,name in b.unresolved.items():
   if name.startswith('?GetGameActionQueue@'):c.hooks[a]=getter
   elif name.startswith('?BeginAssert@'):c.hooks[a]=begin
   elif name.startswith('?FireAssert@'):c.hooks[a]=fire
   elif name.startswith('?EndAssert@'):c.hooks[a]=end
   elif name.startswith('?StartMonitor@'):c.hooks[a]=stop
   else:c.hooks[a]=reject
 else:
  c.hooks[0x827a3708]=getter;c.hooks[0x82817548]=begin;c.hooks[0x82820810]=fire;c.hooks[0x82817558]=end
  # Original StartMonitor identity is taken from the exact call instruction below.
  word=F.M.V.x360rd.u32(0x827c477c);assert word>>26==18 and word&1;delta=word&0x3fffffc
  if delta&0x2000000:delta-=0x4000000
  c.hooks[(0x827c477c+delta)&MASK]=stop
 c.setg(3,WORLD);c.setg(4,0x5a000000);c.setg(5,0x5b000000);c.setg(6,0);c.setg(7,0);c.setg(8,INPUT)
 try:c.run(b.entry if compiled else 0x827c44d8)
 except Stop:pass
 except Exception as e:raise RuntimeError((compiled,index,hex(getattr(c,'last_pc',0))))from e
 else:raise AssertionError('did not reach prefix stop')
 return{'same_physical_tail_region':m.rd(WORLD+off-0x20,0x100).hex(),'queue':m.rd(QUEUE,0x3500).hex(),'trace':trace,'actions':actions,'index':car_index,'control':control}

def main():
 row=json.loads((OUT/'parent-guards-final-World-v1/native/compile-results.json').read_text())['results'][0];o=Obj(row['object']);b=Bundle([o]);b.select('?HandleGameActions@WorldModule@');layout=json.loads((OUT/'actual-publisher-layout.json').read_text())['values'];errors=[]
 for i in range(256):
  pair=[execute(b,c,i,layout)for c in(False,True)]
  if pair[0]!=pair[1]:
   error={'case':i,'original':pair[0],'compiled':pair[1]};errors.append(error);break
 result={'cases':i+1,'mismatches':errors,'scope':__doc__,'object':row['object'],'object_sha256':hashlib.sha256(Path(row['object']).read_bytes()).hexdigest(),'script_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'metadata_excluded':'Existing CGS_ASSERT current source file/line differs from ARTIST metadata; message and Begin/Fire/End sequence compared.','actual_layout_receipt':'actual-publisher-layout.json','layout_receipt_sha256':hashlib.sha256((OUT/'actual-publisher-layout.json').read_bytes()).hexdigest(),'named_location_normalization':False,'both_player_index_address':'0x505E1AE8'};(OUT/'game-actions-actual-world-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print('Action prefix',i+1,'cases;',len(errors),'mismatches',flush=True)
 if errors:print(str(errors[0])[:4500])
 raise SystemExit(bool(errors))
if __name__=='__main__':main()
