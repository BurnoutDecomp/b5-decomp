import struct
from pathlib import Path
p = Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave37\lobby-dispatch-luna\gate1\lobbylogin-48abc231.obj')
b = p.read_bytes()
machine, sections, timestamp, symoff, symcount, optsize, flags = struct.unpack_from('<HHIIIHH', b, 0)
string_off = symoff + symcount * 18
string_size = struct.unpack_from('<I', b, string_off)[0] if symoff else 0
wanted = {'sub_82B1FD80', 'sub_82B20070', 'sub_82B1FCE0'}
found = []
i = 0
while i < symcount:
    off = symoff + i * 18
    raw = b[off:off+8]
    if raw[:4] == b'\0\0\0\0':
        nameoff = struct.unpack_from('<I', raw, 4)[0]
        end = b.find(b'\0', string_off + nameoff)
        name = b[string_off + nameoff:end].decode('ascii', 'replace')
    else:
        name = raw.split(b'\0', 1)[0].decode('ascii', 'replace')
    value = struct.unpack_from('<I', b, off + 8)[0]
    section = struct.unpack_from('<h', b, off + 12)[0]
    symtype = struct.unpack_from('<H', b, off + 14)[0]
    storage = b[off + 16]
    aux = b[off + 17]
    if name in wanted:
        found.append({'name': name, 'section': section, 'value': value,
                      'type': symtype, 'storage_class': storage,
                      'aux_records': aux})
    i += 1 + aux
print({'machine': hex(machine), 'sections': sections, 'symbols': symcount,
       'string_table_size': string_size, 'provider_symbols': found})
