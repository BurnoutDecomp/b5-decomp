import idaapi
import ida_bytes
import ida_funcs
import ida_name
import idautils
import idc
import json
import os

idaapi.auto_wait()
functions = []
for ea in (0x8285EE68, 0x8285FFE0, 0x82853E68, 0x828495B8):
    f = ida_funcs.get_func(ea)
    entry = {"requested_ea": "0x%08X" % ea, "name": ida_name.get_name(ea)}
    if not f:
        entry["error"] = "no function at requested address"
        functions.append(entry)
        continue
    entry["start"] = "0x%08X" % f.start_ea
    entry["end"] = "0x%08X" % f.end_ea
    entry["name"] = ida_name.get_name(f.start_ea)
    entry["pseudocode"] = None
    if idaapi.init_hexrays_plugin():
        try:
            cfunc = idaapi.decompile(f.start_ea)
            entry["pseudocode"] = str(cfunc) if cfunc else None
        except Exception as exc:
            entry["decompile_error"] = repr(exc)
    asm = []
    for head in idautils.Heads(f.start_ea, f.end_ea):
        size = ida_bytes.get_item_size(head)
        raw = ida_bytes.get_bytes(head, size) or b""
        asm.append({
            "ea": "0x%08X" % head,
            "size": size,
            "bytes": raw.hex(),
            "disasm": idc.generate_disasm_line(head, 0) or "",
            "xrefs_from": ["0x%08X" % x.to for x in idautils.XrefsFrom(head, 0)]
        })
    entry["assembly"] = asm
    entry["xrefs_to"] = [
        {"from": "0x%08X" % x.frm, "type": int(x.type), "name": ida_name.get_name(x.frm)}
        for x in idautils.XrefsTo(f.start_ea, 0)
    ]
    functions.append(entry)

# Also collect code xrefs to the user-function table global and candidate slot addresses.
for addr in (0x8324E818, 0x8324E870, 0x8324E87C):
    functions.append({
        "global": "0x%08X" % addr,
        "name": ida_name.get_name(addr),
        "xrefs_to": [
            {"from": "0x%08X" % x.frm, "type": int(x.type), "name": ida_name.get_name(x.frm)}
            for x in idautils.XrefsTo(addr, 0)
        ]
    })

out = r"D:\Reverse\BP-X360-Experimental-Artifacts\wave17\apt-providers-luna\native-free-callbacks\ida-db\apt_callback_slot_evidence.json"
with open(out, "w") as f:
    json.dump(functions, f, indent=2)
idc.qexit(0)
