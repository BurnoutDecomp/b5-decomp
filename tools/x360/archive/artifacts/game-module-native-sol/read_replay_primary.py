from pathlib import Path
import sys,json
sys.path.insert(0,r'D:/Reverse/BP-Decomp_Workflow/tools/re');import x360rd,ppcdis
ROOT=Path(r'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX');OUT=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/game-module-native-sol')
for ea in [0x827e2020,0x8225bd28,0x8225c298,0x8253b0e8,0x8252ddc8]:
 p=ROOT/('0x%08X.json'%ea)
 if p.exists():
  d=json.loads(p.read_text());(OUT/('%08X.asm'%ea)).write_text(d['assembly']+'\n')
  print(hex(ea),'asm count',len(d['assembly'].splitlines()))
  if ea==0x827e2020:print(d['assembly'])
for ea in [0x8200a560,0x8200a150,0x82008d38]:
 print('VTABLE',hex(ea),[(hex(x360rd.u32(ea+i)),ppcdis.sym(x360rd.u32(ea+i)))for i in range(0,40,4)])
