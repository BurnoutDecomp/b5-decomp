import ida_funcs,ida_bytes,json,ida_pro
p='D:/Reverse/BP-X360-Experimental-Artifacts/wave20/iobuffer-byte-match-terra/original.json'
d={}
for a in [0x82204A18,0x82204BB8,0x82204C88,0x82204D50,0x82216E60]:
 f=ida_funcs.get_func(a)
 d[hex(a)]={'size':f.end_ea-a,'bytes':ida_bytes.get_bytes(a,f.end_ea-a).hex()}
open(p,'w').write(json.dumps(d,indent=2));ida_pro.qexit(0)
