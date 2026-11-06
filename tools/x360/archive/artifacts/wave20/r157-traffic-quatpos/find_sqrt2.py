import idautils,ida_bytes,ida_pro,json
from pathlib import Path
out=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r157-traffic-quatpos'); arr=[]
for addr,name in idautils.Names():
 if 'gSqrt2s' in name:
  b=ida_bytes.get_bytes(addr,16);arr.append({'name':name,'ea':hex(addr),'bytes':b.hex() if b else None})
(out/'artist-sqrt2.json').write_text(json.dumps(arr,indent=2));ida_pro.qexit(0)
