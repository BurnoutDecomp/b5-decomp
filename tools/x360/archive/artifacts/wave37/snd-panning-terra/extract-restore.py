import idautils,idc,ida_pro
f=open(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave37\snd-panning-terra\restore-owners.txt','w')
for a in (0x832778b0,0x832778b4,0x832778b8,0x832778bc):
 f.write('DATA %08X\n'%a)
 for x in idautils.XrefsTo(a):f.write('%08X %s %s\n'%(x.frm,idc.get_func_name(x.frm),idc.generate_disasm_line(x.frm,0)))
f.close();ida_pro.qexit(0)
