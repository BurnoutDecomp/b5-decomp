from pathlib import Path
import json,struct,random,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');s=(P/'scratch/parallel-native/native_render_states/validate_hierarchy.py').read_text(encoding='utf8');s=s[:s.index('\nrows=')]+s[s.index('\ndef data_symbol'):s.index('\n# Decode')];__file__=str(P/'scratch/parallel-native/native_render_states/validate_hierarchy.py');exec(compile(s,__file__,'exec'))
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/dlc-provider-terra');obj=out/'BrnDLCManager.obj';rng=random.Random(0x82662c48);rows=[]
for prefix,original,ncases in [('?Construct@DLCFeatureAvailability@BrnResource@@',0x82662c48,256),('?SetEnabledState@DLCBeatTheTeamGame@BrnResource@@',0x82472ca8,256)]:
 co=Coff(obj,prefix)
 for case in range(ncases):
  data=bytearray(rng.randbytes(192));data[0]=case%2;enabled=(case//2)%2;results=[]
  for compiled in (False,True):
   c=co.cpu();calls=[];c.mem.wr(M.S.SELF,data);c.setg(3,M.S.SELF);c.setg(4,enabled)
   def memset(cpu):
    d=cpu.g(3)&0xffffffff;v=cpu.g(4)&255;n=cpu.g(5)&0xffffffff;cpu.mem.wr(d,bytes([v])*n);cpu.setg(3,d)
   def readstr(addr):
    outb=bytearray()
    for j in range(256):
     x=cpu_mem.mem.rd(addr+j,1)[0]
     if x==0:break
     outb.append(x)
    return outb.decode('ascii')
   cpu_mem=c
   def begin(cpu):calls.append(('begin',))
   def fire(cpu):calls.append(('fire',readstr(cpu.g(3)&0xffffffff),readstr(cpu.g(4)&0xffffffff),cpu.g(5)&0xffffffff))
   def end(cpu):calls.append(('end',))
   for a,fn in [(0x82c0b180,memset),(0x82817548,begin),(0x82820810,fire),(0x82817558,end)]:c.hooks[a]=fn
   for a,n in co.unresolved.items():
    if n=='memset':c.hooks[a]=memset
    elif n.startswith('?BeginAssert@'):c.hooks[a]=begin
    elif n.startswith('?FireAssert@'):c.hooks[a]=fire
    elif n.startswith('?EndAssert@'):c.hooks[a]=end
   c.run(co.entry if compiled else original);results.append((calls,c.mem.rd(M.S.SELF,192)))
  assert results[0]==results[1],(prefix,case,results)
 rows.append({'function':prefix,'cases':ncases})
report={'state':'pass','comparisons':rows,'object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Original vs actual compiled table Construct and game SetEnabledState, full 192-byte memory and exact assert call sequence/message/file/line; memset/assert services controlled. Not whole boot or DLC discovery.'};(out/'validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
