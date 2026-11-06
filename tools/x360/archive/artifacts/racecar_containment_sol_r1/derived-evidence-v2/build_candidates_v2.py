from pathlib import Path
import concurrent.futures,hashlib,importlib.util,json,os,re,struct,subprocess,sys

ROOT=Path('D:/codex-native/rce-constraints-sol')
CANON=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp')
OUT=Path(__file__).resolve().parent
BASE='d13668139fd3bf2a60213b662c3b7873855a8160'
RH='src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h'
SH='src/GameSource/Replays/Serialisers/BrnReplayRaceCarEntitySerialiser.h'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(['git','-C',str(ROOT),*args])
assert git('rev-parse','HEAD').decode().strip()==BASE
kind=sys.argv[1]
assert kind in ['class64','class128','members64','members128','provider-baseline']
stage=OUT/('candidate-'+kind+'-v2');stage.mkdir(exist_ok=False)
original={p:git('show',BASE+':'+p).replace(b'\r\n',b'\n').replace(b'\n',b'\r\n') for p in [RH,SH]}
prior=OUT/'lab-current-inputs.json'
if prior.exists():
    for p,h in json.loads(prior.read_text()).items():assert sha(ROOT/p)==h,(p,'unexpected lab mutation')
else:
    assert not git('status','--porcelain').strip()
candidate=dict(original)
if kind.startswith('class'):
    alignment=int(kind[5:]);old=b'class RaceCarEntitySerialiser : public BaseSerialiser'
    assert candidate[SH].count(old)==1
    candidate[SH]=candidate[SH].replace(old,('class CGS_ALIGN(%d) RaceCarEntitySerialiser : public BaseSerialiser'%alignment).encode())
elif kind.startswith('members'):
    alignment=int(kind[7:])
    old=b'    BrnReplays::RaceCarEntitySerialiser mReplaySerialiser;'
    assert candidate[RH].count(old)==1
    candidate[RH]=candidate[RH].replace(old,b'    CGS_ALIGN(16) BrnReplays::RaceCarEntitySerialiser mReplaySerialiser;')
    old=b'    Matrix44Affine mReplayTransform;'
    assert candidate[RH].count(old)==1
    candidate[RH]=candidate[RH].replace(old,('    CGS_ALIGN(%d) Matrix44Affine mReplayTransform;'%alignment).encode())
for p,b in candidate.items():(ROOT/p).write_bytes(b)
pins={p:sha(ROOT/p) for p in candidate}
prior.write_text(json.dumps(pins,indent=2)+'\n')
(stage/'candidate.patch').write_bytes(git('diff','--binary','--',RH,SH))
for p,b in candidate.items():
    q=stage/'source-snapshot'/p;q.parent.mkdir(parents=True,exist_ok=True);q.write_bytes(b)
spec=importlib.util.spec_from_file_location('builder',ROOT/'tools/build_x360.py')
builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
sdk=CANON/'build/x360/sdk-6534.4/portable/XDK';compiler=sdk/'bin/win32/cl.exe'
flags=builder.FLAGS+['/Z7','/showIncludes']
flags+=['/I'+str(ROOT/p) for p in builder.INCLUDES if (ROOT/p).is_dir()]
flags+=['/I'+str(CANON/'vendor'/p) for p in ['EABase/include/Common','EASTL/include','EAThread/include']]
flags+=['/I'+str(sdk/'include/xbox')]
temp=stage/'temp';temp.mkdir()
env=dict(os.environ);env.update(TEMP=str(temp),TMP=str(temp),XEDK=str(sdk),PATH=str(sdk/'bin/win32')+os.pathsep+str(Path(os.environ['SystemRoot'])/'System32'))
for n in ['CL','_CL_','LINK','_LINK_']:env.pop(n,None)
sources=[]
for query in ['BrnRaceCarEntityModule.h','BrnWorldModule.h']:
    paths=git('grep','-l','-F',query,'--','src/**/*.cpp').decode('utf-8').splitlines()
    for rel in paths:
        if 'pc' in Path(rel).parts:continue
        if any('#include' in line and query in line for line in (ROOT/rel).read_text(encoding='utf-8-sig',errors='replace').splitlines()):sources.append(rel)
sources+=['src/GameSource/Replays/Serialisers/BrnReplayRaceCarEntitySerialiser.cpp',
          'src/GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRCEntityActiveRaceCarOutputInterface.cpp']
sources=list(dict.fromkeys(sources))
if kind=='provider-baseline':sources=['src/GameSource/Replays/Serialisers/BrnReplayRaceCarEntitySerialiser.cpp']
probe=stage/'candidate-probe.cpp'
if kind!='provider-baseline':
    probe.write_text('''#include "types.hpp"
#define private public
#define protected public
#include "GameSource/World/BrnWorldModule.h"
#undef protected
#undef private
#include <new>
typedef BrnReplays::RaceCarEntitySerialiser Serialiser;
extern "C" const u32 gCandidateLayout[] = {
sizeof(Serialiser),__alignof(Serialiser),sizeof(BrnWorld::RaceCarEntityModule),__alignof(BrnWorld::RaceCarEntityModule),
offsetof(BrnWorld::RaceCarEntityModule,mReplaySerialiser),offsetof(BrnWorld::RaceCarEntityModule,mReplayTransform),
offsetof(BrnWorld::RaceCarEntityModule,mbInvalidatedRaceCars),offsetof(BrnWorld::RaceCarEntityModule,mabRaceCarsRendered),
offsetof(BrnWorld::RaceCarEntityModule,DEBUG_mbOverrideDamage),offsetof(BrnWorld::WorldModule,mRaceCarEntityModule),
offsetof(BrnWorld::WorldModule,mTrafficEntityModule),sizeof(BrnReplays::BaseSerialiser),__alignof(BrnReplays::BaseSerialiser)
};
extern "C" Serialiser* DiagnosticNext(Serialiser* p) { return p+1; }
extern "C" void DiagnosticDefault(Serialiser* p) { new(p) Serialiser; }
extern "C" void DiagnosticValueInit(Serialiser* p) { new(p) Serialiser(); }
extern "C" void DiagnosticCopy(Serialiser* to,const Serialiser* from) { new(to) Serialiser(*from); }
extern "C" void DiagnosticAssign(Serialiser* to,const Serialiser* from) { *to=*from; }
''',encoding='utf-8')
    sources.append(str(probe))
source_pins={p:sha(Path(p) if Path(p).is_absolute() else ROOT/p) for p in sources}
def build(p):
    path=Path(p) if Path(p).is_absolute() else ROOT/p
    key=path.stem+'-'+hashlib.sha256(p.encode()).hexdigest()[:8]
    obj=stage/(key+'.obj');asm=stage/(key+'.asm');log=stage/(key+'.log')
    command=[str(compiler)]+flags+['/Fo'+str(obj),'/Fa'+str(asm),str(path)]
    r=subprocess.run(command,cwd=ROOT,env=env,stdin=subprocess.DEVNULL,capture_output=True,text=True,errors='replace',timeout=180,creationflags=subprocess.CREATE_NO_WINDOW)
    text=r.stdout+r.stderr;log.write_text(text,encoding='utf-8')
    inputs={}
    for line in text.splitlines():
        if 'including file:' in line:
            q=Path(line.split('including file:',1)[1].strip());assert q.is_file();inputs[str(q.resolve())]=sha(q)
    errors=[l for l in text.splitlines() if 'error ' in l]
    passed=r.returncode==0 and obj.is_file() and struct.unpack('<H',obj.read_bytes()[:2])[0]==0x1F2
    return {'source':p,'source_sha256':source_pins[p],'passed':passed,'returncode':r.returncode,'errors':errors,
            'command':command,'object':str(obj) if obj.exists() else None,'object_sha256':sha(obj) if obj.exists() else None,
            'assembly':str(asm) if asm.exists() else None,'log':str(log),'inputs':inputs}
report={'base':BASE,'lab':str(ROOT),'kind':kind,'inference_only':True,'candidate_paths':pins,'driver_sha256':sha(__file__),'compiler_sha256':sha(compiler),'results':[]}
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    for row in pool.map(build,sources):
        report['results'].append(row)
        print(row['passed'],row['source'],'errors',len(row['errors']),flush=True)
        if row['errors']:print('\n'.join(row['errors'][:2]),flush=True)
        (stage/'compile-results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
assert pins=={p:sha(ROOT/p) for p in candidate}
assert source_pins=={p:sha(Path(p) if Path(p).is_absolute() else ROOT/p) for p in sources}
report['passed']=sum(r['passed'] for r in report['results']);report['failed']=len(report['results'])-report['passed']
(stage/'compile-results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('TERMINAL',kind,report['passed'],report['failed'],flush=True)
