import json
from pathlib import Path
root=Path(r'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')
for addr in ('0x822230D8','0x8227E698'):
 d=json.loads((root/(addr+'.json')).read_text(encoding='utf-8'))
 print('---',addr,d.get('name'),d.get('prototype'))
 for line in d.get('assembly','').splitlines():
  if any(x in line.lower() for x in ('vmaddfp','vnmsubfp','vmulfp','fmadd','fnmsub')):print(line)
