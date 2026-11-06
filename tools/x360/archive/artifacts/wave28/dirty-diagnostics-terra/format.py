import ida_bytes,idc,ida_pro
with open(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave28\dirty-diagnostics-terra\format-near.txt','w') as f:
 for a in range(0x82f81858,0x82f81890):
  raw=idc.get_strlit_contents(a,-1,idc.STRTYPE_C)
  if raw and (raw.startswith(b'%s/') or raw.startswith(b'0x%')): f.write('%08X %r\n'%(a,raw))
ida_pro.qexit(0)
