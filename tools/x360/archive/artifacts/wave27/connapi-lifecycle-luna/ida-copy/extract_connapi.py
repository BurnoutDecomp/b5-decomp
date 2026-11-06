import ida_funcs, ida_hexrays, ida_lines, idautils, idc

ea = 0x82B14608
f = ida_funcs.get_func(ea)
print('R27_CONNAPI_IDA_START %08X' % ea)
if f is None:
    print('NO_FUNCTION_AT_ADDRESS')
else:
    print('FUNCTION %08X-%08X %s' % (f.start_ea, f.end_ea, idc.get_func_name(f.start_ea)))
    try:
        cfunc = ida_hexrays.decompile(ea)
        print('--- PSEUDOCODE ---')
        print(str(cfunc))
    except Exception as exc:
        print('DECOMPILE_ERROR %r' % (exc,))
    print('--- DISASM ---')
    for item in idautils.FuncItems(f.start_ea):
        print('%08X %s' % (item, idc.generate_disasm_line(item, 0)))
print('R27_CONNAPI_IDA_END')
