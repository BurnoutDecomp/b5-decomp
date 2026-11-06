import json
import ida_bytes, ida_funcs, ida_name, ida_lines, idautils, idc
from pathlib import Path

result = {}
for ea in (0x82f2423c, 0x82f2423d, 0x82faedd4, 0x82faedd8, 0x82faee00, 0x82f87b34):
    result[hex(ea)] = {
        'bytes': ida_bytes.get_bytes(ea, 4).hex(),
        'xrefs': [
            (hex(x.frm), ida_funcs.get_func_name(x.frm),
            ida_lines.tag_remove(idc.generate_disasm_line(x.frm, 0))) for x in idautils.XrefsTo(ea)
        ],
    }
for start in (0x827e8e50,):
    fn = ida_funcs.get_func(start)
    end = fn.end_ea if fn else start + 0x50
    result[hex(start)] = [
        (hex(ea), ida_lines.tag_remove(idc.generate_disasm_line(ea, 0)))
        for ea in idautils.Heads(start, end)
    ]
Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave19/renderer-render-sol/object-to-mesh-data.json').write_text(json.dumps(result, indent=2))
idc.qexit(0)
