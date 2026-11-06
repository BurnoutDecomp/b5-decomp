import idaapi,ida_bytes,ida_name,json
out={}
for base in [0x82fa76fc,0x82fa76d4]:
 a=[]
 for i in range(10):
  p=ida_bytes.get_dword(base+4*i);a.append({'offset':i*4,'value':hex(p),'target':ida_name.get_name(p)})
 out[hex(base)]=a
open('D:/Reverse/BP-X360-Experimental-Artifacts/wave40/xhv-sdk-terra/artist-vtables.json','w').write(json.dumps(out,indent=2));idaapi.qexit(0)
