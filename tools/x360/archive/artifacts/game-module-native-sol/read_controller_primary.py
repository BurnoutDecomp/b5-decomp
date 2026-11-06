from pathlib import Path
import json,sys
sys.path.insert(0,r'D:/Reverse/BP-Decomp_Workflow/tools/re');import x360rd,ppcdis
OUT=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/game-module-native-sol');ROOT=Path(r'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')
for a,n in [(0x827e13b8,190),(0x8253da30,100),(0x82532120,35)]:
 p=ROOT/('0x%08X.json'%a)
 if p.exists():
  d=json.loads(p.read_text());s=d['assembly'];(OUT/('%08X.asm'%a)).write_text(s+'\n');print('PRIMARY',hex(a),len(s.splitlines()));print(s[:11000])
 else:
  s='\n'.join(ppcdis.disasm(a,n));(OUT/('%08X.raw.asm'%a)).write_text(s+'\n');print('RAW',hex(a));print(s[:8500])
