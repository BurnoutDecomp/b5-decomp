import ida_auto,ida_lines,ida_funcs,ida_pro,ida_hexrays
ida_auto.auto_wait();f=open('D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r95-apt-push-terra/artist-view-addfont.txt','w');ea=0x82858c10;fn=ida_funcs.get_func(ea);f.write('end %08x\n'%(fn.end_ea if fn else 0));
try:f.write(str(ida_hexrays.decompile(ea))+'\n')
except Exception as e:f.write(str(e)+'\n')
for x in range(ea,min(ea+0x300,fn.end_ea if fn else ea+0x200),4):f.write('%08x %s\n'%(x,ida_lines.tag_remove(ida_lines.generate_disasm_line(x,0) or '')))
f.close();ida_pro.qexit(0)
