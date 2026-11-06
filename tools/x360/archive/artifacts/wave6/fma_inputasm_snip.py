import json
from pathlib import Path
d=json.loads(Path(r'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x828E7618.json').read_text(encoding='utf-8'))
for l in d['assembly'].splitlines():
 if any(a in l for a in ['0x828E768C','0x828E7690','0x828E7694','0x828E7698','0x828E769C','0x828E76A0','0x828E76DC','0x828E76E0','0x828E76E4','0x828E76E8','0x828E76EC','0x828E76F0']):print(l)
