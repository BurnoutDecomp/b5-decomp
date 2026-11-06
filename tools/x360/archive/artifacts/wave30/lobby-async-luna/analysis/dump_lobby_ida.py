import os, idaapi, idc, ida_funcs, ida_hexrays, idautils, ida_lines, ida_auto
out_path = r'D:\Reverse\BP-X360-Experimental-Artifacts\wave30\lobby-async-luna\analysis\lobby-functions-decompiled.txt'
addresses = [0x82B1AF28,0x82B31E00,0x82B31E78,0x82B31EE0,0x82B31F70,0x82B31FE0,0x82B32008,0x82B314A8,0x82B31638,0x82B31760,0x82B31AD8,0x82B31BD0,0x82B31CD0,0x82B32490,0x82B32610,0x82B32630,0x82B32650,0x82B327F0,0x82B32810,0x82B32830,0x82B32850,0x82B32870,0x82B32890,0x82B32910,0x82B32B00,0x82B32D58,0x82B32D78,0x82B32F88,0x82B33120,0x82B33318,0x82B33408,0x82B334B8,0x82B33668,0x82B33710]
ida_auto.auto_wait()
try:
    ida_hexrays.init_hexrays_plugin()
except Exception:
    pass
with open(out_path,'w',encoding='utf-8') as out:
    for ea in addresses:
        fn=ida_funcs.get_func(ea)
        out.write('\n===== 0x%08X %s =====\n' % (ea, idc.get_name(ea)))
        if fn is None:
            out.write('NO FUNCTION\n')
            continue
        out.write('range=0x%08X..0x%08X\n' % (fn.start_ea, fn.end_ea))
        try:
            cf=ida_hexrays.decompile(ea)
            out.write('PSEUDOCODE:\n'+str(cf)+'\n')
        except Exception as e:
            out.write('DECOMP ERROR: %r\n' % (e,))
        out.write('ASSEMBLY:\n')
        for h in idautils.Heads(fn.start_ea,fn.end_ea):
            line=idc.generate_disasm_line(h,0)
            out.write('0x%08X  %s\n' % (h,ida_lines.tag_remove(line or '')))
        out.write('END\n')
idaapi.qexit(0)
