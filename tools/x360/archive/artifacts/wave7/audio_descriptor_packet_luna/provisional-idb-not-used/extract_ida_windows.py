import json
from pathlib import Path
import ida_bytes
import idc

root = Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave7\dac-terra')
records = json.loads((root/'stage-descriptor-original-inventory.json').read_text(encoding='utf-8'))
out = []
for row in records:
    record = int(row['record'], 16)
    desc = ida_bytes.get_bytes(record, 0x34)
    entry = {'name': row['name'], 'record': row['record'],
             'descriptor_read_size': 0x34, 'descriptor_hex': desc.hex() if desc else None,
             'metadata_probe_windows': {}}
    for field in ('channel_maps', 'parameters', 'events'):
        ptr = int(row[field], 16)
        if not ptr:
            entry['metadata_probe_windows'][field] = {'pointer': row[field], 'bytes_read': 0, 'hex': ''}
            continue
        data = ida_bytes.get_bytes(ptr, 0x80)
        entry['metadata_probe_windows'][field] = {
            'pointer': row[field], 'bytes_read': len(data) if data else 0,
            'hex': data.hex() if data else None,
            'note': 'fixed read window for inspection only; not an inferred table extent'
        }
    out.append(entry)
output = Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave7\audio_descriptor_packet_luna\ida-data-windows.json')
output.write_text(json.dumps({'records': out}, indent=2)+'\n', encoding='utf-8')
print('wrote', output, 'records', len(out))
idc.qexit(0)
