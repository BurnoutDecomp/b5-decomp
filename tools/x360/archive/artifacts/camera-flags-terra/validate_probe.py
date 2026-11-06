from pathlib import Path
import json,random,struct,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');s=(P/'scratch/parallel-native/native_render_states/validate_hierarchy.py').read_text(encoding='utf8');s=s[:s.index('\nrows=')]+s[s.index('\ndef data_symbol'):s.index('\n# Decode')];__file__=str(P/'scratch/parallel-native/native_render_states/validate_hierarchy.py');exec(compile(s,__file__,'exec'))
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/camera-flags-terra');obj=out/'camera_probe.obj';r=random.Random(0x140);n=0
for name in ['SetValid','ClearValid','ReadValid']:
 co=Coff(obj,name)
 for i in range(256):
  before=bytearray(r.randbytes(0x180));value=struct.unpack_from('>Q',before,0x140)[0];c=co.cpu();c.mem.wr(M.S.SELF,before);c.setg(3,M.S.SELF);c.run(co.entry);after=c.mem.rd(M.S.SELF,len(before))
  if name=='SetValid':struct.pack_into('>Q',before,0x140,value|2)
  if name=='ClearValid':struct.pack_into('>Q',before,0x140,value & ~2)
  assert after==before,(name,i)
  if name=='ReadValid':assert (c.g(3)&0xffffffff)==bool(value&2)
  n+=1
report={'state':'pass','cases':n,'object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual native SDK Camera layout compile assertions, BE full 64-bit alias and BitArray access, 256 randomized set, clear and read cases; all surrounding bytes compared. No gameplay, link or hardware claim.'};(out/'probe-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
