from pathlib import Path
import sys,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/relocator_job_terra/worker-partial');row=json.loads((out/'compile-results.json').read_text())['results'][0];b=F.Bundle([F.Coff(Path(row['object']),'?Execute@RelocatorJob@@')]);b.select('?Execute@RelocatorJob@@');A=0x40000000;D=A+0x1000;P=A+0x2000
results=[]
for jobid in [0,1,5,0xffffffff]:
 def run(comp):
  c=F.cpu(b);c.mem.add(A,0x4000);F.put(c,D+16,P);events=[]
  def hijack(cpu):events.append([cpu.g(3)&0xffffffff,cpu.g(4)&0xffffffff,cpu.g(5)&0xffffffff])
  c.hooks[0x82AD29F8]=hijack
  for a,n in b.unresolved.items():
   if n.startswith('?ExecuteTestHijacks@RelocatorJob@'):c.hooks[a]=hijack
  c.setg(3,A);c.setg(4,D);c.setg(5,jobid);c.run(b.entry if comp else 0x82AD2BF8)
  return events,c.mem.rd(A,4).hex()
 a=run(False);z=run(True);assert a==z,(jobid,a,z);results.append({'jobid':hex(jobid),'hijack_calls':a[0]})
(out/'hijack-dispatch-comparison.json').write_text(json.dumps({'scope':'actual compiled and original Execute dispatch to controlled genuine hijack method; argument and context pointer only; method body NOT tested or implemented','cases':len(results),'results':results,'object_sha256':hashlib.sha256(Path(row['object']).read_bytes()).hexdigest()},indent=2)+'\n');print('dispatch cases',len(results),'pass')
