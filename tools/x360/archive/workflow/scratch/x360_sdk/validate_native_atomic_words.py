"""Actual native AtomicInt instruction/ABI checks against independent word contracts."""
import hashlib,json,random
from pathlib import Path
import native_atomic_fixture as F
ROOT=Path(__file__).resolve().parents[2]/'b5-decomp'
OUT=ROOT/'build/x360/native-frame-final'
row=next(r for r in json.loads((OUT/'compile-results.json').read_text())['results'] if r['source'].endswith('native-atomic-probes.cpp'))
path=Path(row['object'])
rng=random.Random(360_1030_7)
errors=[];groups=[]
for symbol,width in [('AtomicS32',4),('AtomicU32',4),('AtomicS64',8),('AtomicU64',8)]:
    obj=F.Coff(path,symbol);mask=(1<<(width*8))-1
    values=[0,1,2,mask,mask-1,mask>>1,(mask>>1)+1,0x100000000&mask]+[rng.getrandbits(width*8) for _ in range(24)]
    for operation in range(14):
        for index,initial in enumerate(values):
            value=values[(index+3)%len(values)];condition=initial if index%2 else value
            c=F.cpu(obj);c.failures=index%3;c.mem.wr(F.M.S.SELF,b'\xCD'*0x40);F.put(c,F.M.S.SELF,initial,width)
            c.setg(3,F.M.S.SELF);c.setg(4,operation);c.setg(5,value);c.setg(6,condition)
            c.run(obj.entry)
            result=initial;stored=initial
            if operation==0:stored=value
            elif operation==1:
                result=int(initial==condition)
                if result:stored=value
            elif operation==2:result=stored=(initial+1)&mask
            elif operation==3:result=stored=(initial-1)&mask
            elif operation==4:result=stored=(initial+value)&mask
            elif operation==5:result=stored=value
            elif operation==6:stored=(initial+1)&mask
            elif operation==7:stored=(initial-1)&mask
            elif operation==8:result=stored=value
            elif operation==11:result=stored=(initial+value)&mask
            elif operation==12:result=stored=(initial-value)&mask
            actual=c.g(3)&mask;actual_stored=int.from_bytes(c.mem.rd(F.M.S.SELF,width),'big')
            if (actual,actual_stored)!=(result,stored) or c.msr!=0x8000 or c.mem.rd(F.M.S.SELF+width,0x40-width)!=b'\xCD'*(0x40-width) or any(t[1]!=width for t in c.atomic_trace):
                errors.append({'symbol':symbol,'op':operation,'index':index,'actual':[actual,actual_stored],
                               'expected':[result,stored],'atomic':c.atomic_trace,'msr':c.msr})
    count=14*len(values);groups.append({'symbol':symbol,'cases':count});print(symbol,count,'checked',flush=True)
report={'cases':sum(g['cases'] for g in groups),'groups':groups,'mismatches':errors,
        'object':str(path),'object_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
        'scope':'actual native32/64 signed/unsigned AtomicInt template code,not a C++model; independent modulo-word return/store contracts for exchange,CAS,increment,decrement,add,assignment,postfix,constructors,reads,plus/minus; exact ABI,padding,MSR and failed reservations',
        'limits':'API contract checks using real XDK instructions; no standalone original AtomicInt exports or physical concurrency claim; original integrated32/64 operation comparisons are in frame/barrier/data-stream reports'}
(OUT/'atomic-word-comparison.json').write_text(json.dumps(report,indent=2)+'\n')
print(report['cases'],'checks;',len(errors),'mismatches')
if errors:print(json.dumps(errors[:3],indent=2))
raise SystemExit(bool(errors))
