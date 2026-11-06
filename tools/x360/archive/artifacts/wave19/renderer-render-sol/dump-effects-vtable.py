from pathlib import Path
import json
import ida_bytes, ida_name, idc
base = 0x820D0D48
rows = []
for offset in range(0, 0x50, 4):
    address = int.from_bytes(ida_bytes.get_bytes(base+offset, 4), 'big')
    rows.append({'offset': hex(offset), 'address': hex(address), 'name': ida_name.get_name(address)})
Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave19/renderer-render-sol/effects-vtable-artist.json').write_text(json.dumps(rows, indent=2))
idc.qexit(0)
