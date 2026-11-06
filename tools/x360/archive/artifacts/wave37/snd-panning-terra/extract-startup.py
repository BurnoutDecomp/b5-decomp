import idc,ida_pro,idautils,struct
f=open(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave37\snd-panning-terra\startup-data.txt','w')
for ea,n in [(0x82f88188,24),(0x832776e8,24),(0x832778c4,4),(0x82f883cc,4),(0x8327786c,4),(0x8327786a,1),(0x83277865,1),(0x83277866,1),(0x83277624,24),(0x832778d4,24),(0x832778ec,24),(0x83277904,24)]:
 b=idc.get_bytes(ea,n);f.write('%08x %s\n'%(ea,b.hex()));
 for xr in idautils.XrefsTo(ea):f.write('  %08x %s\n'%(xr.frm,idc.get_func_name(xr.frm)))
f.close();ida_pro.qexit(0)
