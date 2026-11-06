import ida_auto, ida_funcs, ida_lines, ida_hexrays, idc, pathlib
ida_auto.auto_wait()
out = pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave19\realmc-finish-luna\ida-callback-dump.txt')
addrs = [0x82B543F0,0x82B54460,0x82B544D8,0x82B54550,0x82B545D0,0x82B54648,0x82B546C0,0x82B54740,0x82B547B8,0x82B54860,0x82B548E0,0x82B54950,0x82B54A08,0x82B54AC0,0x82B54B60]
with out.open('w', encoding='utf-8') as f:
    for ea in addrs:
        fn = ida_funcs.get_func(ea)
        f.write(f'### {ea:08X} {idc.get_name(ea)} {idc.get_type(ea)} range={fn.start_ea:08X}..{fn.end_ea:08X}\n')
        cur = fn.start_ea
        while cur < fn.end_ea:
            line = idc.generate_disasm_line(cur, 0)
            if line:
                f.write(f'{cur:08X} {ida_lines.tag_remove(line)}\n')
            cur = idc.next_head(cur, fn.end_ea)
        try:
            f.write('PSEUDOCODE:\n' + str(ida_hexrays.decompile(ea)) + '\n')
        except Exception as exc:
            f.write(f'decompile-error={exc}\n')
        f.write('\n')
