from pathlib import Path
import json, struct
import ida_bytes, idc
addrs = [0x82181510, 0x82FAB910, 0x82FAB8F0, 0x82FAB8E0, 0x8327F110, 0x82CDA350, 0x8307A5F0, 0x8307A670, 0x8307A3B0, 0x8307A3C0, 0x8307A680, 0x8307A560, 0x82CDB450, 0x82CDB430]
rows = []
for address in addrs:
    data = ida_bytes.get_bytes(address, 16)
    rows.append({'address':hex(address), 'hex':data.hex(), 'f32':struct.unpack('>4f',data)})
Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave20/debris-render-sol/art-data.json').write_text(json.dumps(rows, indent=2))
idc.qexit(0)
