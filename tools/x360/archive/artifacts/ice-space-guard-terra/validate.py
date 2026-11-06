from pathlib import Path
import json,random,struct,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');s=(P/'scratch/parallel-native/native_render_states/validate_hierarchy.py').read_text(encoding='utf8');s=s[:s.index('\nrows=')]+s[s.index('\ndef data_symbol'):s.index('\n# Decode')];__file__=str(P/'scratch/parallel-native/native_render_states/validate_hierarchy.py');exec(compile(s,__file__,'exec'))
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/ice-space-guard-terra');obj=out/'ICECameraSpaceHandler.obj';co=Coff(obj,'?GetTransformToWorld@CameraSpaceHandler@ICE@@');rng=random.Random(0x82531ba0)
for i in range(128):
 matrix=rng.randbytes(64);handle=[1,0x1001,0x78160e11,0x12345000][i%4];results=[]
 for native in (False,True):
  c=co.cpu();calls=[];c.mem.wr(M.S.SELF+0x200,struct.pack('>I',handle));c.mem.wr(M.S.SELF+0x800,matrix);c.mem.wr(M.S.SELF+0x500,bytes([0xA5])*64);c.setg(3,M.S.SELF+0x500);c.setg(4,M.S.SELF);c.setg(5,9)
  def getcamera(cpu):calls.append(('getcamera',cpu.g(3)&0xffffffff));cpu.setg(3,M.S.SELF+0x800)
  def gettransform(cpu):calls.append(('gettransform',cpu.g(3)&0xffffffff));cpu.setg(3,M.S.SELF+0x800)
  c.hooks[0x82212288]=getcamera;c.hooks[data_symbol(co,'?GetProducedCamera@')]=getcamera
  for a,n in co.unresolved.items():
   if n.startswith('?GetTransform@Camera@'):c.hooks[a]=gettransform
  c.run(co.entry if native else 0x82531ba0)
  assert calls==([('getcamera',handle),('gettransform',M.S.SELF+0x800)] if native else [('getcamera',handle)]),calls
  results.append((c.mem.rd(M.S.SELF+0x500,64),c.g(3)&0xffffffff))
 assert results[0]==results[1]==(matrix,M.S.SELF+0x500),i
report={'state':'pass','cases':128,'object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Original and compiled gameplay-space matrix copy, exact handle forwarding and output bytes; native GetProducedCamera/GetTransform and original sub_82212288 are controlled. Odd and low nonnull handles demonstrate guard absence only; controlled handlers do not dereference them. Null assertion and real camera lifecycle not exercised.'};(out/'validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
