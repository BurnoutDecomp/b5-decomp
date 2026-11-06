from pathlib import Path
import json,struct,random
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');s=(P/'scratch/parallel-native/native_render_states/validate_hierarchy.py').read_text(encoding='utf8');s=s[:s.index('\nrows=')]+s[s.index('\ndef data_symbol'):s.index('\n# Decode')];__file__=str(P/'scratch/parallel-native/native_render_states/validate_hierarchy.py');exec(compile(s,__file__,'exec'))
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/device-storage-candidate');co=Coff(out/'device.obj','?ShowPixelBuffer@Device@renderengine');rng=random.Random(0x61850)
for case in range(64):
 c=co.cpu();c.mem.add(0x82f87b00,0x100,bytes(0x100));c.mem.add(0x83271000,0x1000,rng.randbytes(0x1000));before=c.mem.rd(0x83271570,0xa0);calls=[]
 def memset(cpu):
  dst=cpu.g(3)&0xffffffff;val=cpu.g(4)&0xff;size=cpu.g(5)&0xffffffff;calls.append((dst,val,size));cpu.mem.wr(dst,bytes([val])*size);cpu.setg(3,dst)
 def params(cpu):calls.append(('parameters',cpu.g(3)&0xffffffff,cpu.g(4)&0xffffffff))
 c.hooks[0x82c0b180]=memset;c.hooks[0x82b61540]=params;c.run(0x82b61850)
 after=c.mem.rd(0x83271570,0xa0)
 assert after[:16]==before[:16] and after[16:144]==bytes(128)
 assert calls==[(0x83271628,0,128),(0x83271100,0,64),(0x83271140,0,64),(0x83271580,0,128),('parameters',0x82f87b00,6)],calls
report={'state':'pass','cases':64,'scope':'Original Device::Initialize only: 128-byte front storage reset and preceding adjacent 16 bytes; the following bytes include other globals written independently. This is NOT a compiled candidate reset comparison: Device::Initialize remains absent.'};(out/'original-reset-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
