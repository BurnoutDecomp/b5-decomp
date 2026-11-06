import json
from pathlib import Path
root=Path(r'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')
for addr in ('0x828E7618','0x8282CDF0','0x8282CEA0'):
 d=json.loads((root/(addr+'.json')).read_text(encoding='utf-8'))
 print('---',addr,d.get('name'),d.get('prototype'))
 print('ASM_START')
 print('\n'.join(d.get('assembly','').splitlines()[:55]))
 print('ASM_OPS')
 for l in d.get('assembly','').splitlines():
  if any(k in l for k in ('fmadd','fnmsub')): print(l)
 print('ASM_END',d.get('assembly','').splitlines()[-8:])
