from pathlib import Path
import json, struct
import ida_bytes, idc
addresses = [0x820054D0, 0x82013FD0, 0x82013FCC, 0x82004A20, 0x8200DD40, 0x82001DA0, 0x82004D04, 0x82005548, 0x82009BA0, 0x820092CC, 0x82001C90]
rows = []
for address in addresses:
    data = ida_bytes.get_bytes(address, 4)
    rows.append({'address':hex(address), 'bits':data.hex(), 'value':struct.unpack('>f',data)[0]})
Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave20/debris-render-sol/art-floats.json').write_text(json.dumps(rows, indent=2))
idc.qexit(0)
