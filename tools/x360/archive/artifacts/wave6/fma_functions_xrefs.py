import json
from pathlib import Path
root=Path(r'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')
for addr in ('0x828E7618','0x8282CDF0','0x8282CEA0'):
 d=json.loads((root/(addr+'.json')).read_text(encoding='utf-8'))
 print('---',addr,d.get('name'))
 for k in ('xrefs_to','xrefs_from'):
  print(k,len(d.get(k,[])))
  for x in d.get(k,[])[:15]:print(' ',x.get('address'),x.get('name'))
