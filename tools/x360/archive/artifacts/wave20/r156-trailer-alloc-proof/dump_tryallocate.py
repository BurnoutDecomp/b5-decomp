import os
import idaapi
import idautils
import idc
try:
    import ida_hexrays
except Exception:
    ida_hexrays = None

root = os.path.dirname(os.path.abspath(__file__))
ea = 0x827234B0
f = idaapi.get_func(ea)
out_path = os.path.join(root, 'TryAllocateTrailerId.raw.txt')
with open(out_path, 'w', encoding='utf-8') as out:
    if f is None:
        out.write('NO_FUNCTION at 0x827234B0\n')
    else:
        out.write('FUNCTION 0x%08X..0x%08X\n' % (f.start_ea, f.end_ea))
        out.write('NAME %s\n' % idc.get_func_name(f.start_ea))
        for insn_ea in idautils.FuncItems(f.start_ea):
            line = idc.generate_disasm_line(insn_ea, 0) or ''
            out.write('0x%08X  %s\n' % (insn_ea, line))
        if ida_hexrays is not None:
            try:
                cfunc = ida_hexrays.decompile(f.start_ea)
                if cfunc:
                    out.write('\nDECOMPILED\n%s\n' % str(cfunc))
            except Exception as exc:
                out.write('\nDECOMPILE_ERROR %s\n' % exc)
idc.qexit(0)
