from pathlib import Path
import hashlib,json,sys
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk')
import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/ai_accessors_terra/candidate')
rows=json.loads((out/'compile-results.json').read_text())['results']
class Controlled(F.AtomicCPU):
 def step(self,pc,word,nxt,depth):
  if pc==self.skip:
   self.trace.append('assert-format-fire-end-controlled')
   return self.tail
  return super().step(pc,word,nxt,depth)
specs=[('ResetPump','?GetAICar@AIModule@',0x82765AD0,0x82765AF8,0x82765B78,35,5472,560),('Drive','?GetAIDriver@AIModule@',0x82765B90,0x82765BB8,0x82765C38,8,7536,192080)]
checks=[]
for file,prefix,original,skip,tail,limit,stride,offset in specs:
 row=next(r for r in rows if file in r['source']); b=F.Bundle([F.Coff(Path(row['object']),prefix)]);b.select(prefix)
 def run(compiled,index):
  c=F.cpu(b);c.__class__=Controlled;c.trace=[];c.skip=(b.entry+0x20 if compiled else skip);c.tail=(b.entry+0x74 if compiled else tail)
  for addr,name in b.unresolved.items():
   if 'BeginAssert' in name:c.hooks[addr]=lambda cpu:cpu.trace.append('begin')
  c.hooks[0x82817548]=lambda cpu:cpu.trace.append('begin')
  c.setg(3,0x40000000);c.setg(4,index);c.run(b.entry if compiled else original)
  return (c.g(3)&0xffffffff,c.trace)
 for index in list(range(limit))+[limit,limit+1,0x7fffffff,0x80000000,0xffffffff]:
  a=run(False,index);z=run(True,index); expected=(0x40000000+offset+(stride*index))&0xffffffff
  assert a==z and a[0]==expected and bool(a[1])==(index>=limit),(file,index,a,z,hex(expected))
  checks.append({'function':prefix,'index':hex(index),'result':hex(a[0]),'asserted':bool(a[1])})
res={'scope':'actual original and compiled complete entry and address computation; BeginAssert hook returns, stream construction/format/FireAssert/EndAssert controlled as one block; assertion metadata separately inspected in original and compiled listing; no out-of-range dereference', 'cases':len(checks),'results':checks,'object_sha256':{r['source']:hashlib.sha256(Path(r['object']).read_bytes()).hexdigest() for r in rows}}
(out/'accessor-comparison.json').write_text(json.dumps(res,indent=2)+'\n');print('cases',len(checks),'pass')
