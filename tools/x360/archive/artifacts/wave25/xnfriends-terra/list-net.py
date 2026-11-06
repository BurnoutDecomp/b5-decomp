import idautils,idc,ida_pro
with open(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave25\xnfriends-terra\net-symbols.txt','w') as f:
 for a in idautils.Functions():
  n=idc.get_func_name(a)
  if any(x in n for x in ['NetConnStatus','NetTick','XFriendsCreateEnumerator','XEnumerate']): f.write('%08X %s\n'%(a,n))
ida_pro.qexit(0)
