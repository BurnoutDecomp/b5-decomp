import json,math,random,re,struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent))
import validate_native_math as M
out=M.ROOT/'b5-decomp/build/x360/native-resource-validation'
data=(out/'resource-math.dll').read_bytes();pe=struct.unpack_from('<I',data,0x3c)[0]
assert data[pe:pe+4]==b'PE\0\0' and struct.unpack_from('<H',data,pe+4)[0]==0x1f2
count=struct.unpack_from('<H',data,pe+6)[0];optional=pe+24
base=struct.unpack_from('<I',data,optional+28)[0];table=optional+struct.unpack_from('<H',data,pe+20)[0]
for i in range(count):
    virtual,rva,size,offset=struct.unpack_from('<IIII',data,table+i*40+8)
    body=bytearray(max(virtual,size));body[:size]=data[offset:offset+size]
    M.SECTIONS.append((base+rva,body))
symbols={}
for line in (out/'resource-math.map').read_text().splitlines():
    m=re.match(r'\s+\d{4}:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{8})\s',line)
    if m:symbols[m[1]]=int(m[2],16)
Return=M.CPU.run.__globals__['Return']
class WindowCPU(M.CPU):
    def fetch(self,address):
        if address==self.stop:raise Return
        return super().fetch(address)
def original(stop):
    cpu=M.new_cpu();cpu.__class__=WindowCPU;cpu.stop=stop
    return cpu
random.seed(360)
special=[0.,-0.,1.,-1.,.5,-.5,math.pi,-math.pi,M.value(1),M.value(0x80000001),M.value(0x007fffff),M.value(0x00800000),M.value(0x7f7fffff),math.inf,-math.inf,math.nan]
cases=[(a,b) for a in special for b in special]
cases += [(M.value(M.bits(random.uniform(-100,100))),M.value(M.bits(random.uniform(-100,100)))) for _ in range(256)]
errors=[];checks=[]
for a,b in cases:
    orig=original(0x822b2130);orig.setg(3,M.S.SELF);orig.setg(4,M.S.SELF+4);orig.setvf(2,[b,0,a,0]);orig.setvf(119,[0]*4);orig.setf(12,0);orig.setf(13,0)
    orig.run(0x822b2030);result=M.value(orig.rd32(M.S.STACK+0x10000+0x70))
    compiled=M.new_cpu();compiled.setf(1,a);compiled.setf(2,b);compiled.run(symbols['NativePowerATan2'])
    if not M.same(result,compiled.f(1)):errors.append({'function':'ATan2','a':hex(M.bits(a)),'b':hex(M.bits(b)),'original':hex(M.bits(result)),'compiled':hex(M.bits(compiled.f(1)))})
checks.append({'function':'PowerParking ATan2','cases':len(cases)})
print('ATan2',len(cases),'checked',flush=True)
for state in special+[.1,.8,2.]:
    for timestep in special+[.016,.1,2.]:
        for boosting in [False,True]:
            orig=original(0x82251a78);orig.setg(11,M.S.SELF);orig.setf(0,timestep);orig.setf(30,0);orig.setf(31,1);orig.setcr(6,False,False,not boosting);orig.mem.wr(M.S.SELF+4,struct.pack('>f',state));orig.run(0x82251a40)
            compiled=M.new_cpu();compiled.setg(3,M.S.SELF);compiled.setg(4,int(boosting));compiled.setf(1,timestep);compiled.mem.wr(M.S.SELF+4,struct.pack('>f',state));compiled.run(symbols['NativeTempBoost'])
            a=M.value(orig.rd32(M.S.SELF+4));b=M.value(compiled.rd32(M.S.SELF+4))
            if not M.same(a,b):errors.append({'function':'TempBoost','state':hex(M.bits(state)),'timestep':hex(M.bits(timestep)),'boosting':boosting,'original':hex(M.bits(a)),'compiled':hex(M.bits(b))})
checks.append({'function':'TempBoost Update','cases':(len(special)+3)**2*2})
report={'kind':'compiled PPC vs original ARTIST instruction windows','checks':checks,'mismatches':errors,'hardware_estimates':'same interpreter models on both sides','nan_payloads_compared':False,'game_booted':False}
(out/'comparison.json').write_text(json.dumps(report,indent=2)+'\n')
print('Resource math cases',sum(x['cases'] for x in checks),'mismatches',len(errors))
if errors:print(json.dumps(errors[:8],indent=2))
raise SystemExit(bool(errors))
