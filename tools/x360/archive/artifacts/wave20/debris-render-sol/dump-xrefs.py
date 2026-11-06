from pathlib import Path
import json
import ida_bytes, ida_funcs, ida_lines, idautils, idc
addresses = [0x82FAB910, 0x82FAB8F0, 0x82FAB8E0, 0x8327F110, 0x8307A5F0, 0x8307A670, 0x8307A3B0, 0x8307A3C0, 0x8307A680, 0x8307A560]
rows = {}
for address in addresses:
    result = []
    for ref in idautils.XrefsTo(address):
        name = ida_funcs.get_func_name(ref.frm)
        text = ida_lines.tag_remove(idc.generate_disasm_line(ref.frm,0))
        row = {'from':hex(ref.frm), 'name':name, 'line':text}
        if not name or 'initializer' in name.lower():
            row['near'] = [(hex(ea), ida_lines.tag_remove(idc.generate_disasm_line(ea,0))) for ea in range(ref.frm-40,ref.frm+40,4)]
        result.append(row)
    rows[hex(address)] = result
Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave20/debris-render-sol/art-xrefs.json').write_text(json.dumps(rows, indent=2))
idc.qexit(0)
