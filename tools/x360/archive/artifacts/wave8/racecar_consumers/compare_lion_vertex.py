from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
class CPU(R.CPU):
 def step(self,pc,w,nxt,depth):
  if w>>26==31 and (w>>1)&1023==199:
   vs=(w>>21)&31;ra=(w>>16)&31;rb=(w>>11)&31;ea=((self.g(ra) if ra else 0)+self.g(rb))&0xffffffff;self.mem.wr(ea&~3,struct.pack('>I',self.vr[vs][(ea>>2)&3]));return nxt
  return super().step(pc,w,nxt,depth)
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers');obj=out/'BrnLionBlendVertex/probe.obj';o=R.Coff(obj,'?Write@VertexIterator@LionBlendVertex@');rng=random.Random(8227);errors=[]
for i in range(128):
 data=rng.randbytes(0x600);v1=tuple(rng.getrandbits(32) for _ in range(4));v2=tuple(rng.getrandbits(32) for _ in range(4));results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu();c.__class__=CPU
  if not compiled:
   for addr,file in [(0x8227e568,'8227E568.bin'),(0x8227c2e8,'8227C2E8.bin')]:
    image=(out/file).read_bytes();c.mem.add(addr,len(image),image)
  a=R.M.S.SELF;b=R.M.S.AUX;c.mem.wr(a,bytes(0x100));c.mem.wr(b,data);c.mem.wr(a,struct.pack('>4I',b+0xfc,b+0x100,b+0x300,36));c.setg(3,a);c.setg(4,b+0x400);c.vr[1]=v1;c.vr[2]=v2
  try:c.run(o.entry if compiled else 0x8227e568)
  except Exception as e:print('error',compiled,type(e).__name__,e);raise
  results.append((c.mem.rd(a,0x100),c.mem.rd(b,0x600)))
 if results[0]!=results[1]:errors.append({'case':i,'iterator':[j for j,(a,b) in enumerate(zip(results[0][0],results[1][0])) if a!=b][:10],'buffer':[j for j,(a,b) in enumerate(zip(results[0][1],results[1][1])) if a!=b][:12]})
d={'cases':128,'mismatch_count':len(errors),'mismatches':errors[:5],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'whole actual LionBlendVertex::Write vs original 8227E568 and original delegated writer 8227C2E8; random raw v1/v2, RGBA8 and poisoned iterator buffer, all 0x100 iterator/0x600 buffer bytes compared; valid stride/capacity, asserts excluded; no GPU claim'};(out/'lion-vertex-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
