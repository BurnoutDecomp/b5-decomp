import ida_bytes, ida_lines, ida_pro
with open(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave24\hlb-user-index-terra\disasm.txt','w') as f:
 for a,b in [(0x82B281C8,0x82B281D0),(0x82B38FCC,0x82B39020)]:
  for ea in range(a,b,4):
   f.write('%08X %s %s\n' % (ea,ida_bytes.get_bytes(ea,4).hex(),ida_lines.tag_remove(ida_lines.generate_disasm_line(ea,0) or '')))
ida_pro.qexit(0)
