import ida_bytes,ida_name,ida_pro
p='D:/Reverse/BP-X360-Experimental-Artifacts/wave75/attrib-vault-terra/vtable.txt'
with open(p,'w') as f:
 for base in [0x820A1400]:
  for i in range(16):
   a=base+4*i;v=ida_bytes.get_dword(a);f.write('%08X %08X %s\n'%(a,v,ida_name.get_name(v)))
ida_pro.qexit(0)
