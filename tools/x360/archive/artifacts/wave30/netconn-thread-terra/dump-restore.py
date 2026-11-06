import idc,ida_lines,ida_pro
with open(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave30\netconn-thread-terra\restore-thunk.txt','w') as f:
 for a in range(0x82C08F30,0x82C08F60,4): f.write('%08X %s\n'%(a,ida_lines.tag_remove(idc.generate_disasm_line(a,0) or '')))
ida_pro.qexit(0)
