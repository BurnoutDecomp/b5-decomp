import json, random, struct, sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent))
import validate_native_math as M
from validate_render_states import Coff, CPU as BaseCPU

class CPU(BaseCPU):
    def fetch(self,pc):
        self.last_pc=pc
        return super().fetch(pc)

ROOT=Path(__file__).resolve().parents[2]/'b5-decomp'
OUT=ROOT/'build/x360/native-trigger-validation'
OUT.mkdir(exist_ok=True)
report=json.loads((ROOT/'build/x360/native-data-layouts/compile-results.json').read_text(encoding='utf-8'))
obj=Path(next(x['object'] for x in report['results'] if x['source'].endswith('/BrnTriggerData.cpp') and x['passed']))
functions={'up':Coff(obj,'?FixUp@TriggerData@'),'down':Coff(obj,'?FixDown@TriggerData@')}
TOP=[(0x30,0x34,0x200),(0x3c,0x40,0x300),(0x44,0x48,0x400),
     (0x4c,0x50,0x500),(0x54,0x58,0x600),(0x5c,0x60,0x700),
     (0x64,0x68,0x800),(0x6c,0x70,0x900),(0x74,0x78,0xa00)]
rng=random.Random(360)
cases=[[v]*9 for v in [-1,0,1,2,4]]
for active in range(9):
    counts=[0]*9;counts[active]=4;cases.append(counts)
cases += [[rng.choice([-1,0,1,2,4]) for _ in range(9)] for _ in range(120)]
errors=[];checked=0


def fixture(counts,index,base,absolute):
    header=bytearray(rng.randbytes(0x100))
    payload=bytearray(rng.randbytes(0x4000))
    struct.pack_into('>II',header,0,34,0x4000)
    def pointer(offset):return (base+offset)&0xffffffff if absolute else offset
    for slot,count_slot,offset in TOP:
        count=counts[TOP.index((slot,count_slot,offset))]
        struct.pack_into('>I',header,slot,pointer(offset if count>0 else 0))
        struct.pack_into('>i',header,count_slot,count)
    for i in range(max(0,counts[0])):
        # Actual52-byte Landmark, private starting-grid pointer at2C.
        struct.pack_into('>I',payload,0x200+i*52+0x2c,pointer(0xb00+i*0x40 if (index+i)%3 else 0))
    for i in range(max(0,counts[1])):
        count=[-1,0,1,4][(index+i)%4]
        offset=0xc00+i*0x40 if count>0 else 0
        struct.pack_into('>Ii',payload,0x300+i*24+0x10,pointer(offset),count)
        for j in range(max(0,count)):
            struct.pack_into('>I',payload,offset+j*4,pointer(0x400+j*56))
    for i in range(max(0,counts[3])):
        count=[-1,0,1,4][(index+i)%4]
        offset=0xe00+i*0x40 if count>0 else 0
        struct.pack_into('>IiIi',payload,0x500+i*16,pointer(offset),count,pointer(0xf00+i*0x40),i)
        for j in range(max(0,count)):
            struct.pack_into('>I',payload,offset+j*4,pointer(0x400+j*56))
    for j in range(max(0,counts[8])):
        struct.pack_into('>I',payload,0xa00+j*4,pointer(0x200+j*52))
    return header,payload


for direction,address in [('up',0x8267fc08),('down',0x8267f800)]:
    coff=functions[direction]
    for index,counts in enumerate(cases):
        base=[0x50000000,0xa0000000][index%2]
        assert all(base+0x4000<=at or at+len(body)<=base for at,body in M.SECTIONS)
        header,payload=fixture(counts,index,base,direction=='down')
        outputs=[]
        for compiled in [False,True]:
            c=coff.cpu()
            c.__class__=CPU
            c.mem.add(base,len(payload),payload)
            c.mem.wr(M.S.SELF,header)
            c.setg(3,M.S.SELF);c.setg(4,base)
            try:
                c.run(coff.entry if compiled else address)
            except Exception:
                print('Failed',direction,index,counts,'compiled',compiled,'PC',hex(c.last_pc),flush=True)
                print('registers',{r:hex(c.g(r)) for r in [3,4,9,10,11,26,27,28,29,30,31]},flush=True)
                print('header',header.hex(),flush=True)
                raise
            outputs.append((c.mem.rd(M.S.SELF,len(header)),c.mem.rd(base,len(payload))))
        checked+=1
        if outputs[0]!=outputs[1]:
            errors.append({'direction':direction,'case':index,'counts':counts,'base':hex(base),
                           'header_bytes':[i for i,(a,b) in enumerate(zip(outputs[0][0],outputs[1][0])) if a!=b],
                           'payload_bytes':[i for i,(a,b) in enumerate(zip(outputs[0][1],outputs[1][1])) if a!=b]})
    print(direction,len(cases),'checked',flush=True)
result={'cases':checked,'mismatches':errors,'object':str(obj),
        'target':'actual native PPC COFF FixUp/Down versus ARTIST instructions',
        'limits':'version34 and valid mapped pointer trees; counts -1/0/1/2/4, empty/nested arrays, high/low load bases; assert paths reject execution; no hardware/boot or byte match claim'}
(OUT/'comparison.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print('Trigger relocation:',checked,'cases;',len(errors),'mismatches')
if errors:print(json.dumps(errors[:6],indent=2))
raise SystemExit(bool(errors))
