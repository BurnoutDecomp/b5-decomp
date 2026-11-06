from pathlib import Path
import json,random,struct,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');s=(P/'scratch/parallel-native/native_render_states/validate_hierarchy.py').read_text(encoding='utf8');s=s[:s.index('\nrows=')]+s[s.index('\ndef data_symbol'):s.index('\n# Decode')];__file__=str(P/'scratch/parallel-native/native_render_states/validate_hierarchy.py');exec(compile(s,__file__,'exec'))
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/world-ai-lanes-terra');obj=out/'BrnAIBoundaryLine.obj';co=Coff(obj,'?GetInterp@BoundaryLine@BrnAI@@');rng=random.Random(0x82676c78)
for i in range(64):
 vals=[rng.uniform(-1000,1000) for _ in range(4)];t=rng.uniform(-4,4);result=[]
 for native in (False,True):
  c=co.cpu();c.mem.wr(M.S.SELF,struct.pack('>4f',*vals));c.mem.wr(M.S.SELF+0x100,bytes([0xA5])*32);c.setg(3,M.S.SELF if native else M.S.SELF+0x100);c.setg(4,M.S.SELF+0x100 if native else M.S.SELF);c.setf(1,t);c.run(co.entry if native else 0x82676c78);result.append(c.mem.rd(M.S.SELF+0x100,32))
 assert result[0]==result[1],i
report={'state':'pass','cases':64,'object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Original versus compiled BoundaryLine GetInterp four-lane output and surrounding bytes with explicit differing source ABI register setup; finite random inputs only. Original r3=output/r4=this, current compiled r3=this/r4=output; existing caller ABI remains unresolved. No nonfinite, linking or gameplay claim.'};(out/'boundary-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
