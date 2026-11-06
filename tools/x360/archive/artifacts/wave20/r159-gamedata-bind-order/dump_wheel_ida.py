import idautils
import ida_funcs
import idc

out_path = r"D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r159-gamedata-bind-order\wheel-art-idb-0x8266D1F8.txt"
start = 0x8266D1F8
f = ida_funcs.get_func(start)
with open(out_path, "w", encoding="utf-8") as out:
    if f is None:
        out.write("no function at 0x8266D1F8\n")
    else:
        out.write("requested=0x8266D1F8\n")
        out.write("function=%s start=0x%08X end=0x%08X\n" %
                  (idc.get_func_name(f.start_ea), f.start_ea, f.end_ea))
        for ea in idautils.FuncItems(f.start_ea):
            line = idc.generate_disasm_line(ea, 0)
            if line:
                out.write("0x%08X  %s\n" % (ea, line))
idc.qexit(0)