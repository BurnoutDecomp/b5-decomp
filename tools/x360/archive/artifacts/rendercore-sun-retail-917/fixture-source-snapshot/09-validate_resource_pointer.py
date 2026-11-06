import json, random, re, struct, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
import validate_native_math as M
from validate_render_states import Coff, CPU

ROOT = Path(__file__).resolve().parents[2]/'b5-decomp'
REPORT = ROOT/'build/x360/native-resource-pointer/compile-results.json'
OUT = ROOT/'build/x360/native-resource-pointer-validation'
OUT.mkdir(exist_ok=True)


class Bundle:
    def __init__(self, objects):
        self.objects = objects
        self.definitions = {}
        for oi, obj in enumerate(objects):
            for name, value, sec in obj.symbols.values():
                if sec > 0 and name.startswith('?'):
                    self.definitions.setdefault(name, (oi,value,sec))

    def select(self, prefix):
        matches = [(name,v) for name,v in self.definitions.items() if name.startswith(prefix)]
        assert len(matches) == 1, (prefix,matches)
        self.symbol, (oi,value,sec) = matches[0]
        used, pending = set(), [(oi,sec)]
        while pending:
            key = pending.pop()
            if key in used:
                continue
            used.add(key)
            obj = self.objects[key[0]]
            for at, index, kind in obj.relocations(key[1]):
                if kind == 0x12:
                    assert index == 0
                    continue
                name, _, target = obj.symbols[index]
                if target > 0:
                    pending.append((key[0],target))
                elif name in self.definitions:
                    other, _, section = self.definitions[name]
                    pending.append((other,section))
        bases = {key:0x83800000+i*0x10000 for i,key in enumerate(sorted(used))}
        self.bases = bases
        self.images, self.unresolved = {}, {}
        unresolved_by_name = {}
        for key in used:
            obj = self.objects[key[0]]
            s = obj.sections[key[1]]
            assert s[3] < 0x10000
            body = bytearray(obj.data[s[4]:s[4]+s[3]]) if s[4] else bytearray(s[3])
            for at, index, kind in obj.relocations(key[1]):
                if kind == 0x12:
                    continue
                name, value, target = obj.symbols[index]
                crt = re.fullmatch(r'__(save|rest)gprlr_(\d+)',name)
                if target > 0:
                    address = bases[(key[0],target)]+value
                elif name in self.definitions:
                    other,value,section = self.definitions[name]
                    address = bases[(other,section)]+value
                elif crt:
                    address = (0x82c08eb0 if crt[1]=='save' else 0x82c08f00)+4*(int(crt[2])-14)
                else:
                    if name not in unresolved_by_name:
                        address = 0x84800000+16*len(self.unresolved)
                        unresolved_by_name[name] = address
                        self.unresolved[address] = name
                    address = unresolved_by_name[name]
                word = struct.unpack_from('>I',body,at)[0]
                if kind in [0x10,0x11]:
                    assert word & 0xffff == 0
                    imm = (address+0x8000)>>16 if kind==0x10 else address
                    word = (word&0xffff0000)|(imm&0xffff)
                elif kind == 6:
                    delta = address-(bases[key]+at)
                    assert -(1<<25) <= delta < (1<<25)
                    word = (word&0xfc000003)|(delta&0x03fffffc)
                elif kind == 2:
                    word = (word+address)&0xffffffff
                else:
                    raise AssertionError(('PPC relocation',kind,name))
                struct.pack_into('>I',body,at,word)
            self.images[bases[key]] = body
        self.entry = bases[(oi,sec)]+matches[0][1][1]

    def cpu(self):
        c = M.new_cpu()
        c.__class__ = CPU
        c.mem.add(0,0x1000)
        for at,body in self.images.items():
            c.mem.add(at,len(body),body)
        def reject(name):
            def called(cpu):
                raise AssertionError(('Unresolved function executed',name))
            return called
        for at,name in self.unresolved.items():
            c.hooks[at] = reject(name)
        return c


def main():
    d = json.loads(REPORT.read_text(encoding='utf-8'))
    paths = {Path(x['source']).name:Path(x['object']) for x in d['results'] if x['passed']}
    bundle = Bundle([Coff(paths['CgsBaseResourcePtr.cpp'],'??0BaseResourcePtr@'),
                     Coff(paths['CgsResourcePtr.cpp'],'?CreateFromHandle@BaseResourcePtr@')])
    specs = [('??0BaseResourcePtr@',0x82204e20,'ctor'),('??1BaseResourcePtr@',0x821f1e18,'dtor'),
             ('?IsEqual@BaseResourcePtr@',0x8227d298,'equal'),('?AddToNewList@BaseResourcePtr@',0x828d6230,'add'),
             ('?CreateFromHandle@BaseResourcePtr@',0x828d93a0,'bind'),('?Propogate@BaseResourcePtr@',0x828d9418,'propagate')]
    rng = random.Random(360)
    checks, errors, functions = 0, [], []
    for prefix, original, kind in specs:
        bundle.select(prefix)
        for index in range(128):
            self_bytes, aux_bytes = bytearray(rng.randbytes(0x1000)), bytearray(rng.randbytes(0x2000))
            zero = rng.randbytes(12)
            a,b,cnode = M.S.SELF,M.S.AUX,M.S.AUX+0x100
            def put_node(address,next_node,prev_node):
                image,offset = (self_bytes,address-a) if a<=address<a+0x1000 else (aux_bytes,address-b)
                struct.pack_into('>II',image,offset+0xc,next_node,prev_node)
            mode = index%6
            if kind=='add':
                put_node(a,a,a)
                put_node(b,b if mode%2==0 else cnode,b if mode%2==0 else cnode)
                if mode%2:
                    put_node(cnode,b,b)
            elif mode==0:
                put_node(a,a,a)
            elif mode==1:
                put_node(a,0,0)
            elif mode==2:
                put_node(a,b,0);put_node(b,a,a)
            elif mode==3:
                put_node(a,0,b);put_node(b,a,a)
            elif mode==4:
                put_node(a,b,b);put_node(b,a,a)
            else:
                put_node(a,b,cnode);put_node(b,cnode,a);put_node(cnode,a,b)
            if kind=='propagate' and mode in [1,2,3]:
                put_node(a,a,a)
            if kind=='equal':
                aux_bytes[:12] = self_bytes[:12]
                if index%4:
                    lane = index%3
                    value = struct.unpack_from('>I',aux_bytes,lane*4)[0]
                    struct.pack_into('>I',aux_bytes,lane*4,value^1)
            source = 0 if index%7==0 else (a if index%7==1 else b+0x200)
            entry = b+0x400 if index%2 or kind=='propagate' else 0
            if entry:
                put_node(entry+0x28,entry+0x28,entry+0x28)
            handle = a+0x14 if index%5==0 else b+0x1000
            if kind in ['bind','propagate']:
                hbuf, hoff = (self_bytes,handle-a) if handle<a+0x1000 else (aux_bytes,handle-b)
                struct.pack_into('>II',hbuf,hoff,source,entry)
                if kind=='propagate':
                    struct.pack_into('>II',self_bytes,0x14,source,entry)
            outputs=[]
            for compiled in [False,True]:
                cpu = bundle.cpu()
                cpu.mem.wr(a,self_bytes);cpu.mem.wr(b,aux_bytes);cpu.mem.wr(0,zero)
                cpu.setg(3,a);cpu.setg(4,handle if kind=='bind' else b)
                cpu.run(bundle.entry if compiled else original)
                ret = cpu.g(3)&0xffffffff if kind in ['ctor','equal'] else None
                outputs.append((ret,cpu.mem.rd(a,0x1000),cpu.mem.rd(b,0x2000)))
            checks+=1
            if outputs[0]!=outputs[1]:
                errors.append({'function':prefix,'case':index,'mode':mode,
                               'original_return':outputs[0][0],'compiled_return':outputs[1][0],
                               'self_bytes':[i for i,(a,b) in enumerate(zip(outputs[0][1],outputs[1][1])) if a!=b],
                               'aux_bytes':[i for i,(a,b) in enumerate(zip(outputs[0][2],outputs[1][2])) if a!=b]})
        functions.append({'symbol':bundle.symbol,'original':hex(original),'cases':128,'sections':len(bundle.images)})
        print(prefix,'checked',flush=True)
    result={'cases':checks,'mismatches':errors,'functions':functions,'objects':[str(p) for p in paths.values()],
            'target':'actual compiled PPC COFF, cross-object calls resolved, versus ARTIST instructions',
            'limits':'no hardware/boot claim; source-zero cases use an explicitly mapped guest address0 with deterministic data; unresolved assert calls reject execution'}
    (OUT/'comparison.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('Resource pointers:',checks,'cases;',len(errors),'mismatches')
    if errors:print(json.dumps(errors[:5],indent=2))
    raise SystemExit(bool(errors))


if __name__=='__main__':
    main()
