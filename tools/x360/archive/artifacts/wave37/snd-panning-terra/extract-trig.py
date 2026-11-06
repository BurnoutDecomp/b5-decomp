import json,idc,ida_pro,ida_funcs
out={}
for a in (0x82b73df0,0x82b73e78):
 f=ida_funcs.get_func(a);out[hex(a)]=idc.get_bytes(a,f.end_ea-a).hex()
for a in (0x82001c94,0x820c87a4,0x82057dd0,0x82156420,0x8215641c,0x82156418,0x82001c98,0x8206c7dc,0x82156434,0x82156430,0x8215642c,0x82156428,0x82156424,0x82001da0):out[hex(a)]=idc.get_bytes(a,4).hex()
open(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave37\snd-panning-terra\trig-artist-bytes.json','w').write(json.dumps(out,indent=2)+'\n');ida_pro.qexit(0)
