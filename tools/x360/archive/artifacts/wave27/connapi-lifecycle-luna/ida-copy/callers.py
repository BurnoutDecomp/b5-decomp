import ida_funcs, idautils, idc
addrs = (0x82B150D8,0x82B15168,0x82B15200,0x82B152C8,0x82B153F0,0x82B15EF0)
for target in addrs:
    print('TARGET %08X %s' % (target, idc.get_func_name(target)))
    refs = list(idautils.CodeRefsTo(target, False))
    print('XREF_COUNT %d' % len(refs))
    for ref in refs:
        f = ida_funcs.get_func(ref)
        print('CALLER %08X %s CALL %08X' % (f.start_ea if f else 0, idc.get_func_name(f.start_ea) if f else '?', ref))
        ea = ref
        for i in range(9):
            print('  %08X %s' % (ea, idc.generate_disasm_line(ea,0)))
            ea = idc.next_head(ea, idc.BADADDR)
            if ea == idc.BADADDR:
                break
