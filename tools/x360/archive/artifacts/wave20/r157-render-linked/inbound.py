exec(open(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r157-render-linked\inspect_pe.py').read().split('results=[]')[0])
import bisect
wanted={a:n for a,n,o in entries if any(x in n for x in ['PreDispatchUpdate@TrafficEntityModule','GenerateDispatchLists@TrafficEntityModule','ReadAsQuatPos@TrafficEntitySerialiser'])}
s=pe.sections[2]; b=pe.get_data(s.VirtualAddress,s.Misc_VirtualSize); start=pe.OPTIONAL_HEADER.ImageBase+s.VirtualAddress
for off in range(0,len(b)-3,4):
 w=int.from_bytes(b[off:off+4],'big')
 if (w & 0xfc000003)!=0x48000001: continue
 pc=start+off; imm=w&0x03fffffc; imm=imm-(1<<26) if imm & (1<<25) else imm; target=pc+imm
 if target in wanted:
  idx=bisect.bisect_right(addresses,pc)-1; owner=names.get(addresses[idx],['?'])[0]
  print(hex(pc),hex(target),wanted[target].split('@')[0],owner)
