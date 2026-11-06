from pathlib import Path
import sys,struct,json
sys.path.insert(0,r'D:/Reverse/BP-Decomp_Workflow/tools/re');import x360rd,ppcdis
OUT=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/eventinfo-constants-sol')
print('\n'.join(ppcdis.disasm(0x82c503b0,28)))
for a in [0x82001c98,0x82001cc0,0x820054c8,0x820047b8,0x8204c5b4,0x82054360,0x82054364,0x8201ecc8,0x820138dc]:print(hex(a),hex(x360rd.u32(a)),x360rd.f32(a))
# Initializer pointer table lives in the image's data. Search only its known
# startup-table neighborhood, retaining every exact pointer occurrence.
lo,hi=0x82cd0000,0x82cd5000;raw=x360rd.rd(lo,hi-lo)
for target in [0x82c503d8,0x82c503dc,0x82c50418,0x82c50458,0x82c50490,0x82c504c8,0x82c50508,0x82c50830]:
 b=struct.pack('>I',target);off=0;found=[]
 while True:
  off=raw.find(b,off)
  if off<0:break
  if off%4==0:found.append(hex(lo+off))
  off+=1
 print('crt',hex(target),found)
(OUT/'CRT-pointer-neighborhood.raw.bin').write_bytes(raw)
