import json
from pathlib import Path
root=Path(r'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')
for addr in ('0x8282CEA0','0x8282CDF0'):
 d=json.loads((root/(addr+'.json')).read_text(encoding='utf-8'))
 print('---',addr,d.get('name'))
 for line in d.get('assembly','').splitlines():
  if any(x in line for x in ('0x8282CF70','0x8282CF74','0x8282CF78','0x8282CF7C','0x8282CF80','0x8282D178','0x8282D17C','0x8282D180','0x8282D184','0x8282D188','0x8282CE50','0x8282CE54','0x8282CE58','0x8282CE5C')):print(line)
