import idc,ida_funcs,ida_pro,struct
h=r'D:\Reverse\BP-X360-Experimental-Artifacts\wave37\snd-panning-terra'
f=open(h+r'\sin-cos-raw.txt','w')
for x in (0x82B73DF0,0x82B73E78):
 fn=ida_funcs.get_func(x);f.write('FUNCTION %08x start %s end %s\n'%(x,hex(fn.start_ea) if fn else 'none',hex(fn.end_ea) if fn else 'none'));end=fn.end_ea if fn else x+0x88
 for ea in range(x,end,4):
  raw=idc.get_bytes(ea,4);line=idc.generate_disasm_line(ea,0);f.write('%08x %s %s\n'%(ea,raw.hex(),line))
for ea in (0x821565b4,0x820ad9b0,0x82001cc0,0x820037c8,0x82004928,0x82001da0,0x82010c20,0x82047b38):
 b=idc.get_bytes(ea,4);f.write('FLOAT %08x %s %r\n'%(ea,b.hex(),struct.unpack('>f',b)[0]))
f.close();ida_pro.qexit(0)
