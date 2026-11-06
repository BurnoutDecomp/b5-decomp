"""Fresh R7 typed/header/layout safeguards; expected negatives remain failures."""
from datetime import datetime,timezone
import ast,hashlib,json,os,struct,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent;WORK=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake');ART=HERE.parent.parent
META=json.loads((HERE/'native-parallel-wave8-r7-selected-stage1-patch.json').read_text());RAW=json.loads((HERE/'selected-stage1/compile-results.json').read_text());OUT=HERE/'named-native-gates';assert not OUT.exists();OUT.mkdir();TEMP=OUT/'compiler-temp';TEMP.mkdir()
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def check():
 assert subprocess.check_output(['git','--no-optional-locks','-C',str(WORK),'rev-parse','HEAD']).decode().strip()==META['head']
 assert not subprocess.check_output(['git','--no-optional-locks','-C',str(WORK),'status','--porcelain=v1']).strip()
 for p,h in META['source_snapshot']['hashes'].items():assert sha(WORK/p)==h,p
fn=HERE.parent/'repair-r4/compile_header_probes.py';nodes=[n for n in ast.parse(fn.read_text()).body if isinstance(n,ast.FunctionDef)and n.name=='symbol_words'];exec(compile(ast.Module(body=nodes,type_ignores=[]),str(fn),'exec'),globals())
typed='''#include "types.hpp"
#include <cstddef>
#define private public
#define protected public
#include "GameSource/Effects/EffectsModule.h"
#include "GameSource/Replays/Serialisers/BrnReplayEffectsSerialiser.h"
#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManager.h"
#include "vendor/renderware/collision/ClusteredMesh.hpp"
#include "GameSource/Director/Camera/Camera.h"
#include "GameSource/Gui/Flow/HUD/States/BrnRaceMainHudState.h"
#include "GameSource/Gui/BrnGuiCache.h"
#include "GameSource/Gui/Events/BrnGuiMugshotControlEvent.h"
#undef protected
#undef private
extern "C" const unsigned int R7TypedFacts[] = {
 sizeof(BrnEffects::EffectsModule),sizeof(BrnReplays::EffectsSerialiser),offsetof(BrnEffects::EffectsModule,mEffectsSerialiser),
 sizeof(CgsSceneManager::CacheSlot),__alignof(CgsSceneManager::CacheSlot),offsetof(CgsSceneManager::CacheSlot,mLastCachedSphere),offsetof(CgsSceneManager::CacheSlot,mInnerSpherePositionAndRadius),offsetof(CgsSceneManager::CacheSlot,miIndexIntoTriangleCache),offsetof(CgsSceneManager::CacheSlot,miNumCachedTriangleBatches),offsetof(CgsSceneManager::CacheSlot,miOverflow),
 sizeof(rw::collision::Aggregate),offsetof(rw::collision::ClusteredMesh,mpKDTree),offsetof(rw::collision::ClusteredMesh,mpCluster),
 sizeof(((BrnDirector::Camera::Camera*)0)->mState_uFlags),offsetof(BrnDirector::Camera::Camera,mState_uFlags),
 sizeof(BrnGui::GuiMugshotControlEvent),offsetof(BrnGui::GuiMugshotControlEvent,meResponse),offsetof(BrnGui::GuiMugshotControlEvent,meImageType),offsetof(BrnGui::GuiMugshotControlEvent,meOpponentARCI),offsetof(BrnGui::GuiMugshotControlEvent,mbLocalAggressor),sizeof(BrnGui::RaceMainHudState)
};
typedef void (BrnReplays::EffectsSerialiser::*EffectsCtor)();
EffectsCtor gR7EffectsCtor=&BrnReplays::EffectsSerialiser::Construct;
'''
hud=(ART/'race-hud-native-root/layout_probe.cpp').read_text();positive_hud='\n'.join(line for line in hud.splitlines()if not line.startswith('PIN(HudSize,'))+'\n'
sources={'CombinedLayouts':(HERE.parent/'repair-r6/named-native-gates/CombinedLayouts.cpp').read_text(),'TypedLayouts':typed,'CameraFlags':(ART/'camera-flags-terra/camera_probe.cpp').read_text(),'HUDMemberGuards':positive_hud,'HUDAllocationNegative':hud,'WorldEntityNativeNegative':'#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModule.h"\nCGS_STATIC_ASSERT((sizeof(BrnWorld::WorldEntityModule)==0x79f0),"Independent native child79F0 negative");\n'}
flags=[a for a in RAW['results'][0]['command'][1:-1]if not a.startswith(('/Fo','/Fa'))]+['/FAcs','/showIncludes'];check();rows=[]
for name,text in sources.items():
 cpp,obj,asm,log=[OUT/(name+s)for s in ['.cpp','.obj','.asm','.log']];cpp.write_text(text,encoding='utf-8');cmd=[RAW['results'][0]['command'][0],*flags,'/Fo'+str(obj),'/Fa'+str(asm),str(cpp)]
 r=subprocess.run(cmd,cwd=WORK,env={**os.environ,'TEMP':str(TEMP),'TMP':str(TEMP)},capture_output=True,timeout=180);log.write_bytes(r.stdout+r.stderr)
 ok=r.returncode==0 and obj.exists()and struct.unpack_from('<H',obj.read_bytes())[0]==0x1f2;neg=name in ['HUDAllocationNegative','WorldEntityNativeNegative'];errors=[s for s in log.read_text(errors='replace').splitlines()if 'error C'in s];satisfied=(r.returncode!=0 and len(errors)==1 and 'C2118'in errors[0]and not obj.exists())if neg else ok
 headers={}
 for line in log.read_text(errors='replace').splitlines():
  if 'including file:'in line:
   p=Path(line.split('including file:',1)[1].strip())
   if p.is_file():headers[str(p.resolve())]=sha(p)
 rows.append({'name':name,'command':cmd,'returncode':r.returncode,'passed':ok,'expected_negative':neg,'gate_satisfied':satisfied,'source':str(cpp),'source_sha256':sha(cpp),'object':str(obj)if obj.exists()else None,'artifacts':{str(p):sha(p)for p in [cpp,obj,asm,log]if p.exists()},'actual_included_headers':headers,'diagnostics':errors});print(name,r.returncode,'satisfied',satisfied,flush=True)
facts={}
for name,symbol,count in [('CombinedLayouts','R4LayoutFacts',25),('TypedLayouts','R7TypedFacts',21),('HUDMemberGuards','HudNativeOffsets',8)]:
 if(OUT/(name+'.obj')).exists():facts[name]=symbol_words(OUT/(name+'.obj'),symbol,count)
check();expected=[0x2f600,0x5c,0x2f550,0x30,16,0,0x10,0x24,0x28,0x2c,0x30,0x30,0x34,8,0x140,24,8,12,16,20,0x7920]
result={'head':META['head'],'rows':rows,'emitted_facts':facts,'typed_expected':expected,'scope':'Current headers and actual PPC emitted data. Positive diagnostic HUD members omit only the separate known allocation-size assertion; the original negative probe is also compiled unchanged. No production guards removed. Cache30/al16 and Mesh known prefix only; no full cache/mesh/parent runtime or HUD allocation closure.','source_inputs_rechecked':6299,'created_utc':datetime.now(timezone.utc).isoformat()}
with(OUT/'gate-results.json').open('x')as f:json.dump(result,f,indent=2)
assert all(r['gate_satisfied']for r in rows)
assert facts['TypedLayouts']==expected,(facts['TypedLayouts'],expected)
assert facts['CombinedLayouts']==json.loads((HERE.parent/'repair-r6/named-native-gates/gate-results.json').read_text())['emitted_facts']['CombinedLayouts']
print('SATISFIED',sha(OUT/'gate-results.json'),flush=True)
