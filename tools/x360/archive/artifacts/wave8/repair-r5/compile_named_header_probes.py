"""R5 actual typed header gates, emitted layout facts, and known native child negative."""
from datetime import datetime, timezone
import ast, hashlib, json, os, struct, subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent
WORK=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
META=json.loads((HERE/'native-parallel-wave8-r5-selected-stage1-patch.json').read_text())
RAW=json.loads((HERE/'selected-stage1/compile-results.json').read_text())
OUT=HERE/'named-header-probes'
assert not OUT.exists();OUT.mkdir();TEMP=OUT/'compiler-temp';TEMP.mkdir()
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def check():
 assert subprocess.check_output(['git','--no-optional-locks','-C',str(WORK),'rev-parse','HEAD']).decode().strip()==META['head']
 assert not subprocess.check_output(['git','--no-optional-locks','-C',str(WORK),'status','--porcelain=v1']).strip()
 for p,h in META['source_snapshot']['hashes'].items():assert sha(WORK/p)==h,p
old_script=HERE.parent/'repair-r4/compile_header_probes.py'
nodes=[n for n in ast.parse(old_script.read_text()).body if isinstance(n,ast.FunctionDef) and n.name=='symbol_words']
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(old_script),'exec'),globals())
combined=(HERE.parent/'repair-r4/header-probes-v2/CombinedLayouts.cpp').read_text()
typed='''#include "types.hpp"
#include <cstddef>
#define private public
#define protected public
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"
#include "GameSource/Replays/Serialisers/BrnReplayTrafficEntitySerialiser.h"
#include "GameSource/Resource/BrnDLCManager.h"
#undef private
#undef protected
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"
#include "rw/audio/core/Butterworth.h"
#include "rw/audio/core/plugins/HighPassButterworth.h"
#include "rw/audio/core/plugins/LowPassButterworth.h"
#include "GameSource/Effects/BrnEffectsDebrisColourRandomiser.h"
extern "C" const unsigned int R5TypedFacts[] = {
 sizeof(rw::BaseResourceDescriptors<5>), sizeof(rw::Resource), sizeof(renderengine::ProgramBufferParameters),
 sizeof(BrnReplays::BaseSerialiser), offsetof(BrnReplays::BaseSerialiser,maX360Extension18),
 offsetof(BrnReplays::BaseSerialiser,mpStaticBuffer),
 sizeof(rw::audio::core::Butterworth), offsetof(rw::audio::core::HighPassButterworth,mButterworth),
 offsetof(rw::audio::core::LowPassButterworth,mButterworth),
 sizeof(BrnEffects::Utils::DebrisColourRandomiser), sizeof(BrnResource::DLCBeatTheTeamGame),
 offsetof(BrnResource::DLCFeatureAvailability,mBeatTheTeamGame),
 offsetof(BrnResource::DLCFeatureAvailability,muAvailabilityMask),
 offsetof(BrnResource::DLCFeatureAvailability,mauPackMask),
 offsetof(BrnResource::DLCFeatureAvailability,maePackForFeature),
 offsetof(BrnResource::DLCFeatureAvailability,mabFeatureEnabled)
};
typedef void (BrnReplays::BaseSerialiser::*BaseCtor)(BrnReplays::ESerialiserId,BrnReplays::ESerialiserContext,s32,s32,const char*,bool);
BaseCtor gR5BaseCtor=&BrnReplays::BaseSerialiser::Construct;
typedef void (BrnReplays::TrafficEntitySerialiser::*TrafficCtor)();
TrafficCtor gR5TrafficCtor=&BrnReplays::TrafficEntitySerialiser::Construct;
typedef void (rw::audio::core::Butterworth::*Designer)(f32,u32,f32,rw::audio::core::Butterworth::FilterType,f32);
Designer gR5Designer=&rw::audio::core::Butterworth::CalculateFilterCoefficients;
Vector4 R5DebrisTypedCaller(BrnEffects::Utils::DebrisColourRandomiser* p,CgsNumeric::Random& r) {return p->Randomise(r);}
renderengine::ProgramBufferData* R5ProgramTypedCaller(rw::Resource* r,const renderengine::ProgramBufferParameters* p) {return renderengine::ProgramBuffer::Initialize(r,p);}
'''
negative='''#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModule.h"
CGS_STATIC_ASSERT((sizeof(BrnWorld::WorldEntityModule)==0x79f0),"Independent ARTIST WorldEntity79F0 remains negative");
'''
icepath=Path('D:/Reverse/BP-X360-Experimental-Artifacts/ice-geometry-a946-terra/geometry_containment_probe.cpp')
SOURCES={'CombinedLayouts':combined,'TypedABI':typed,'ICEContainment':icepath.read_text(),'WorldEntityNativeNegative':negative}
expected_layout=[0x5e3c80,0x80,0x18800,0x80,0x80,0x80,0x79e0,0x10,0x79800,0x80,0x80,16,20,16,8,12,16,4,20,64,8,12,48,112,320]
expected_typed=[40,20,40,0x5c,0x18,0x20,0x30,0x60,0x60,32,2,0,4,8,0x1c,0x80]
flags=[a for a in RAW['results'][0]['command'][1:-1] if not a.startswith(('/Fo','/Fa'))]+['/FAcs','/showIncludes']
check();rows=[]
for name,source in SOURCES.items():
 cpp,obj,asm,log=[OUT/(name+s) for s in ('.cpp','.obj','.asm','.log')];cpp.write_text(source,encoding='utf-8')
 cmd=[RAW['results'][0]['command'][0],*flags,'/Fo'+str(obj),'/Fa'+str(asm),str(cpp)]
 run=subprocess.run(cmd,cwd=WORK,env={**os.environ,'TEMP':str(TEMP),'TMP':str(TEMP)},capture_output=True,timeout=180)
 log.write_bytes(run.stdout+run.stderr);ok=run.returncode==0 and obj.exists() and struct.unpack_from('<H',obj.read_bytes())[0]==0x1f2
 neg=name=='WorldEntityNativeNegative';satisfied=(run.returncode!=0 and b'C2118' in log.read_bytes() and not obj.exists()) if neg else ok
 headers={}
 for line in log.read_text(errors='replace').splitlines():
  if 'including file:' in line:
   p=Path(line.split('including file:',1)[1].strip())
   if p.is_file():headers[str(p.resolve())]=sha(p)
 rows.append({'name':name,'command':cmd,'returncode':run.returncode,'actual_passed':ok,'expected_negative':neg,'gate_satisfied':satisfied,'source_sha256':sha(cpp),'artifacts':{str(p):sha(p) for p in (cpp,obj,asm,log) if p.exists()},'actual_included_headers':headers})
 print(name,run.returncode,'satisfied',satisfied,flush=True)
facts={}
if (OUT/'CombinedLayouts.obj').exists():facts['CombinedLayouts']=symbol_words(OUT/'CombinedLayouts.obj','R4LayoutFacts',25)
if (OUT/'TypedABI.obj').exists():facts['TypedABI']=symbol_words(OUT/'TypedABI.obj','R5TypedFacts',16)
if (OUT/'ICEContainment.obj').exists():facts['ICEContainment']=symbol_words(OUT/'ICEContainment.obj','geometry_containment',28)
check()
result={'head':META['head'],'rows':rows,'emitted_layout_facts':facts,'expected_CombinedLayouts':expected_layout,'expected_TypedABI':expected_typed,'source_inputs_rechecked':6299,'scope':'Fresh actual XDK header/typed-signature/layout compile; private/public access changed only in the diagnostic TU for offsetof. RCE/World production guards unchanged. DLC total9C declaration shape is not promoted to independently observed original sizeof. ICE physical mismatch and WorldEntity79E0!=79F0 remain negatives, not whole-layout proof.','created_utc':datetime.now(timezone.utc).isoformat()}
(OUT/'probe-results.json').write_text(json.dumps(result,indent=2))
assert all(r['gate_satisfied'] for r in rows)
assert facts['CombinedLayouts']==expected_layout,(facts['CombinedLayouts'],expected_layout)
assert facts['TypedABI']==expected_typed,(facts['TypedABI'],expected_typed)
assert facts['ICEContainment'][21]==0x12110 and facts['ICEContainment'][8]==0x7200 and facts['ICEContainment'][26]==0x36330 and facts['ICEContainment'][27]==0x38a40,facts['ICEContainment']
print('SATISFIED',sha(OUT/'probe-results.json'),flush=True)
