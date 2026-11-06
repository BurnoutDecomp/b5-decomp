import idc,ida_pro,struct
f=open(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave37\snd-panning-terra\polynomial-floats.txt','w')
for a in (0x82001c94,0x820c87a4,0x82057dd0,0x82156420,0x8215641c,0x82156418,0x82001c98,0x8206c7dc,0x82156434,0x82156430,0x8215642c,0x82156428,0x82156424):
 b=idc.get_bytes(a,4);f.write('%08x %s %r\n'%(a,b.hex(),struct.unpack('>f',b)[0]))
f.close();ida_pro.qexit(0)
