import idc,ida_pro
from pathlib import Path
out=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r156-traffic-replay-phase')
with (out/'artist-phase.txt').open('w') as f:
 for start,end in ((0x8274a968,0x8274aa90),(0x827af180,0x827af1d0)):
  for ea in range(start,end,4): f.write('%08X %s\n'%(ea,idc.generate_disasm_line(ea,0) or ''))
ida_pro.qexit(0)
