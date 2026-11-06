from pathlib import Path
import sys,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/relocator_job_terra');row=json.loads((out/'worker-partial/compile-results.json').read_text())['results'][0];b=F.Bundle([F.Coff(Path(row['object']),'?Execute@RelocatorJob@@')]);b.select('?Execute@RelocatorJob@@');A=0x40000000;SELF=A;DATA=A+0x1000;OPS=A+0x1100;BOUNCE=A+0x2000;MEM=A+0x3000
cases=json.loads((out/'original-execute-copy-cases.json').read_text())['cases'];bad=[]
for i,t in enumerate(cases):
 c=F.cpu(b);c.mem.add(A,0x5000);c.mem.wr(MEM,bytes.fromhex(t['initial_hex']));c.mem.wr(BOUNCE,b'\xa5'*128);F.put(c,DATA,OPS);F.put(c,DATA+4,1);F.put(c,DATA+8,BOUNCE);F.put(c,DATA+12,t['capacity']);F.put(c,DATA+16,0);F.put(c,OPS,MEM+t['src']);F.put(c,OPS+4,MEM+t['dest']);F.put(c,OPS+8,t['size']);F.put(c,OPS+12,0);calls=[]
 def copy(cpu):
  d=cpu.g(3)&0xffffffff;s=cpu.g(4)&0xffffffff;n=cpu.g(5)&0xffffffff;calls.append([d,s,n]);cpu.mem.wr(d,cpu.mem.rd(s,n));cpu.setg(3,d)
 for a,n in b.unresolved.items():
  if n=='XMemCpy':c.hooks[a]=copy
 c.setg(3,SELF);c.setg(4,DATA);c.setg(5,2);c.run(b.entry)
 actual=(c.mem.rd(MEM,1024).hex(),c.mem.rd(BOUNCE,128).hex(),calls,c.mem.rd(SELF,4).hex());expected=(t['final_hex'],t['bounce_hex'],t['copy_calls'],t['context_pointer'])
 if actual!=expected:bad.append({'case':i,'expected_calls':expected[2],'actual_calls':actual[2]})
res={'cases':len(cases),'mismatches':bad,'object_sha256':hashlib.sha256(Path(row['object']).read_bytes()).hexdigest(),'scope':'actual compiled Execute copy arm vs actual original 82AD2BF8 golden; one op/case; buffer state, XMemCpy calls and stored context; XMemCpy controlled; hijack excluded and definition unresolved'};(out/'worker-partial/copy-comparison.json').write_text(json.dumps(res,indent=2)+'\n');print('cases',len(cases),'mismatches',len(bad));print(bad[:1]);assert not bad
