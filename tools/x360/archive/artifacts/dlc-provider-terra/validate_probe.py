from pathlib import Path
import json,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');s=(P/'scratch/parallel-native/native_render_states/validate_hierarchy.py').read_text(encoding='utf8');s=s[:s.index('\nrows=')]+s[s.index('\ndef data_symbol'):s.index('\n# Decode')];__file__=str(P/'scratch/parallel-native/native_render_states/validate_hierarchy.py');exec(compile(s,__file__,'exec'))
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/dlc-provider-terra');obj=out/'dlc_provider_probe.obj';n=0
for name in ['DLCProbeGame','DLCProbeAvailable','DLCProbeEnabled','DLCProbeEnable']:
 co=Coff(obj,name)
 for avail in [0,1]:
  for enabled in [0,1]:
   c=co.cpu();globaladdr=next(a for a,sym in co.unresolved.items() if sym.startswith('?g_DLCFeatureAvailability@'));c.mem.add(globaladdr,192,bytes([avail,enabled])+bytes(190));calls=[]
   for a,sym in co.unresolved.items():
    if sym.startswith('?SetEnabledState@'):c.hooks[a]=lambda cpu:calls.append((cpu.g(3)&0xffffffff,cpu.g(4)&0xffffffff))
   if name == 'DLCProbeEnable':
    pc=co.entry
    for _ in range(3):
     word=c.fetch(pc);pc=c.step(pc,word,pc+4,0)
    word=c.fetch(pc); delta=word & 0x03fffffc
    if delta & 0x02000000:delta-=0x04000000
    target=(pc+delta)&0xffffffff
    assert co.unresolved[target].startswith('?SetEnabledState@')
    calls.append((c.g(3)&0xffffffff,c.g(4)&0xffffffff))
   else:c.run(co.entry)
   if name=='DLCProbeGame':assert c.g(3)&0xffffffff==globaladdr
   elif name=='DLCProbeAvailable':assert c.g(3)==avail
   elif name=='DLCProbeEnabled':assert c.g(3)==enabled
   else:assert calls==[(globaladdr,1)],calls
   n+=1
report={'state':'pass','cases':n,'object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual XDK-compiled direct API probe reads true two flag bytes, forms game receiver at exact feature-global base, and passes true to SetEnabledState. Setter service controlled; actual GUI BootLegal migration is separately owned.'};(out/'probe-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
