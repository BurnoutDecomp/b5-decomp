from pathlib import Path
import sys,json,random,hashlib,struct
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/relocator_job_terra/worker-final');r=json.loads((out/'compile-results.json').read_text())['results'][0];b=F.Bundle([F.Coff(Path(r['object']),'?Execute@RelocatorJob@@')]);b.select('?Execute@RelocatorJob@@');A=0x40000000;DATA=A+0x1000;PARAM=A+0x2000;POSTER=A+0x3000;READER=A+0x3100
rng=random.Random(0x82AD29F8);bad=[]
def run(native,poison,job,counter,batches):
 c=F.cpu(b);c.mem.add(A,0x5000);c.mem.wr(A,poison);F.put(c,DATA+16,PARAM);F.put(c,PARAM,0x12345678);F.put(c,PARAM+4,POSTER);F.put(c,PARAM+8,READER);F.put(c,PARAM+12,counter);events=[];queue=list(batches)
 def rctor(cpu):
  x=cpu.g(3)&0xffffffff;y=cpu.g(4)&0xffffffff;events.append(('reader_construct',x,y));F.put(cpu,x,0xffffffff);F.put(cpu,x+4,y)
 def pctor(cpu):
  x=cpu.g(3)&0xffffffff;y=cpu.g(4)&0xffffffff;events.append(('poster_construct',x,y));F.put(cpu,x,y)
 def read(cpu):
  x=cpu.g(3)&0xffffffff;dest=cpu.g(4)&0xffffffff;limit=cpu.g(5)&0xffffffff;count=cpu.g(6)&0xffffffff;first=cpu.g(7)&0xffffffff;events.append(('read',x,dest,limit,first))
  if not queue:cpu.setg(3,1);return
  batch=queue.pop(0);cpu.mem.wr(dest,b''.join(batch));F.put(cpu,count,len(batch));cpu.setg(3,0)
 def add(cpu):
  x=cpu.g(3)&0xffffffff;dest=cpu.g(4)&0xffffffff;n=cpu.g(5)&0xffffffff;events.append(('add',x,dest,n,cpu.mem.rd(dest,n*128).hex()));cpu.setg(3,0)
 def rdtor(cpu):events.append(('reader_destruct',cpu.g(3)&0xffffffff))
 def pdtor(cpu):events.append(('poster_destruct',cpu.g(3)&0xffffffff))
 nativehooks={0x82867830:rctor,0x82867FF0:pctor,0x82867A00:read,0x828680F0:add,0x828678B0:rdtor,0x82868070:pdtor};c.hooks.update(nativehooks)
 for a,n in b.unresolved.items():
  for prefix,fn in [('?Construct@DataStreamCommandReader',rctor),('?Construct@DataStreamResultPoster',pctor),('?ReadCommands@DataStreamCommandReader',read),('?AddResults@DataStreamResultPoster',add),('?Destruct@DataStreamCommandReader',rdtor),('?Destruct@DataStreamResultPoster',pdtor)]:
   if n.startswith(prefix):c.hooks[a]=fn
 c.setg(3,A);c.setg(4,DATA);c.setg(5,job);c.run(b.entry if native else 0x82AD2BF8)
 return {'context':c.mem.rd(A,0xC80).hex(),'events':events}
for i in range(96):
 poison=rng.randbytes(0xC80);job=rng.randrange(1,6);counter=rng.choice([-1,0,1,2,3]);batches=[]
 for batchidx in range(rng.randrange(0,4)):
  batch=[]
  for j in range(rng.randrange(0,job+1)):
   n=rng.randrange(0,32);s=bytes(rng.randrange(65,91) for _ in range(n))+b'\0';record=bytearray(rng.randbytes(48));record[:len(s)]=s;struct.pack_into('>i',record,32,rng.randrange(-100000,100000));batch.append(bytes(record))
  batches.append(batch)
 x=run(False,poison,job,counter,batches);y=run(True,poison,job,counter,batches)
 if x!=y:
  bad.append({'case':i,'counter':counter,'job':job,'context_differences':[hex(j) for j,(a,z) in enumerate(zip(bytes.fromhex(x['context']),bytes.fromhex(y['context']))) if a!=z][:30],'original_events':x['events'],'compiled_events':y['events']})
res={'cases':96,'mismatches':bad,'scope':'actual original vs compiled Execute and full ExecuteTestHijacks body, poisoned complete C80 context, 0..3 batches, zero/one/multiple command counts, signed counters, original stream services controlled identically; compares whole context and ordered service calls/result bytes. Overflow diagnostic path excluded.','object_sha256':hashlib.sha256(Path(r['object']).read_bytes()).hexdigest()};(out/'hijack-comparison.json').write_text(json.dumps(res,indent=2)+'\n');print('hijack cases',96,'mismatches',len(bad));print(bad[:1]);assert not bad
