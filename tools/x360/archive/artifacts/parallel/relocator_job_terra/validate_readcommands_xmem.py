from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/relocator_job_terra/reader-xmem');r=json.loads((out/'compile-results.json').read_text())['results'][0];obj=F.Coff(Path(r['object']),'?ReadCommands@DataStreamCommandReader@');b=F.Bundle([obj]);b.select('?ReadCommands@DataStreamCommandReader@')
A=0x40000000;P=A+0x100;SRC=A+0x400;DST=A+0x1000;CNT=A+0x1800;FIRST=A+0x1810

def run(native,idx,posted,maxcount,first,casfail,recordsize):
 c=F.cpu(b);c.mem.add(A,0x4000);F.put(c,A+4,P);F.put(c,P,((3<<24)|idx),8);F.put(c,P+8,SRC);F.put(c,P+16,recordsize);F.put(c,P+20,posted);F.put(c,CNT,0xabababab);F.put(c,FIRST,0xcdcdcdcd);c.mem.wr(SRC,bytes((i*7+11)&255 for i in range(512)));c.mem.wr(DST,b'\xA5'*512);c.failures=casfail;calls=[]
 def copy(cpu):
  dest=cpu.g(3)&0xffffffff;src=cpu.g(4)&0xffffffff;n=cpu.g(5)&0xffffffff;calls.append((dest,src,n));cpu.mem.wr(dest,cpu.mem.rd(src,n));cpu.setg(3,dest)
 c.hooks[0x82926BA0]=copy
 for a,n in b.unresolved.items():
  if n=='XMemCpy':c.hooks[a]=copy
 c.setg(3,A);c.setg(4,DST);c.setg(5,maxcount);c.setg(6,CNT);c.setg(7,FIRST if first else 0);c.run(b.entry if native else 0x82867A00)
 return {'return':c.g(3)&0xffffffff,'status':c.mem.rd(P,8).hex(),'count':c.mem.rd(CNT,4).hex(),'first':c.mem.rd(FIRST,4).hex(),'dest':c.mem.rd(DST,512).hex(),'calls':calls}
rng=random.Random(0x82867A00);bad=[]
for i in range(160):
 posted=rng.randrange(0,14);idx=rng.randrange(0,16);maxcount=rng.randrange(0,10);first=bool(rng.randrange(2));casfail=rng.randrange(0,2);recordsize=rng.randrange(1,20);args=(idx,posted,maxcount,first,casfail,recordsize)
 a=run(False,*args);z=run(True,*args)
 if a!=z:bad.append({'case':i,'args':args,'orig':a,'compiled':z})
report={'cases':160,'mismatches':bad,'scope':'actual PPC original 82867A00 and compiled method; controlled XMemCpy, identical reservation failures; output, status, calls, return and untouched count/index on exhaustion','object_sha256':hashlib.sha256(Path(r['object']).read_bytes()).hexdigest()};(out/'readcommands-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print('cases',160,'mismatches',len(bad));print(bad[:1]);assert not bad


