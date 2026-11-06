import ida_hexrays, ida_funcs, ida_lines, idautils, idc, json, traceback
out=r'D:\Reverse\BP-X360-Experimental-Artifacts\wave17\network-providers-terra\ida-localize\targeted.json'
results={}
try:
    ready=ida_hexrays.init_hexrays_plugin()
    for ea in [0x82B35328,0x82B34D10,0x82B34EA0,0x82B35038,0x82B34C50,0x82B351C0]:
        row={'address':hex(ea),'hexrays_ready':ready}
        try:
            fn=ida_funcs.get_func(ea)
            row['start']=hex(fn.start_ea) if fn else None
            row['end']=hex(fn.end_ea) if fn else None
            row['name']=idc.get_func_name(ea)
            if fn:
                cf=ida_hexrays.decompile(ea)
                row['pseudocode']=str(cf) if cf else None
                row['assembly']=[{'address':hex(x),'text':ida_lines.tag_remove(idc.generate_disasm_line(x,0) or '')} for x in idautils.FuncItems(fn.start_ea)]
        except Exception:
            row['error']=traceback.format_exc()
        results[hex(ea)]=row
except Exception:
    results['fatal']=traceback.format_exc()
with open(out,'w',encoding='utf-8') as f:json.dump(results,f,indent=2)
idc.qexit(0)
