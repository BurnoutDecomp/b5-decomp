import idautils,idc,ida_funcs,ida_hexrays,ida_pro
p=r'D:\Reverse\BP-X360-Experimental-Artifacts\wave26\netconn-terra\socket-api.txt'
with open(p,'w') as f:
 for a in idautils.Functions():
  n=idc.get_func_name(a)
  if n in ('SocketCreate','SocketDestroy','DirtyErrGetName'):
   f.write('%08x %s\n'%(a,n))
   try: f.write(str(ida_hexrays.decompile(a))+'\n')
   except Exception as e: f.write(str(e)+'\n')
ida_pro.qexit(0)
