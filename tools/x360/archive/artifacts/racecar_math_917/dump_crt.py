import idc,ida_auto
from pathlib import Path
ida_auto.auto_wait()
a=0x82C5C480
rows=[]
while a<0x82C5C520:
 rows.append('%08X %s'%(a,idc.generate_disasm_line(a,0)))
 a+=4
Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_math_917/artist-crt-disasm.txt').write_text('\n'.join(rows))
idc.qexit(0)
