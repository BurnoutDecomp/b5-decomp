import idc,ida_pro
f=open(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r33-batch\wake-disassembly.txt','w')
for ea in range(0x82bc9b48,0x82bc9b60,4):
 f.write('%08X %s %s\n' % (ea, idc.get_bytes(ea,4).hex(), idc.generate_disasm_line(ea,0)))
f.close(); ida_pro.qexit(0)
