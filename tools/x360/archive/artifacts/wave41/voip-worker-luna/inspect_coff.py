import json
import struct
from pathlib import Path

obj_path = Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave41\voip-worker-luna\gate1\voip_x360-7f024369.obj')
data = obj_path.read_bytes()
machine, section_count, timestamp, symbol_offset, symbol_count, optional_size, flags = struct.unpack_from('<HHIIIHH', data, 0)
string_offset = symbol_offset + symbol_count * 18
string_size = struct.unpack_from('<I', data, string_offset)[0] if symbol_offset else 0
symbols = []
i = 0
while i < symbol_count:
    offset = symbol_offset + i * 18
    raw_name = data[offset:offset + 8]
    if raw_name[:4] == bytes(4):
        name_offset = struct.unpack_from('<I', raw_name, 4)[0]
        start = string_offset + name_offset
        end = data.find(bytes([0]), start)
        name = data[start:end].decode('ascii', 'replace')
    else:
        name = raw_name.split(bytes([0]), 1)[0].decode('ascii', 'replace')
    value = struct.unpack_from('<I', data, offset + 8)[0]
    section = struct.unpack_from('<h', data, offset + 12)[0]
    symbol_type = struct.unpack_from('<H', data, offset + 14)[0]
    storage = data[offset + 16]
    aux_count = data[offset + 17]
    if storage == 2 and section > 0 and any(token in name for token in
          ('VoipRegisterRemoteTalker', 'VoipUpdateVoiceActivity', 'VoipXhvUpdate', 'VoipWorkerThread')):
        symbols.append({'name': name, 'value': value, 'section': section,
                        'type': symbol_type, 'storageClass': storage,
                        'auxRecords': aux_count})
    i += 1 + aux_count
assert machine == 0x01f2, hex(machine)
assert any(s['name'] == 'VoipRegisterRemoteTalker' for s in symbols), symbols
assert any(s['name'] == 'VoipUpdateVoiceActivity' for s in symbols), symbols
assert any(s['name'] == 'VoipWorkerThread' for s in symbols), symbols
assert any('VoipXhvUpdate' in s['name'] for s in symbols), symbols
result = {'object': str(obj_path), 'machine': hex(machine), 'sections': section_count,
          'symbols': symbol_count, 'stringTableSize': string_size,
          'definitions': symbols}
Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave41\voip-worker-luna\coff-symbols.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
print(json.dumps(result, indent=2))
