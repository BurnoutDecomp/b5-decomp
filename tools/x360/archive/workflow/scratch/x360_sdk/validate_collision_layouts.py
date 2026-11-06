import json,random,struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent))
import validate_native_math as M
root=Path(__file__).resolve().parents[2]/'b5-decomp'
out=root/'build/x360/native-collision-validation'
report=json.loads((root/'build/x360/native-collision-layouts/compile-results.json').read_text())
obj=Path(next(x['object'] for x in report['results'] if x['source'].endswith('/VolumeBBoxQuery.cpp')))
data=obj.read_bytes()
machine,nsec,stamp,symoff,nsyms,optsize,flags=struct.unpack_from('<HHIIIHH',data)
assert machine==0x1f2
strings=data[symoff+nsyms*18:]
def name(raw):
    if raw[:4]==b'\0'*4:
        at=struct.unpack_from('<I',raw,4)[0]
        return strings[at:strings.index(b'\0',at)].decode()
    return raw.rstrip(b'\0').decode()
sections=[struct.unpack_from('<8sIIIIIIHHI',data,20+optsize+i*40) for i in range(nsec)]
functions={};i=0
while i<nsyms:
    raw,value,sec,kind,storage,aux=struct.unpack_from('<8sIhHBB',data,symoff+i*18)
    text=name(raw)
    for f in ['GetResourceDescriptor','Initialize']:
        if text.startswith('?'+f+'@VolumeBBoxQuery@') and sec>0:
            section=sections[sec-1]
            assert section[7]==0, (text,'Unexpected relocation')
            functions[f]=(section,value,text)
    i+=1+aux
assert len(functions)==2
random.seed(360)
cases=[(s,r) for s in [0,1,2,31,255,0x7fffffff,0xffffffff] for r in [0,1,2,17,255,0x7fffffff,0xffffffff]]
cases += [(random.getrandbits(32),random.getrandbits(32)) for _ in range(100)]
checks=0;errors=[]
for f,original in [('GetResourceDescriptor',0x82bbbd38),('Initialize',0x82bbbd90)]:
    section,value,symbol=functions[f]
    code=data[section[4]:section[4]+section[3]]
    for stack,results in cases:
        for null in ([False,True] if f=='Initialize' else [False]):
            outputs=[]
            for compiled in [False,True]:
                cpu=M.new_cpu()
                cpu.mem.add(0x91000000,len(code),code)
                cpu.mem.wr(M.S.SELF,b'\xa5'*0x100)
                cpu.mem.wr(M.S.AUX,struct.pack('>I',0 if null else M.S.SELF))
                cpu.setg(3,M.S.AUX if f=='Initialize' else M.S.SELF)
                cpu.setg(4,stack);cpu.setg(5,results)
                cpu.run(0x91000000+value if compiled else original)
                outputs.append((cpu.g(3)&0xffffffff,cpu.mem.rd(M.S.SELF,0x100)))
            checks+=1
            if outputs[0]!=outputs[1]:
                errors.append({'function':f,'stack':stack,'results':results,'null':null})
    print(f,'checked',flush=True)
result={'cases':checks,'mismatches':errors,'object':str(obj),'source':'actual native COFF function sections, verified relocation-free','game_booted':False}
(out/'comparison.json').write_text(json.dumps(result,indent=2)+'\n')
print('Collision cases',checks,'mismatches',len(errors))
if errors:print(errors[:5])
raise SystemExit(bool(errors))
