from pathlib import Path
import sys,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/relocator_job_terra/worker-final');r=json.loads((out/'compile-results.json').read_text())['results'][0];b=F.Bundle([F.Coff(Path(r['object']),'?Execute@RelocatorJob@@')]);b.select('?Execute@RelocatorJob@@');A=0x40000000;DATA=A+0x1000;PARAM=A+0x2000;POSTER=A+0x3000;READER=A+0x3100

def run(native):
 c=F.cpu(b);c.mem.add(A,0x5000);c.mem.add(0x83015F20,8192);c.mem.wr(A,b'\xa5'*0xC80);F.put(c,DATA+16,PARAM);F.put(c,PARAM,0x12345678);F.put(c,PARAM+4,POSTER);F.put(c,PARAM+8,READER);F.put(c,PARAM+12,0);F.put(c,0x84803004,0x84803500);n=[0];prints=[];addcount=[0]
 def rctor(cpu):x=cpu.g(3)&0xffffffff;F.put(cpu,x,0xffffffff);F.put(cpu,x+4,cpu.g(4))
 def pctor(cpu):F.put(cpu,cpu.g(3)&0xffffffff,cpu.g(4))
 def read(cpu):
  if n[0]>=2001:cpu.setg(3,1);return
  n[0]+=1;F.put(cpu,cpu.g(6)&0xffffffff,0);cpu.setg(3,0)
 def add(cpu):addcount[0]+=1;cpu.setg(3,0)
 def clear(cpu):F.put(cpu,(cpu.g(3)&0xffffffff)+4,0)
 def ctor(cpu):
  p=cpu.g(3)&0xffffffff;dest=cpu.g(4)&0xffffffff;F.put(cpu,p,0x84803000);F.put(cpu,p+8,dest);cpu.mem.wr(dest,b'\0')
 def sink(cpu):
  ptr=cpu.g(3)&0xffffffff;addr=int.from_bytes(cpu.mem.rd(ptr+8,4),'big');src=cpu.g(4)&0xffffffff;cpu.mem.wr(addr,F.cstring(cpu,src).encode()+b'\0');cpu.setg(3,ptr)
 def report(cpu):prints.append([F.cstring(cpu,cpu.g(3)&0xffffffff),cpu.g(4)&0xffffffff,cpu.g(5)&0xffffffff,F.cstring(cpu,cpu.g(6)&0xffffffff)])
 def noop(cpu):pass
 c.hooks.update({0x82867830:rctor,0x82867FF0:pctor,0x82867A00:read,0x828680F0:add,0x828678B0:noop,0x82868070:noop,0x82815E58:clear,0x82815E80:sink,0x82815928:report,0x84803500:sink})
 for addr,name in b.unresolved.items():
  for key,fn in [('?Construct@DataStreamCommandReader',rctor),('?Construct@DataStreamResultPoster',pctor),('?ReadCommands@DataStreamCommandReader',read),('?AddResults@DataStreamResultPoster',add),('?Destruct@DataStreamCommandReader',noop),('?Destruct@DataStreamResultPoster',noop),('??0StrStream@',ctor),('?PrintStringed@Assert@',report)]:
   if name.startswith(key):c.hooks[addr]=fn
 c.max_steps=5000000;c.setg(3,A);c.setg(4,DATA);c.setg(5,0);c.run(b.entry if native else 0x82AD2BF8)
 return {'context':c.mem.rd(A,0xC80).hex(),'read_batches':n[0],'add_calls':addcount[0],'prints':prints}
x=run(False);y=run(True);assert x==y,(x['prints'],y['prints']);assert len(x['prints'])==4
res={'cases':1,'batches':2001,'prints':x['prints'],'scope':'actual original and compiled full hijack path, 2001 controlled zero-count successful reads followed by exhaustion, with controlled stream/assert services; compares context bytes, post count, two pairs of diagnostic arguments; PPC emulator models privileged MSR writes, no hardware claim','object_sha256':hashlib.sha256(Path(r['object']).read_bytes()).hexdigest()};(out/'hijack-overflow-comparison.json').write_text(json.dumps(res,indent=2)+'\n');print('overflow 2001 batches original/compiled pass, prints',len(x['prints']))
