from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra');obj=out/'ActiveRaceCarRenderParams/probe.obj';rng=random.Random(0x822b87dc);errors=[]
for sym,addr in [('?GetWheelTransform@RenderParams@',0x822a3220),('?GetWheelScaleMatrix@RenderParams@',0x822a31b8)]:
 o=R.Coff(obj,sym)
 for i in range(128):
  idx=i%6;before=rng.randbytes(0x2000);results=[]
  for compiled in (False,True):
   c=o.cpu() if compiled else R.M.new_cpu();self=R.M.S.SELF+0x1000;c.mem.wr(self,before);c.setg(3,self);c.setg(4,idx)
   if not compiled:
    image=(out/('%08X.bin'%addr)).read_bytes();c.mem.add(addr,len(image),image)
   c.run(o.entry if compiled else addr);results.append((c.g(3),c.mem.rd(self,0x2000)))
  if results[0]!=results[1]:errors.append([sym,i])
o=R.Coff(obj,'?SetWheelTransform@RenderParams@')
for i in range(128):
 idx=i%4;before=rng.randbytes(0x2000);matrix=rng.randbytes(64);results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu();self=R.M.S.SELF+0x1000;c.mem.wr(self,before);c.mem.wr(R.M.S.AUX,matrix)
  if compiled:c.setg(3,self);c.setg(4,idx);c.setg(5,R.M.S.AUX);start=o.entry;end=None
  else:
   image=(out/'822B8738.bin').read_bytes();c.mem.add(0x822b8738,len(image),image);c.setg(31,idx);c.setg(24,self-0x7e0);c.setg(30,R.M.S.AUX);c.setg(27,16);c.setg(28,32);c.setg(29,48);start=0x822b87d8;end=0x822b8810;c.mem.wr(end,bytes.fromhex('4e800020'))
  c.run(start);results.append((c.mem.rd(self,0x2000),c.mem.rd(R.M.S.AUX,64)))
 if results[0]!=results[1]:errors.append(['setter',i])
d={'getter_cases_each':128,'setter_cases':128,'mismatch_count':len(errors),'mismatches':errors[:5],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'whole actual two const wheel getters vs ARTIST 822A3220/31B8 and actual setter vs inlined ARTIST 822B87D8..880C, valid indices, arbitrary raw 64-byte matrices and full poisoned RenderParams; setter by-value pointer r5, software VMX, assertion and caller lifecycle excluded'};(out/'renderparams-wheel-accessors-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
