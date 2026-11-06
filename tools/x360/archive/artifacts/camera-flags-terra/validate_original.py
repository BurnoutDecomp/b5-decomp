from pathlib import Path
import json,random,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');s=(P/'scratch/parallel-native/native_render_states/validate_hierarchy.py').read_text(encoding='utf8');s=s[:s.index('\nrows=')]+s[s.index('\ndef data_symbol'):s.index('\n# Decode')];__file__=str(P/'scratch/parallel-native/native_render_states/validate_hierarchy.py');exec(compile(s,__file__,'exec'))
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/camera-flags-terra');obj=out/'ICECamera.obj';co=Coff(obj,'?SetCameraMatrix@ICECamera@ICE@@');r=random.Random(0x82531ac8)
for i in range(128):
 before=r.randbytes(0x180);matrix=r.randbytes(64);results=[]
 for native in (False,True):
  c=co.cpu();calls=[];c.mem.wr(M.S.SELF,before);c.mem.wr(M.S.SELF+0x400,matrix);c.setg(3,M.S.SELF);c.setg(4,M.S.SELF+0x400)
  def validate(cpu):
   addr=cpu.g(3)&0xffffffff;calls.append((addr,cpu.mem.rd(addr,64)));cpu.setg(3,addr)
  c.hooks[0x8220a850]=validate
  for a,n in co.unresolved.items():
   if n.startswith('?ValidateTransformWithDebugInfo@'):c.hooks[a]=validate
  c.run(co.entry if native else 0x82531ac8);results.append((calls,c.mem.rd(M.S.SELF,0x180),c.mem.rd(M.S.SELF+0x400,64),c.g(3)&0xffffffff))
 assert results[0]==results[1],i
report={'state':'pass','cases':128,'object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Original/compiled SetCameraMatrix full camera bytes and full source matrix, original call argument/order/return with ValidateTransform controlled; randomized 64-bit flag patterns. No gameplay or device claim.'};(out/'original-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
