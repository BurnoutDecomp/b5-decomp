import ida_auto,ida_funcs,ida_hexrays,ida_pro,idc
path=r'D:\Reverse\BP-X360-Experimental-Artifacts\wave36\ai-debug-render-terra\nearby-ida-decompile.txt'
ida_auto.auto_wait()
ea=0x82773D28
f=ida_funcs.get_func(ea)
if not f:
 ida_funcs.add_func(ea,0x827743A0)
if ida_hexrays.init_hexrays_plugin():
 try:
  c=ida_hexrays.decompile(ea)
  with open(path,'w',encoding='utf-8') as out: out.write(str(c) if c else 'decompile returned None')
 except Exception as ex:
  with open(path,'w',encoding='utf-8') as out: out.write('ERROR '+str(ex))
else:
 with open(path,'w',encoding='utf-8') as out: out.write('hexrays unavailable')
ida_pro.qexit(0)
