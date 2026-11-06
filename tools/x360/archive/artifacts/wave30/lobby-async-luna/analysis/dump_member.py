import idaapi, idc, ida_funcs, ida_hexrays, idautils, ida_lines, ida_auto
out_path = r'D:\Reverse\BP-X360-Experimental-Artifacts\wave30\lobby-async-luna\analysis\member-decompiled.txt'
addresses = [0x82B342D0]
ida_auto.auto_wait()
try: ida_hexrays.init_hexrays_plugin()
except Exception: pass
with open(out_path,'w',encoding='utf-8') as out:
    for ea in addresses:
        fn=ida_funcs.get_func(ea)
        out.write('\n===== 0x%08X %s =====\n' % (ea, idc.get_name(ea)))
        if fn is None:
            out.write('NO FUNCTION\n'); continue
        out.write('range=0x%08X..0x%08X\n' % (fn.start_ea, fn.end_ea))
        try: out.write('PSEUDOCODE:\n'+str(ida_hexrays.decompile(ea))+'\n')
        except Exception as e: out.write('DECOMP ERROR: %r\n' % e)
        out.write('ASSEMBLY:\n')
        for h in idautils.Heads(fn.start_ea,fn.end_ea):
            out.write('0x%08X  %s\n' % (h,ida_lines.tag_remove(idc.generate_disasm_line(h,0) or '')))
        out.write('END\n')
idaapi.qexit(0)
