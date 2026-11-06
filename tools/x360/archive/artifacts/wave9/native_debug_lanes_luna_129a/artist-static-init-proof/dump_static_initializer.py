import json
from pathlib import Path
import ida_auto, ida_bytes, ida_funcs, ida_ua, idc

start = 0x82C6C3E8
end = 0x82C6C494
static_address = 0x830112D0
ida_auto.auto_wait()
fn = ida_funcs.get_func(start)
if fn is None:
    ida_funcs.add_func(start, end)
    ida_auto.auto_wait()
rows = []
ea = start
while ea < end:
    insn = ida_ua.insn_t()
    size = ida_ua.decode_insn(insn, ea)
    if not size:
        rows.append({"address": "0x%08X" % ea, "decode_failed": True,
                     "bytes": (ida_bytes.get_bytes(ea, 4) or b"").hex().upper()})
        ea += 4
        continue
    raw = ida_bytes.get_bytes(ea, size) or b""
    rows.append({"address": "0x%08X" % ea, "size": size, "bytes": raw.hex().upper(),
                 "mnemonic": idc.print_insn_mnem(ea),
                 "operands": [idc.print_operand(ea, i) for i in range(6) if idc.print_operand(ea, i)],
                 "disassembly": idc.generate_disasm_line(ea, 0) or ""})
    ea += size
fn = ida_funcs.get_func(start)
mem = ida_bytes.get_bytes(static_address, 64)
result = {
    "input_database": idc.get_input_file_path(),
    "range_start": "0x%08X" % start,
    "range_end_exclusive": "0x%08X" % end,
    "function_start": "0x%08X" % fn.start_ea if fn else None,
    "function_end": "0x%08X" % fn.end_ea if fn else None,
    "function_name": idc.get_func_name(start),
    "runtime_static_address": "0x%08X" % static_address,
    "static_bytes_in_image_before_ctor": mem.hex().upper() if mem else None,
    "instructions": rows,
}
out = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave9\native_debug_lanes_luna_129a\artist-static-init-proof\static_initializer.json")
out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
print(json.dumps({"output": str(out), "instructions": len(rows), "function_start": result["function_start"],
                  "function_end": result["function_end"], "function_name": result["function_name"],
                  "static_bytes_in_image_before_ctor": result["static_bytes_in_image_before_ctor"]}, indent=2))
idc.qexit(0)