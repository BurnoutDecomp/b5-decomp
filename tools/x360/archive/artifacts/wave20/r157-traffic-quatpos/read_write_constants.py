import ida_bytes,ida_pro,json
from pathlib import Path
p=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r157-traffic-quatpos'); addresses=(0x8209D7CC,0x8327F100,0x8327F120,0x8327F0F0,0x82CDA360,0x82CDA370,0x82CDA380,0x82CDA390,0x82CDA3A0,0x82CDA3B0,0x82CDA3E0,0x82CDA3F0,0x82CDA420,0x82CDA430);d={hex(a):ida_bytes.get_bytes(a,16).hex() for a in addresses};(p/'artist-write-constants.json').write_text(json.dumps(d,indent=2));ida_pro.qexit(0)
