import idautils,idc,ida_funcs,ida_pro
with open(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave25\xnfriends-terra\hlb-funcs.txt','w') as f:
 for a in idautils.Functions(0x82B27C00,0x82B28800): f.write('%08x %s end %08x\n'%(a,idc.get_func_name(a),ida_funcs.get_func(a).end_ea))
ida_pro.qexit(0)
