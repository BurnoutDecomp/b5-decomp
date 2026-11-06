import ida_auto,ida_lines,ida_funcs,ida_pro
ida_auto.auto_wait();f=open('D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r95-apt-push-terra/artist-op-54-66.txt','w')
for ea in [0x82aea788,0x82addee8]:
 fn=ida_funcs.get_func(ea);f.write('entry %08x end %08x\n'%(ea,fn.end_ea if fn else 0))
 for x in range(ea,min(ea+0x80,fn.end_ea if fn else ea+0x80),4):f.write('%08x %s\n'%(x,ida_lines.tag_remove(ida_lines.generate_disasm_line(x,0) or '')))
f.close();ida_pro.qexit(0)
