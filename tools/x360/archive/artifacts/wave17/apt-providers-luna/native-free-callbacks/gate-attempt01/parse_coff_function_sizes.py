import json
import struct
import sys
from pathlib import Path

obj_path = Path(sys.argv[1])
data = obj_path.read_bytes()
machine, section_count, timestamp, symptr, symcount, opt_size, characteristics = struct.unpack_from('<HHIIIHH', data, 0)
section_table = 20 + opt_size
sections = []
for i in range(section_count):
    off = section_table + i * 40
    name, vsize, vaddr, rawsize, rawptr, relocptr, lineptr, nreloc, nline, flags = struct.unpack_from('<8sIIIIIIHHI', data, off)
    sections.append({
        'number': i + 1,
        'name_raw': name.rstrip(b'\0').decode('ascii', 'replace'),
        'virtual_size': vsize,
        'virtual_address': vaddr,
        'raw_size': rawsize,
        'raw_pointer': rawptr,
        'reloc_pointer': relocptr,
        'relocation_count': nreloc,
        'characteristics': '0x%08X' % flags,
    })
strtab_start = symptr + symcount * 18
strtab_len = struct.unpack_from('<I', data, strtab_start)[0] if symptr else 0
strtab = data[strtab_start:strtab_start + strtab_len] if symptr else b''
def decode_name(raw):
    if raw[:4] == b'\0\0\0\0':
        off = struct.unpack_from('<I', raw, 4)[0]
        if off < len(strtab):
            end = strtab.find(b'\0', off)
            return strtab[off:end if end >= 0 else len(strtab)].decode('utf-8', 'replace')
        return '<bad string offset %d>' % off
    return raw.split(b'\0', 1)[0].decode('utf-8', 'replace')
targets = {
    '?FreeTexture@AptCallbackRender@CgsGui@@SAXPAX@Z',
    '?FreeRenderingUnit@AptCallbackRender@CgsGui@@SAXPAX@Z',
}
found = []
i = 0
while i < symcount:
    off = symptr + i * 18
    raw = data[off:off + 18]
    name = decode_name(raw[:8])
    value, section_number, typ = struct.unpack_from('<IhH', raw, 8)
    storage_class, aux_count = raw[16], raw[17]
    if name in targets:
        aux = data[off + 18:off + 36] if aux_count else b''
        total_size = struct.unpack_from('<I', aux, 4)[0] if len(aux) == 18 else None
        section = sections[section_number - 1] if 1 <= section_number <= len(sections) else None
        raw_body = None
        if section and total_size is not None and section['raw_pointer']:
            start = section['raw_pointer'] + value
            raw_body = data[start:start + total_size].hex().upper()
        found.append({
            'name': name,
            'symbol_table_index': i,
            'value': value,
            'section_number': section_number,
            'type': '0x%04X' % typ,
            'storage_class': storage_class,
            'aux_count': aux_count,
            'function_aux_size': total_size,
            'section': section,
            'body_bytes': raw_body,
        })
    i += 1 + aux_count
result = {
    'path': str(obj_path),
    'coff_machine': '0x%04X' % machine,
    'section_count': section_count,
    'symbol_count': symcount,
    'functions': found,
}
print(json.dumps(result, indent=2))

