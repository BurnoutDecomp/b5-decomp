import json, random, struct, sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent))
import validate_native_math as M
from validate_render_states import Coff, CPU
from validate_resource_pointer import Bundle

ROOT=Path(__file__).resolve().parents[2]/'b5-decomp'
OUT=ROOT/'build/x360/native-pool-validation'
OUT.mkdir(exist_ok=True)

class PoolCPU(CPU):
    def step(self,pc,w,nxt,depth):
        if w>>26==31 and (w>>1)&1023==27: # sld; RB bit6 selects zero
            assert not w&1
            rs,ra,rb=(w>>21)&31,(w>>16)&31,(w>>11)&31
            shift=self.g(rb)&127
            self.setg(ra,self.g(rs)<<shift if shift<64 else 0)
            return nxt
        return super().step(pc,w,nxt,depth)

class Images(Bundle):
    def cpu(self):
        cpu=super().cpu();cpu.__class__=PoolCPU;return cpu

    def select(self,prefix):
        super().select(prefix)
        oi,_,sec=self.definitions[self.symbol]
        used=set();pending=[(oi,sec)]
        while pending:
            key=pending.pop()
            if key in used:continue
            used.add(key);obj=self.objects[key[0]]
            for _,index,kind in obj.relocations(key[1]):
                if kind==0x12:continue
                name,_,target=obj.symbols[index]
                if target>0:pending.append((key[0],target))
                elif name in self.definitions:
                    other,_,section=self.definitions[name];pending.append((other,section))
        self.bases={key:0x83800000+i*0x10000 for i,key in enumerate(sorted(used))}
    def address(self,name):
        oi,value,sec=self.definitions[name]
        return self.bases[(oi,sec)]+value

def main():
    report=json.loads((ROOT/'build/x360/native-pool-validation-objects/compile-results.json').read_text(encoding='utf-8'))
    paths={Path(v['source']).name:Path(v['object']) for v in report['results'] if v['passed']}
    b=Images([Coff(paths['pool-gyro.cpp'],'?AllocateNativeGyro'),Coff(paths['BrnAbstractPool.cpp'],'?Prepare@AbstractPoolVoidHandle@')])
    b.select('?AllocateNativeGyro')
    tables=[(0,0x8200ab84,'??_7BehaviourGyroCam@Camera@BrnDirector@@6B@'),
            (0x20,0x8200a158,'??_7VisibilityCollisionPolicy@Camera@BrnDirector@@6B@'),
            (0x260,0x8200a150,'??_7CollisionPolicyAttachedToVehicle@Camera@BrnDirector@@6B@')]
    rng=random.Random(36010020);errors=[];cases=0
    for n in range(40):
        index=n%20;seed=rng.randbytes(0x7d70)
        state=bytearray(seed);struct.pack_into('>I',state,0x7d10,index)
        struct.pack_into('>I',state,0x7d60,1);struct.pack_into('>Q',state,0x7d68,0)
        outputs=[]
        slot=0x10+1600*index
        for compiled in (False,True):
            cpu=b.cpu();cpu.mem.wr(M.S.SELF,state)
            cpu.mem.wr(M.S.AUX,rng.randbytes(16));cpu.setg(3,M.S.AUX);cpu.setg(4,M.S.SELF)
            cpu.run(b.entry if compiled else 0x822597f8)
            pool=bytearray(cpu.mem.rd(M.S.SELF,0x7d70))
            for off,original,name in tables:
                expected=b.address(name) if compiled else original
                actual=struct.unpack_from('>I',pool,slot+off)[0]
                assert actual==expected,(compiled,n,hex(off),name,hex(actual),hex(expected))
                struct.pack_into('>I',pool,slot+off,original)
            outputs.append((bytes(pool),cpu.mem.rd(M.S.AUX,16)))
        cases+=1
        if outputs[0]!=outputs[1]:
            errors.append({'kind':'allocate','case':n,'pool_offsets':[i for i,(a,c) in enumerate(zip(outputs[0][0],outputs[1][0])) if a!=c],'handles':[x[1].hex() for x in outputs]})
    b.select('?ReleaseNativePool')
    for n in range(40):
        results=[]
        for compiled in (False,True):
            cpu=b.cpu();table=M.S.AUX+0x1000;pool=M.S.SELF;handle=M.S.AUX
            cpu.wr32(pool,table)
            for i in range(3):cpu.wr32(table+4*i,0xa2000000+4*i)
            cpu.mem.wr(handle,struct.pack('>4I',M.S.SELF+0x10,pool,n%20,1600))
            calls=[]
            def observe(c):calls.append((c.g(3)&0xffffffff,c.g(4)&0xffffffff))
            def reject(c):raise AssertionError('wrong free callback vtable slot')
            cpu.hooks[0xa2000000]=observe;cpu.hooks[0xa2000004]=reject;cpu.hooks[0xa2000008]=reject
            if compiled:
                cpu.setg(3,handle);cpu.run(b.entry);assert cpu.g(3)&0xff==1
            else:
                cpu.setg(3,pool);cpu.setg(31,handle)
                fetch=cpu.fetch;cpu.fetch=lambda at:0x4e800020 if at==0x8221ff7c else fetch(at)
                cpu.run(0x8221ff68)
            results.append(calls)
        cases+=1
        if results[0]!=results[1]:errors.append({'kind':'release','case':n,'calls':results})
    result={'cases':cases,'mismatches':errors,'target':'actual compiled PPC gyro allocation and free-callback dispatch versus ARTIST',
            'original_allocate':'822597F8','original_release_window':'8221FF68..8221FF78',
            'objects':[str(p) for p in paths.values()],
            'limits':'valid nonempty pool; all20 slots;40 allocation cases preserve random payload and surrounding bytes; three vtable identities normalized only after exact role/address checks; callback observed without executing FreeObject body; no hardware/boot proof'}
    (OUT/'comparison.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('Native camera pool:',cases,'cases;',len(errors),'mismatches')
    if errors:print(json.dumps(errors[:3],indent=2))
    raise SystemExit(bool(errors))

if __name__=='__main__':main()
