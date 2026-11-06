import collections,hashlib,json,re,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]/'b5-decomp'
assert subprocess.check_output(['git','branch','--show-current'],cwd=ROOT,text=True).strip()=='codex/experimental-x360-byte-match'
LOG=[]
PENDING=[]
def mask(s):
    pattern=r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
    return re.sub(pattern,lambda m:''.join('\n' if c=='\n' else ' ' for c in m[0]),s)
def pair(m,a,op='{',cl='}'):
    assert m[a]==op
    depth=0
    for b in range(a,len(m)):
        if m[b]==op:depth+=1
        elif m[b]==cl:
            depth-=1
            if depth==0:return b+1
    raise AssertionError(('Unclosed',a))
def line_start(s,a):return s.rfind('\n',0,a)+1
def cut(s,a,b,why):
    if not s[line_start(s,a):a].strip():a=line_start(s,a)
    if b<len(s) and s[b]=='\n':b+=1
    LOG.append({'kind':why,'line':s.count('\n',0,a)+1,'lines':s[a:b].count('\n')})
    return s[:a]+s[b:]
def function(s,name):
    m=mask(s);match=re.search(re.escape(name)+r'\s*\(',m);assert match,name
    a=match.start();brace=m.index('{',match.end());end=pair(m,brace)
    return cut(s,line_start(s,a),end,'function '+name)
def anon(s,tag,prefix=None):
    m=mask(s);at=m.index(tag);candidates=[]
    for n in re.finditer(r'\bnamespace\s*\{',m[:at]):
        brace=m.index('{',n.start());end=pair(m,brace)
        if end>at:candidates.append((n.start(),end))
    assert candidates,tag
    a,b=candidates[-1]
    assert not re.search(r'\b(?:RaceCarPhysics|VehicleManager)::\w+\s*\([^;{}]*\)\s*(?:const\s*)?\{',m[a:b]),tag
    if prefix:a=s.index(prefix)
    return cut(s,a,b,'PC namespace '+tag)
def statement(m,a):
    while m[a].isspace():a+=1
    if m[a]=='{':b=pair(m,a)
    elif re.match(r'if\b',m[a:]):
        op=m.index('(',a);cond=pair(m,op,'(',')');b=statement(m,cond)
    else:
        depth=0;b=a
        while b<len(m):
            if m[b] in '([':depth+=1
            elif m[b] in ')]':depth-=1
            elif m[b]==';' and depth==0:b+=1;break
            b+=1
        else:raise AssertionError('No semicolon')
    look=b
    while look<len(m) and m[look].isspace():look+=1
    if re.match(r'else\b',m[look:]):b=statement(m,look+4)
    return b
def ifs(s,conditions):
    count=0
    while True:
        m=mask(s);found=None
        for x in re.finditer(r'\bif\s*\(',m):
            op=m.index('(',x.start());end=pair(m,op,'(',')')
            cond=re.sub(r'\s+','',m[op+1:end-1])
            if cond in conditions:found=(x.start(),statement(m,end),cond);break
        if not found:return s,count
        a,b,cond=found;s=cut(s,a,b,'PC guard '+cond);count+=1

def block(s,tag,why):
    m=mask(s);at=m.index(tag);a=None;b=None
    for opening in (x.start() for x in re.finditer(r'\{',m[:at])):
        end=pair(m,opening)
        if end>at:a,b=opening,end
    assert a is not None,tag
    return cut(s,a,b,why)
def literal(s,old,new='',count=1):
    assert s.count(old)==count,(old,s.count(old),count)
    return s.replace(old,new)
def call_counts(s):
    m=mask(s)
    return {name:len(re.findall(r'\b'+name+r'\s*\(',m)) for name in ['CGS_ASSERT','CGS_STATIC_ASSERT','AddAirRam','AddWorldSpaceForce','AddWorldSpaceAngularImpulse','AddLocalImpulse','AddEvent','AddEventSafe','SetRaceCarCrashing','InstantTakedown']}
def write(name,before,after,raw):
    assert call_counts(before)==call_counts(after),(name,call_counts(before),call_counts(after))
    path=ROOT/name
    ending='\r\n' if b'\r\n' in raw else '\n'
    PENDING.append((path,raw,after.replace('\n',ending).encode('utf-8')))
    print(name,'removed',before.count('\n')-after.count('\n'),'lines')

name='src/GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.cpp'
raw=(ROOT/name).read_bytes();s=raw.decode('utf-8').replace('\r\n','\n');before=s
s=anon(s,'struct AtGateCensus',prefix='    // ---- [at-gate] AFTERTOUCH GATE CENSUS')
s=anon(s,'gaTdSlamGate')
s=function(s,'RaceCarPhysics::FindTdSlamGateSample')
s=anon(s,'struct TdReactWatchSlot')
s=function(s,'RaceCarPhysics::TdReactWatch')
s=function(s,'RaceCarPhysics::TdReactStep')
s=anon(s,'struct TargetAssistWitness')
a=s.index('    // [showtime-watch]');b=s.index('    void RaceCarPhysics::Update(',a);s=cut(s,a,b,'PC showtime history')
a=s.index('    // ⭐⭐⭐ MEASURED AND CLEARED');b=s.index('    // REWRITTEN 2026-08-24 (showtime wave) from a fresh full disassembly',a);s=cut(s,a,b,'PC aftertouch census history')
s=block(s,'sbWasInShowtime','PC showtime counters')
s,n=ifs(s,{'TdSlamGateArmed()','AtGateArmed()&&IsCrashing()','lbAtGate','lbWitness','CgsDev::Log::gpDebugPrint!=0'});assert n==19,n
s=literal(s,'        const bool lbAtGate = AtGateArmed();\n')
s=literal(s,'        TargetAssistWitness lWitness = {};\n')
s=literal(s,'        lWitness.mbAirGate = true;\n')
s=re.sub(r'(?m)^        // ---- \[at-gate\] CALLER-SIDE CENSUS\.[\s\S]*?^        // the probe never ran"\. Reads only; changes nothing the console does\.\n','',s)
s=re.sub(r'(?m)^        // ---- \[at-gate\] CALLEE-SIDE CENSUS[^\n]*\n(?:        //[^\n]*\n){2}','',s)
s=re.sub(r'(?m)^        // \[tassist\] witness record[^\n]*\n','',s)
s=re.sub(r'(?m)^                    // \[showtime\] one-shot[^\n]*\n(?:                    // once-per-mode-change[^\n]*\n)?','',s)
s=re.sub(r'(?m)^        // \[DIAG\] BRN_STRENGTH_STAT_OVERRIDE[^\n]*\n(?:        //[^\n]*\n)*        \{\n        \}\n','',s)
for line in ['#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // [showtime-probe] CgsDev::Log::gpDebugPrint\n','#include <cstdlib>   // [showtime-probe] getenv\n','#include <cstdio>    // opt-in Showtime input/force witness\n']:
    s=literal(s,line)
for tag in ['AtGate','TdSlamGate','TdReact','TargetAssistWitness','lWitness','gpDebugPrint']:
    assert tag not in mask(s),tag
write(name,before,s,raw)

name='src/GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h'
raw=(ROOT/name).read_bytes();s=raw.decode('utf-8').replace('\r\n','\n');before=s
s=literal(s,'        static const TdSlamGateSample* FindTdSlamGateSample(const RaceCarPhysics* lpCar);\n')
s=literal(s,'        void TdReactStep(const BrnPlayerDriverControls* lpControls) const;\n')
write(name,before,s,raw)

name='src/GameSource/Physics/VehicleManager/BrnVehicleManager.cpp'
raw=(ROOT/name).read_bytes();s=raw.decode('utf-8').replace('\r\n','\n');before=s
for f in ['WitnessImpactTypeName','WitnessTakedownTypeName','WitnessEntityOwnerName']:s=function(s,f)
s=block(s,'saiWitnessed','PC contact allowance')
s=block(s,'sbBankReported','PC surface-bank report')
s,n=ifs(s,{'lbWitness'});assert n==11,n
s=literal(s,'        const RaceCarPhysics::TdSlamGateSample* lpWGateB = 0;\n')
s=literal(s,'        s32 liWGap = -1;\n')
a=s.index('        auto lWitnessEpisode =');m=mask(s);b=pair(m,m.index('{',a));assert s[b]==';';s=cut(s,a,b+1,'PC impact episode lambda')
s=s.replace('lfWitnessSpeedSum','lfSpeedSum')
s=literal(s,'        const bool lbStationary = CheckForStationaryTargetTakedown(lpInfo);','        CheckForStationaryTargetTakedown(lpInfo);')
a=s.index('    // [PC HARNESS, NOT X360]');b=s.index('    // Game event 31:',a);s=cut(s,a,b,'PC impact witness history')
s=re.sub(r'(?m)^    // \[surface-bank\][^\n]*\n    //[^\n]*\n','',s)
s=re.sub(r'(?m)^        // \[td-(?:tbone|vert)\][^\n]*\n','',s)
for line in ['#include "GameShared/GameClasses/Development/Log/CgsLog.h"                     // gpDebugPrint ([bringup] crash-entry banner)\n','#include "GameSource/World/BrnEntityTypes.h"                                   // BrnWorld::EEntityTypeID -- the owner byte the crash witness names\n','#include <cstdlib>  // getenv: opt-in PC contact diagnostics\n','#include <cstring>  // std::memcpy: the [td-replay] witness prints raw IEEE bits\n']:
    s=literal(s,line)
for tag in ['Witness','TdSlamGate','gpDebugPrint']:
    assert tag not in mask(s),tag
write(name,before,s,raw)
for path,raw,result in PENDING:
    assert path.read_bytes()==raw
for path,raw,result in PENDING:path.write_bytes(result)
(ROOT.parent/'scratch/x360_sdk/vehicle-pc-removal.json').write_text(json.dumps(LOG,indent=2)+'\n',encoding='utf-8')
