import json
from pathlib import Path
root=Path(r'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')
for addr in ('0x828E7618','0x8282CEA0','0x8282CA38','0x8282CDF0'):
 p=root/(addr+'.json')
 if not p.is_file(): continue
 d=json.loads(p.read_text(encoding='utf-8'))
 print('---',addr,d.get('name'),d.get('prototype'))
 print('pseudocode:',d.get('pseudocode','')[:1300])
 for line in d.get('assembly','').splitlines():
  if any(k in line for k in ('fmadd','fmsub','fnmadd','fnmsub','fdiv','fsubs','fadds')): print(line)
