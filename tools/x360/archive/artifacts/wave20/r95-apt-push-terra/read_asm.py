import ida_auto,ida_lines,ida_pro
ida_auto.auto_wait()
f=open('D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r95-apt-push-terra/artist-standalone.txt','w')
for ea in range(0x82af4128,0x82af41a8,4):f.write('%08x %s\n'%(ea,ida_lines.generate_disasm_line(ea,0)))
f.close();ida_pro.qexit(0)
