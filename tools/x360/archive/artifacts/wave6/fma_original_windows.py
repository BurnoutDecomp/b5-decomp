import json
from pathlib import Path
root=Path(r'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')
def show(addr, spans):
 p=root/(addr+'.json')
 if not p.is_file(): print('MISSING',addr); return
 d=json.loads(p.read_text(encoding='utf-8'))
 print('---',addr,d.get('name'),d.get('prototype'))
 asm=d.get('assembly','').splitlines()
 for lo,hi in spans:
  for line in asm:
   if line.startswith('0x'):
    try:a=int(line.split()[0],16)
    except:continue
    if lo<=a<=hi:print(line)
 for key in ('xrefs_to','xrefs_from'):
  refs=d.get(key,[])
  print(key,len(refs),[(x.get('address'),x.get('name')) for x in refs[:8]])
show('0x828E7618',[(0x828E7618,0x828E7710)])
show('0x828EFC58',[(0x828EFC58,0x828EFD40)])
show('0x8282CDF0',[(0x8282CDF0,0x8282CE98)])
show('0x8282CEA0',[(0x8282CEF8,0x8282CF88),(0x8282D170,0x8282D188)])
show('0x8282F520',[(0x8282F520,0x8282F600)])
