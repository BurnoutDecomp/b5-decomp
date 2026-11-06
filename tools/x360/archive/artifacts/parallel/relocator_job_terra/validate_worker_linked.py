from pathlib import Path
import sys,json,random,hashlib,struct
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/relocator_job_terra/worker-final');rows=json.loads((out/'compile-results.json').read_text())['results'];pref={'RelocatorJob.cpp':'?Execute@RelocatorJob@@','CgsDataStreamCommandReader.cpp':'?ReadCommands@DataStreamCommandReader@','CgsDataStreamResultPoster.cpp':'?AddResults@DataStreamResultPoster@'};objs=[F.Coff(Path(x['object']),v) for x in rows for k,v in pref.items() if k in x['source']];b=F.Bundle(objs);b.select('?Execute@RelocatorJob@@')
A=0x40000000;DATA=A+0x1000;PARAM=A+0x2000;POSTER=A+0x3000;READER=A+0x3100;COMMANDS=A+0x4000;RESULTS=A+0x5000
rng=random.Random(0x82AD29F8);bad=[]
def run(native,poison,job,counter,cmds):
 c=F.cpu(b);c.mem.add(A,0x8000);c.mem.wr(A,poison);F.put(c,DATA+16,PARAM);F.put(c,PARAM,0x12345678);F.put(c,PARAM+4,POSTER);F.put(c,PARAM+8,READER);F.put(c,PARAM+12,counter);F.put(c,POSTER,0,8);F.put(c,POSTER+8,COMMANDS);F.put(c,POSTER+16,48);F.put(c,POSTER+20,len(cmds));F.put(c,READER,0,8);F.put(c,READER+8,RESULTS);F.put(c,READER+16,128);F.put(c,READER+20,16);c.mem.wr(COMMANDS,b''.join(cmds));c.mem.wr(RESULTS,b'\xC7'*(16*128));calls=[]
 def copy(cpu):
  dest=cpu.g(3)&0xffffffff;src=cpu.g(4)&0xffffffff;n=cpu.g(5)&0xffffffff;calls.append([dest,src,n]);cpu.mem.wr(dest,cpu.mem.rd(src,n));cpu.setg(3,dest)
 c.hooks[0x82926BA0]=copy
 for a,n in b.unresolved.items():
  if n in ['XMemCpy','memcpy']:c.hooks[a]=copy
 c.max_steps=500000;c.setg(3,A);c.setg(4,DATA);c.setg(5,job);c.run(b.entry if native else 0x82AD2BF8)
 return {'context':c.mem.rd(A,0xC80).hex(),'poster':c.mem.rd(POSTER,0x40).hex(),'reader':c.mem.rd(READER,0x40).hex(),'results':c.mem.rd(RESULTS,16*128).hex(),'calls':calls}
for i in range(64):
 poison=rng.randbytes(0xC80);job=rng.randrange(1,6);counter=rng.choice([-1,0,1,2,3]);cmds=[]
 for j in range(rng.randrange(0,9)):
  n=rng.randrange(0,32);s=bytes(rng.randrange(65,91) for _ in range(n))+b'\0';record=bytearray(rng.randbytes(48));record[:len(s)]=s;struct.pack_into('>i',record,32,rng.randrange(-100000,100000));cmds.append(bytes(record))
 x=run(False,poison,job,counter,cmds);y=run(True,poison,job,counter,cmds)
 if x!=y:
  bad.append({'case':i,'counter':counter,'job':job,'num_commands':len(cmds),'context_diff':[hex(j) for j,(a,z) in enumerate(zip(bytes.fromhex(x['context']),bytes.fromhex(y['context']))) if a!=z][:30],'orig_calls':x['calls'],'compiled_calls':y['calls'],'orig_poster':x['poster'],'compiled_poster':y['poster'],'orig_reader':x['reader'],'compiled_reader':y['reader']})
res={'cases':64,'mismatches':bad,'scope':'actual original vs compiled Execute+ExecuteTestHijacks+DataStreamCommandReader and DataStreamResultPoster providers; real CAS/status/read/post/construct/destruct bodies, XMemCpy/memcpy controlled, poisoned full worker+output; compares all worker bytes, poster/reader state, results and ordered copy calls; no overflow/hardware claim','object_sha256':{x['source']:hashlib.sha256(Path(x['object']).read_bytes()).hexdigest() for x in rows if any(k in x['source'] for k in pref)}};(out/'linked-stream-comparison.json').write_text(json.dumps(res,indent=2)+'\n');print('linked stream cases',64,'mismatches',len(bad));print(bad[:1]);assert not bad
