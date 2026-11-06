from pathlib import Path
import json
R=Path(r'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX');O=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/game-module-native-sol')
for a in [0x82532718,0x8253f4e8,0x8253ab80]:
 d=json.loads((R/('0x%08X.json'%a)).read_text());s=d['assembly'];(O/('%08X.asm'%a)).write_text(s+'\n')
 print(hex(a));print('\n'.join(x for x in s.splitlines()if '0x72' in x or '0x73' in x or '0x71F' in x))
