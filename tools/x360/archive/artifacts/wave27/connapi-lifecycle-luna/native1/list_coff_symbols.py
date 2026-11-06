import json
import struct
import sys
from pathlib import Path

def symbols(path):
    data=Path(path).read_bytes()
    machine, nsections, timestamp, symptr, nsymbols, optsize, characteristics=struct.unpack_from('<HHIIIHH', data, 0)
    stbase=symptr + nsymbols * 18
    stsize=struct.unpack_from('<I', data, stbase)[0] if symptr else 0
    table=data[stbase:stbase+stsize] if stsize else b''
    undefined=[]
    all_external=[]
    i=0
    while i < nsymbols:
        pos=symptr + i*18
        ent=data[pos:pos+18]
        raw=ent[:8]
        if raw[:4] == b'\0\0\0\0':
            off=struct.unpack_from('<I', raw, 4)[0]
            end=table.find(b'\0', off)
            name=table[off:end].decode('latin1') if end >= 0 else ''
        else:
            name=raw.split(b'\0',1)[0].decode('latin1')
        value, section, typ, storage, aux=struct.unpack_from('<IhHBB', ent, 8)
        if storage in (2,105):
            all_external.append({'name':name,'section':section,'value':value,'storage':storage})
            if section == 0:
                undefined.append({'name':name,'storage':storage})
        i += 1 + aux
    return {'object':str(path),'machine':machine,'sections':nsections,'symbols':nsymbols,'undefined_external':undefined,'defined_external':all_external}

out=[symbols(p) for p in sys.argv[1:]]
print(json.dumps(out, indent=2))
