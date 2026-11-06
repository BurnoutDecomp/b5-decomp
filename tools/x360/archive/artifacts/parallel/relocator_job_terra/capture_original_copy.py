from pathlib import Path
import sys,json,random,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/relocator_job_terra');r=json.loads((out/'reader-xmem/compile-results.json').read_text())['results'][0];b=F.Bundle([F.Coff(Path(r['object']),'?ReadCommands@DataStreamCommandReader@')]);b.select('?ReadCommands@DataStreamCommandReader@');A=0x40000000;SELF=A;DATA=A+0x1000;OPS=A+0x1100;BOUNCE=A+0x2000;MEM=A+0x3000
rng=random.Random(0x82ad2bf8);cases=[]
for i in range(128):
 src=rng.randrange(128,512);n=rng.randrange(0,100);dest=max(0,src+rng.randrange(-110,111));capacity=rng.randrange(1,50);initial=bytes(rng.randrange(256) for _ in range(1024));c=F.cpu(b);c.mem.add(A,0x5000);c.mem.wr(MEM,initial);c.mem.wr(BOUNCE,b'\xa5'*128);F.put(c,DATA,OPS);F.put(c,DATA+4,1);F.put(c,DATA+8,BOUNCE);F.put(c,DATA+12,capacity);F.put(c,DATA+16,0);F.put(c,OPS,MEM+src);F.put(c,OPS+4,MEM+dest);F.put(c,OPS+8,n);F.put(c,OPS+12,0);calls=[]
 def copy(cpu):
  d=cpu.g(3)&0xffffffff;s=cpu.g(4)&0xffffffff;z=cpu.g(5)&0xffffffff;calls.append([d,s,z]);cpu.mem.wr(d,cpu.mem.rd(s,z));cpu.setg(3,d)
 c.hooks[0x82926BA0]=copy;c.setg(3,SELF);c.setg(4,DATA);c.setg(5,2);c.run(0x82ad2bf8)
 cases.append({'src':src,'dest':dest,'size':n,'capacity':capacity,'initial_hex':initial.hex(),'final_hex':c.mem.rd(MEM,1024).hex(),'bounce_hex':c.mem.rd(BOUNCE,128).hex(),'copy_calls':calls,'context_pointer':c.mem.rd(SELF,4).hex()})
p=out/'original-execute-copy-cases.json';p.write_text(json.dumps({'original':'0x82AD2BF8','cases':cases,'scope':'actual original PPC Execute, one relocation op per case, controlled XMemCpy; random finite overlapping/nonoverlapping ranges, zero size, partial chunks; excludes hijack arm'},indent=2));print('original cases',len(cases),'sha256',hashlib.sha256(p.read_bytes()).hexdigest())
