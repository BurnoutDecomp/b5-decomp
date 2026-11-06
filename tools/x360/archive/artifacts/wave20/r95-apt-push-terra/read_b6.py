import ida_auto,ida_bytes,ida_lines,ida_pro
ida_auto.auto_wait();f=open('D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r95-apt-push-terra/artist-b6.txt','w');ea=ida_bytes.get_dword(0x82f73068+0xb6*4);f.write('dispatch %08x\n'%ea)
for x in range(ea,ea+0x70,4):f.write('%08x %s\n'%(x,ida_lines.tag_remove(ida_lines.generate_disasm_line(x,0) or '')))
f.close();ida_pro.qexit(0)
