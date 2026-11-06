import ida_auto, ida_funcs, ida_hexrays, ida_idaapi, ida_lines, idc, pathlib
ida_auto.auto_wait()
out = pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave19\realmc-finish-luna\ida-native-dump.txt')
addrs = [0x82B55980, 0x82B543F0, 0x82B546C0, 0x82B54648, 0x82B545D0, 0x82B54AC0]
with out.open('w', encoding='utf-8') as f:
    for ea in addrs:
        f.write(f'### {ea:08X} {idc.get_name(ea)}\n')
        f.write(f'prototype={idc.get_type(ea)}\n')
        fn = ida_funcs.get_func(ea)
        if fn:
            f.write(f'range={fn.start_ea:08X}..{fn.end_ea:08X}\n')
            cur = fn.start_ea
            while cur < fn.end_ea:
                line = idc.generate_disasm_line(cur, 0)
                if line:
                    f.write(f'{cur:08X} {ida_lines.tag_remove(line)}\n')
                cur = idc.next_head(cur, fn.end_ea)
        try:
            cfunc = ida_hexrays.decompile(ea)
            f.write('PSEUDOCODE:\n' + str(cfunc) + '\n')
        except Exception as exc:
            f.write(f'decompile-error={exc}\n')
        f.write('\n')
    f.write('### function names 82B54000..82B55000\n')
    for ea in idc.Functions(0x82B54000, 0x82B55000):
        n = idc.get_name(ea)
        if n:
            f.write(f'{ea:08X} {n} {idc.get_type(ea)}\n')

