import json
import struct
import sys
from pathlib import Path

def read(path):
    data=Path(path).read_bytes()
    machine,nsections,timestamp,symptr,nsymbols,optsize,chars=struct.unpack_from('<HHIIIHH',data,0)
    stbase=symptr+nsymbols*18
    stsize=struct.unpack_from('<I',data,stbase)[0] if symptr else 0
    table=data[stbase:stbase+stsize] if stsize else b''
    undefined=[]; defined=[]
    i=0
    while i<nsymbols:
        ent=data[symptr+i*18:symptr+i*18+18]; raw=ent[:8]
        if raw[:4]==b'\0\0\0\0':
            off=struct.unpack_from('<I',raw,4)[0]; end=table.find(b'\0',off)
            name=table[off:end].decode('latin1') if end>=0 else ''
        else: name=raw.split(b'\0',1)[0].decode('latin1')
        value,section,typ,storage,aux=struct.unpack_from('<IhHBB',ent,8)
        if storage in (2,105):
            if section==0: undefined.append(name)
            else: defined.append(name)
        i+=1+aux
    undefined=sorted(set(undefined)); defined=sorted(set(defined))
    wanted=['ConnApiHost','ConnApiConnect','ConnApiAddClient','ConnApiRemoveClient','ConnApiStart','ConnApiUpdate']
    return {'object':str(path),'machine':machine,'undefined_count':len(undefined),
            'undefined_external':undefined,
            'unresolved_internal_sub_82B_names':[n for n in undefined if 'sub_82B' in n],
            'defined_connapi_public':[n for n in defined if n in wanted]}
print(json.dumps([read(p) for p in sys.argv[1:]],indent=2))
