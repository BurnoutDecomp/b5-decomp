from pathlib import Path
import json,sys,struct
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk')
import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/ai_accessors_terra/candidate')
rows=json.loads((out/'compile-results.json').read_text())['results'];results=[]
for file,prefix,limit,line in [('ResetPump','?GetAICar@AIModule@',35,445),('Drive','?GetAIDriver@AIModule@',8,453)]:
 row=next(x for x in rows if file in x['source']);b=F.Bundle([F.Coff(Path(row['object']),prefix)]);b.select(prefix)
 for index in [limit,0xffffffff,0x80000000]:
  c=F.cpu(b);events=[]; F.put(c,0x84800010,256);F.put(c,0x84800020,0x84802000);F.put(c,0x84801004,0x84801100)
  def begin(cpu):events.append(('begin',))
  def ctor(cpu):F.put(cpu,cpu.g(3),0x84801000);F.put(cpu,cpu.g(3)+8,0x84802000);cpu.mem.wr(0x84802000,b'\0')
  def sink(cpu):
   s=F.cstring(cpu,(cpu.g(4)&0xffffffff));events.append(('text',s));cpu.mem.wr(0x84802000,s.encode()+b'\0')
  def number(cpu):
   v=cpu.g(4)&0xffffffff;v=(v&0x7fffffff)-(v&0x80000000);s=F.cstring(cpu,0x84802000)+str(v);cpu.mem.wr(0x84802000,s.encode()+b'\0');events.append(('index',v))
  def fire(cpu):events.append(('fire',F.cstring(cpu,cpu.g(3)),F.cstring(cpu,(cpu.g(4)&0xffffffff)),cpu.g(5)&0xffffffff))
  def end(cpu):events.append(('end',))
  for a,n in b.unresolved.items():
   for key,func in [('?BeginAssert',begin),('??0StrStream@',ctor),('??6StrStreamBase@',number),('?FireAssert',fire),('?EndAssert',end)]:
    if n.startswith(key):c.hooks[a]=func
  c.hooks[0x84801100]=sink;c.setg(3,0x40000000);c.setg(4,index);c.run(b.entry)
  signed=(index&0x7fffffff)-(index&0x80000000);msg=('Invalid AI car index: ' if file=='ResetPump' else 'Invalid driver index: ')+str(signed)
  expected=[('begin',),('text',msg.rsplit(str(signed),1)[0]),('index',signed),('fire',msg,'d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\world\\ai\\BrnAIModule.h',line),('end',)]
  assert events==expected,(index,events,expected)
  results.append({'function':prefix,'index':hex(index),'events':events})
(out/'assertion-comparison.json').write_text(json.dumps({'scope':'actual compiled accessor with controlled stream and assertion services; checks call order, signed index, exact text, file and line; original metadata independently read from ARTIST 82765AD0/82765B90','cases':len(results),'results':results},indent=2)+'\n');print('cases',len(results),'pass')

