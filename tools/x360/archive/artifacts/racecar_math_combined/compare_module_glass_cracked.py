import sys,struct,random,json,hashlib
from pathlib import Path
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
from validate_resource_pointer import Bundle
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_math_combined');module=out/'module/probe.obj';params=out/'params/probe.obj';a=R.Coff(module,'?ReadUpdatedActiveRaceCarDataFromPhysics@RaceCarEntityModule@');b=R.Coff(params,'?SetCrackedGlassScaleFactorsN@RenderParams@');bundle=Bundle([a,b]);bundle.select('?ReadUpdatedActiveRaceCarDataFromPhysics@RaceCarEntityModule@');name=next(x for x in bundle.definitions if x.startswith('?SetCrackedGlassScaleFactorsN@RenderParams@'));oi,v,sec=bundle.definitions[name];target=bundle.bases[(oi,sec)]+v;rng=random.Random(655);errors=[]
class CPU(R.CPU):
 def step(self,pc,w,nxt,depth):
  if pc==target:self.reached+=1
  return super().step(pc,w,nxt,depth)
for i in range(128):
 width=struct.unpack('>f',struct.pack('>f',rng.uniform(0.0001,100)))[0];pane=i%8;data=rng.randbytes(0x14a0);results=[]
 for compiled in (False,True):
  c=bundle.cpu() if compiled else R.M.new_cpu();c.__class__=CPU;c.reached=0;sp=c.g(1);c.mem.wr(R.M.S.SELF,data)
  if compiled:c.setg(29,R.M.S.SELF);c.setg(30,pane);c.setf(31,width);start=bundle.entry+0x490;end=bundle.entry+0x4a8
  else:
   image=(out/'822E87B8.bin').read_bytes();c.mem.add(0x822e87b8,len(image),image);c.setg(30,R.M.S.SELF);c.setg(31,pane);c.mem.wr(sp+0xc0,struct.pack('>f',width));start=0x822e8c18;end=0x822e8c30
  c.mem.wr(end,bytes.fromhex('4e800020'));c.run(start)
  if compiled:assert c.reached==1,c.reached
  results.append(c.mem.rd(R.M.S.SELF,0x14a0))
 if results[0]!=results[1]:errors.append({'case':i,'offsets':[j for j,(a,b) in enumerate(zip(*results)) if a!=b][:20]})
d={'cases':128,'mismatch_count':len(errors),'mismatches':errors[:5],'module_sha256':hashlib.sha256(module.read_bytes()).hexdigest(),'params_sha256':hashlib.sha256(params.read_bytes()).hexdigest(),'scope':'original cracked-glass inline stores 822E8C18..2C vs actual compiled common caller slice and linked actual FPU8 setter; valid panes, random finite width and full poisoned 0x14A0 params; prior event path and assertion-failure path excluded'};(out/'module-glass-cracked-linked-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
