"""Shared read-only native fixture for the frame/barrier/atomic batch."""
import ast
from pathlib import Path
import struct
import sys
sys.path.insert(0,str(Path(__file__).parent))
import validate_native_math as M
from validate_render_states import Coff,CPU
from validate_resource_pointer import Bundle

tree=ast.parse((Path(__file__).parent/'validate_native_strings.py').read_text())
nodes=[n for n in tree.body if isinstance(n,ast.ClassDef) and n.name=='StringCPU']
exec(compile(ast.Module(body=nodes,type_ignores=[]),'reservation-fixture','exec'),globals())

class AtomicCPU(StringCPU):
    def step(self,pc,word,nxt,depth):
        self.last_pc=pc
        if hasattr(self,'instruction_observer'):self.instruction_observer(pc,word)
        if word>>26==31 and word&1 and ((word>>1)&511) in [40,104,266]:
            xo=(word>>1)&511;dest=(word>>21)&31;ra=(word>>16)&31;rb=(word>>11)&31
            value=(self.g(rb)-self.g(ra) if xo==40 else (-self.g(ra) if xo==104 else self.g(ra)+self.g(rb)))&0xFFFFFFFFFFFFFFFF
            self.setg(dest,value);self.setcr(0,bool(value>>63),bool(value and not value>>63),value==0)
            return nxt
        if word>>26==31 and ((word>>1)&1023)==824: # srawi + carry
            src=(word>>21)&31;dest=(word>>16)&31;shift=(word>>11)&31
            value=self.g(src)&0xFFFFFFFF;signed=(value&0x7FFFFFFF)-(value&0x80000000)
            result=signed>>shift;self.xer_ca=int(signed<0 and bool(value&((1<<shift)-1)))
            self.setg(dest,result)
            if word&1:self.setcr(0,result<0,result>0,result==0)
            return nxt
        if word>>26==31 and ((word>>1)&1023)==202: # addze
            dest=(word>>21)&31;src=(word>>16)&31
            result=self.g(src)+getattr(self,'xer_ca',0);self.xer_ca=int(result>>64!=0)
            self.setg(dest,result)
            if word&1:
                value=result&0xFFFFFFFFFFFFFFFF;self.setcr(0,bool(value>>63),bool(value and not value>>63),value==0)
            return nxt
        if word == 0x7C2004AC:
            self.fence_trace.append((self.mem.rd(M.S.SELF,0x30).hex(),len(self.atomic_trace)))
        if word == 0x4E800420 and (self.ctr & 0xFFFFFFFC) in self.hooks:
            self.call(self.ctr & 0xFFFFFFFC,depth)
            raise CPU.run.__globals__['Return']()
        if word>>26==31 and ((word>>1)&1023) in [20,84,150,214]:
            xo=(word>>1)&1023;reg=(word>>21)&31;ra=(word>>16)&31;rb=(word>>11)&31
            address=((self.g(ra) if ra else 0)+self.g(rb))&0xFFFFFFFF
            width=8 if xo in [84,214] else 4
            if xo in [20,84]:
                if getattr(self,'interfere',None) and address==self.interfere[0]:
                    _,value=self.interfere;put(self,address,value,width);self.interfere=None
                self.reservation=address;self.setg(reg,int.from_bytes(self.mem.rd(address,width),'big'))
                return nxt
            success=self.reservation==address and not self.failures
            if self.failures:self.failures-=1
            if success:put(self,address,self.g(reg),width)
            if hasattr(self,'atomic_trace'):
                self.atomic_trace.append((address,width,self.g(reg)&((1<<(width*8))-1),success))
            self.reservation=None;self.setcr(0,False,False,success)
            return nxt
        # Compiler-generated signed-divide guards; valid fixtures must not trap.
        if word>>26==3:
            to=(word>>21)&31;ra=(word>>16)&31;imm=word&0xFFFF
            if imm&0x8000:imm-=0x10000
            value=self.g(ra)&0xFFFFFFFF;signed=(value&0x7FFFFFFF)-(value&0x80000000)
            trap=(to&16 and signed<imm) or (to&8 and signed>imm) or (to&4 and signed==imm) or (to&2 and value<(imm&0xFFFFFFFF)) or (to&1 and value>(imm&0xFFFFFFFF))
            assert not trap,('unexpected native trap',hex(pc),hex(word))
            return nxt
        return super().step(pc,word,nxt,depth)

def put(c,address,value,width=4):
    c.mem.wr(address,(value&((1<<(width*8))-1)).to_bytes(width,'big'))

def cpu(bundle):
    c=bundle.cpu();c.__class__=AtomicCPU;c.msr=0x8000;c.reservation=None;c.failures=0
    c.atomic_trace=[];c.fence_trace=[];c.mem.add(0x84800000,0x4000)
    return c

def address(bundle,fragment):
    matches=set()
    for oi,obj in enumerate(bundle.objects):
        for name,value,section in obj.symbols.values():
            if (name.startswith(fragment) if fragment.startswith('?') else fragment in name) and (oi,section) in bundle.bases:
                matches.add(bundle.bases[(oi,section)]+value)
    assert len(matches)==1,(fragment,matches)
    return matches.pop()

def cstring(c,address):
    data=bytearray()
    while True:
        value=c.mem.rd(address+len(data),1)[0]
        if not value:return data.decode('utf-8',errors='replace')
        data.append(value);assert len(data)<0x2000
