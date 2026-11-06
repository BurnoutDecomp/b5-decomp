"""Read the SDK COFF archive index and extract the native Arena object, read-only SDK."""
import json,struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]/'b5-decomp'
OUT=ROOT/'build/x360/xgraphics-inspection';OUT.mkdir(exist_ok=True)
reports=[]
for suffix in ['', 'd']:
    path=ROOT/f'build/x360/sdk-6534.4/portable/XDK/lib/xbox/xgraphics{suffix}.lib'
    data=path.read_bytes();assert data[:8]==b'!<arch>\n'
    at=8;members={};index=None
    while at<len(data):
        header=data[at:at+60];assert header[58:60]==b'`\n'
        size=int(header[48:58]);name=header[:16].decode().strip();body=data[at+60:at+60+size]
        members[at]=(name,body)
        if name=='/' and index is None:index=body
        at+=60+size+(size%2)
    count=struct.unpack_from('>I',index)[0];offsets=struct.unpack_from('>'+str(count)+'I',index,4)
    names=index[4+count*4:].split(b'\0');assert len(names)>=count
    symbols=[(name.decode('ascii'),offset) for name,offset in zip(names,offsets)]
    arena=[(n,o) for n,o in symbols if 'Arena@XGRAPHICS' in n]
    selected=[]
    for offset in sorted({o for n,o in arena}):
        name,body=members[offset];obj=OUT/f'xgraphics{suffix}-{offset:x}.obj';obj.write_bytes(body)
        machine,nsec,_,symoff,nsyms,optsize,_=struct.unpack_from('<HHIIIHH',body)
        sections=[]
        for i in range(nsec):
            raw,vs,va,size,ptr,*rest=struct.unpack_from('<8sIIIIIIHHI',body,20+optsize+i*40)
            sections.append({'name':raw.rstrip(b'\0').decode(),'size':size,'offset':ptr})
        selected.append({'object':str(obj),'member':name,'machine':machine,'sections':sections})
    reports.append({'library':str(path),'symbols':len(symbols),'arena_symbols':arena,'members':selected,
                    'related_symbols':[(n,hex(o)) for n,o in symbols if 'XGRAPHICS' in n and any(s in n for s in ['Heap','Compiler@','Context@'])]})
(OUT/'library-index.json').write_text(json.dumps(reports,indent=2)+'\n')
for r in reports:
    print(Path(r['library']).name, r['symbols'],'symbols;Arena:',r['arena_symbols'])
    print('Objects:',[(m['object'],[s for s in m['sections'] if s['name'].startswith('.debug')]) for m in r['members']])
    print('Related:',r['related_symbols'][:30])
