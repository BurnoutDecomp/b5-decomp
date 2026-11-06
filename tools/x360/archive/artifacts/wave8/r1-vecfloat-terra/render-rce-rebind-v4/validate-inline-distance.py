from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
original_exec=R.CPU.exec_vector
def exec_vector(self,addr,word,mn):
 if word==0x1800fe73:
  old=self.vr[13]; self.vr[13]=self.vr[127]; result=original_exec(self,addr,0x1000694a,mn); self.vr[13]=old; return result
 return original_exec(self,addr,word,mn)
R.CPU.exec_vector=exec_vector
base=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra');out=base/'render-rce-rebind-v4';obj=out/'render/probe.obj';o=R.Coff(obj,'?GenerateDispatchLists@RaceCarEntityModule@BrnWorld@@');rng=random.Random(0x822e7c3c);errors=[]
for i in range(512):
 cam=struct.unpack('>4I',struct.pack('>4f',*[rng.uniform(-500,500) for _ in range(4)]));pos=cam if i%16==0 else struct.unpack('>4I',struct.pack('>4f',*[rng.uniform(-500,500) for _ in range(4)]));results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu()
  if compiled:
   c.mem.wr(R.M.S.SELF+0x220,struct.pack('>4I',*pos));c.setg(3,R.M.S.SELF);c.vr[123]=cam;c.mem.wr(o.entry+0x390,bytes.fromhex('4e800020'));c.run(o.entry+0x330);results.append((c.vr[126],c.vr[127]))
  else:
   image=(base/'822E79F8.bin').read_bytes();c.mem.add(0x822e79f8,len(image),image);c.mem.wr(R.M.S.SELF+0x30,struct.pack('>4I',*pos));c.setg(3,R.M.S.SELF);c.setg(26,0x30);c.vr[123]=cam;c.mem.wr(0x822e7c94,bytes.fromhex('4e800020'));c.run(0x822e7c3c);results.append((c.vr[127],c.vr[0]))
 if results[0]!=results[1]:errors.append([i,results[0],results[1]])
d={'cases':512,'mismatches':errors[:5],'count':len(errors),'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'symbol':'GenerateDispatchLists@RaceCarEntityModule','compiled_slice':'COMDAT+0x330..0x38C actual replay branch inlined distance','original_slice':'822E7C3C..822E7C90','scope':'actual inlined GenerateDispatchLists code vs ARTIST distance slice, finite camera/position vectors incl equal points, four lanes and squared output; modeled reciprocal estimate; exact 0x1800FE73 high-register vrsqrtefp128 mapped to existing low-register reciprocal-estimate model; standalone slice, no full dispatch branch/cull/runtime claim'};(out/'actual-inline-distance-comparison.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2));assert not errors
