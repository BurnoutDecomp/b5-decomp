import ida_bytes, ida_lines, ida_pro
with open(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave18\hlb-bounds-terra\disasm.txt','w') as f:
 for a,b in [(0x82B22508,0x82B22530),(0x82B281D0,0x82B28288)]:
  for ea in range(a,b,4):
   f.write('%08X %s %s\n' % (ea,ida_bytes.get_bytes(ea,4).hex(),ida_lines.tag_remove(ida_lines.generate_disasm_line(ea,0) or '')))
ida_pro.qexit(0)
