from pathlib import Path
import hashlib,json,struct

HERE=Path(__file__).resolve().parent
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()

def runtime(path):
    raw=Path(path).read_bytes()
    machine,count,stamp,symbols,nsyms,opt,flags=struct.unpack_from('<HHIIIHH',raw)
    assert machine==0x1F2
    strings=symbols+18*nsyms
    def string(offset):
        pos=strings+offset
        return raw[pos:raw.index(0,pos)].decode('utf-8',errors='replace')
    names={}
    for i in range(nsyms):
        p=symbols+18*i
        name=string(struct.unpack_from('<I',raw,p+4)[0]) if raw[p:p+4]==bytes(4) else raw[p:p+8].split(b'\0')[0].decode('utf-8',errors='replace')
        value,section=struct.unpack_from('<Ih',raw,p+8)
        names[i]=(name,value,section)
    sections=[]
    for i in range(count):
        s=struct.unpack_from('<8sIIIIIIHHI',raw,20+opt+40*i)
        name=s[0].split(b'\0')[0].decode()
        if name.startswith('/'):
            name=string(int(name[1:]))
        sections.append((name,s))
    slots={i+1:j for j,(i,(name,s))in enumerate((x for x in enumerate(sections)if not x[1][0].startswith('.debug')))}
    result=[]
    for i,(name,s)in enumerate(sections):
        if name.startswith('.debug'):continue
        data=raw[s[4]:s[4]+s[3]] if s[4] else bytes(s[3])
        relocs=[]
        for n in range(s[7]):
            offset,symbol,kind=struct.unpack_from('<IIH',raw,s[5]+10*n)
            target,value,section=names[symbol]
            relocs.append((offset,kind,target,value,slots.get(section,section)))
        result.append((name,s[9],len(data),hashlib.sha256(data).hexdigest(),relocs))
    return result

old_report=HERE/'sol-native/candidate-v3/compile-results.json'
new_report=HERE/'final-native/compile-results.json'
old=next(Path(x['object'])for x in json.loads(old_report.read_text())['results']if x['source'].endswith('ExternalPhysicsBody.cpp'))
new=next(Path(x['object'])for x in json.loads(new_report.read_text())['results']if x['source'].endswith('ExternalPhysicsBody.cpp'))
a,b=runtime(old),runtime(new)
differences=[i for i in range(max(len(a),len(b)))if (a[i]if i<len(a)else None)!=(b[i]if i<len(b)else None)]
result={'old':str(old),'old_sha256':sha(old),'new':str(new),'new_sha256':sha(new),'old_report_sha256':sha(old_report),'new_report_sha256':sha(new_report),
        'old_runtime_sections':len(a),'new_runtime_sections':len(b),'different_runtime_section_indexes':differences,
        'method':'Compare every non-debug section name,flags,size,unrelocated bytes and named relocation target/value/normalized runtime-section index. COFF build timestamp/debug records/section indexes excluded; no product code or data bytes rewritten.',
        'verifier_sha256':sha(__file__)}
p=HERE/'final-runtime-binding.json';assert not p.exists();p.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps(result));assert not differences
