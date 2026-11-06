from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
native=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\native-job-entries-root\native');out=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\parallel\job_entries_author_terra');d=json.loads((native/'compile-results.json').read_text());pref={'Relocator.cpp':'?RelocatorEntry@@','RelocatorJob.cpp':'?Execute@RelocatorJob@@','CgsDataStreamCommandReader.cpp':'?ReadCommands@DataStreamCommandReader@','CgsDataStreamResultPoster.cpp':'?AddResults@DataStreamResultPoster@'};objs=[F.Coff(Path(x['object']),v) for x in d['results'] for k,v in pref.items() if x['source'].endswith('/'+k)];b=F.Bundle(objs);b.select('?RelocatorEntry@@');worker=next(b.bases[(oi,s)]+v for name,(oi,v,s) in b.definitions.items() if 'gaRelocatorJobs' in name and (oi,s) in b.bases);original_worker=0x83248C00
A=0x40000000;DATA=A+0x1000;PARAM=A+0x2000;POSTER=A+0x3000;READER=A+0x3100;COMMANDS=A+0x4000;RESULTS=A+0x5000;OPS=A+0x6000;BOUNCE=A+0x7000;MEM=A+0x8000
rng=random.Random(0x82AD28C0);bad=[]
def run(comp,idx,mode,poison,counter,cmds,copydata):
 c=F.cpu(b);c.mem.add(A,0xA000);c.mem.add(original_worker,0xC80*6);base=worker if comp else original_worker;c.mem.wr(base,poison);F.put(c,DATA,OPS);F.put(c,DATA+4,1);F.put(c,DATA+8,BOUNCE);F.put(c,DATA+12,17);F.put(c,DATA+16,PARAM if mode=='hijack' else 0);F.put(c,PARAM,0x12345678);F.put(c,PARAM+4,POSTER);F.put(c,PARAM+8,READER);F.put(c,PARAM+12,counter);F.put(c,POSTER,0,8);F.put(c,POSTER+8,COMMANDS);F.put(c,POSTER+16,48);F.put(c,POSTER+20,len(cmds));F.put(c,READER,0,8);F.put(c,READER+8,RESULTS);F.put(c,READER+16,128);F.put(c,READER+20,16);c.mem.wr(COMMANDS,b''.join(cmds));c.mem.wr(RESULTS,b'\xc7'*2048);c.mem.wr(MEM,copydata);c.mem.wr(BOUNCE,b'\xa5'*128);F.put(c,OPS,MEM+100);F.put(c,OPS+4,MEM+105);F.put(c,OPS+8,71);F.put(c,OPS+12,0);calls=[];tid=0xF8000088+4*idx
 def thread(cpu):cpu.setg(3,tid);cpu.setg(4,0x12345678)
 def normalize(x):return ('worker',x-base) if base<=x<base+0xC80*6 else ('absolute',x)
 def copy(cpu):
  dst=cpu.g(3)&0xffffffff;src=cpu.g(4)&0xffffffff;n=cpu.g(5)&0xffffffff;calls.append([normalize(dst),normalize(src),n]);cpu.mem.wr(dst,cpu.mem.rd(src,n));cpu.setg(3,dst)
 c.hooks[0x82B42560]=thread;c.hooks[0x82926BA0]=copy
 for a,n in b.unresolved.items():
  if n.startswith('?GetThreadId@Thread@EA@@'):c.hooks[a]=thread
  if n in ['XMemCpy','memcpy']:c.hooks[a]=copy
 c.max_steps=500000;c.setg(3,0x111);c.setg(4,DATA);c.setg(5,0x222);c.setg(6,0x333);c.run(b.entry if comp else 0x82AD28C0)
 return {'workers':c.mem.rd(base,0xC80*6).hex(),'poster':c.mem.rd(POSTER,0x40).hex(),'reader':c.mem.rd(READER,0x40).hex(),'results':c.mem.rd(RESULTS,2048).hex(),'memory':c.mem.rd(MEM,1024).hex(),'bounce':c.mem.rd(BOUNCE,128).hex(),'calls':calls}
for i in range(48):
 idx=i%6;mode='copy' if i<24 else 'hijack';poison=rng.randbytes(0xC80*6);counter=rng.choice([-1,0,1,2,3]);cmds=[]
 for j in range(0 if idx==0 else rng.randrange(0,9)):
  n=rng.randrange(0,32);s=bytes(rng.randrange(65,91) for _ in range(n))+b'\0';rec=bytearray(rng.randbytes(48));rec[:len(s)]=s;struct.pack_into('>i',rec,32,rng.randrange(-100000,100000));cmds.append(bytes(rec))
 copydata=rng.randbytes(1024);a=run(False,idx,mode,poison,counter,cmds,copydata);z=run(True,idx,mode,poison,counter,cmds,copydata)
 if a!=z:bad.append({'case':i,'idx':idx,'mode':mode,'counter':counter,'commands':len(cmds),'worker_diff':[hex(j) for j,(x,y) in enumerate(zip(bytes.fromhex(a['workers']),bytes.fromhex(z['workers']))) if x!=y][:30],'orig_calls':a['calls'],'native_calls':z['calls'],'orig_poster':a['poster'],'native_poster':z['poster'],'orig_reader':a['reader'],'native_reader':z['reader']})
res={'cases':48,'mismatches':bad,'scope':'actual original RelocatorEntry through worker and actual stream reader/poster versus actual compiled Entry+worker+reader/poster objects, six worker indices, 24 copy and 24 hijack cases; thread ID and XMemCpy/memcpy controlled; complete six-worker poisoned memory, queue status, result and copy memory/calls compared; job0 hijack tested only empty stream to avoid original zero-batch nontermination; no hardware/strict-link claim','original_worker_base':hex(original_worker),'compiled_worker_base':hex(worker),'objects':{x['source']:hashlib.sha256(Path(x['object']).read_bytes()).hexdigest() for x in d['results'] if any(x['source'].endswith('/'+k) for k in pref)}};(out/'linked-relocator-comparison.json').write_text(json.dumps(res,indent=2)+'\n');print('linked entry-worker cases',48,'mismatches',len(bad));print(bad[:1]);assert not bad
