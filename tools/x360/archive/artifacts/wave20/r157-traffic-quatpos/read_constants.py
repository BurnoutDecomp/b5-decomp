import ida_bytes,ida_name,ida_pro,json
from pathlib import Path
out=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r157-traffic-quatpos');names=['rw__math__vpu__detail__gSqrt2s','flt_8206CEB4'];result={}
for address in (0x82CDA3D0,0x82CDA450,0x82CDA410,0x8206CEB4):result[hex(address)]=ida_bytes.get_bytes(address,16 if address!=0x8206CEB4 else 4).hex()
for n in names:
 a=ida_name.get_name_ea(0xffffffffffffffff,n); result[n]={'ea':hex(a),'bytes':None if a==0xffffffffffffffff else ida_bytes.get_bytes(a,16).hex()}
(out/'artist-constants.json').write_text(json.dumps(result,indent=2));ida_pro.qexit(0)
