"""Actual R6 native ABI/layout gates and two real providers needed by owner models."""
from datetime import datetime,timezone
import ast,hashlib,json,os,struct,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent;WORK=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
META=json.loads((HERE/'native-parallel-wave8-r6-selected-stage1-patch.json').read_text());RAW=json.loads((HERE/'selected-stage1/compile-results.json').read_text())
OUT=HERE/'named-native-gates';assert not OUT.exists();OUT.mkdir();TEMP=OUT/'compiler-temp';TEMP.mkdir()
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def check():
 assert subprocess.check_output(['git','--no-optional-locks','-C',str(WORK),'rev-parse','HEAD']).decode().strip()==META['head']
 assert not subprocess.check_output(['git','--no-optional-locks','-C',str(WORK),'status','--porcelain=v1']).strip()
 for p,h in META['source_snapshot']['hashes'].items():assert sha(WORK/p)==h,p
nodes=[n for n in ast.parse((HERE.parent/'repair-r4/compile_header_probes.py').read_text()).body if isinstance(n,ast.FunctionDef) and n.name=='symbol_words']
exec(compile(ast.Module(body=nodes,type_ignores=[]),'retained-coff-data-reader','exec'),globals())
combined=(HERE.parent/'repair-r5/named-header-probes/CombinedLayouts.cpp').read_text()
gui='''#include "types.hpp"
#include <cstddef>
#define private public
#define protected public
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameSource/Gui/BrnGuiVideoEvents.h"
#undef private
#undef protected
#include "GameShared/GameClasses/Development/PerfMon/Gpu/CgsPerfMonGpu.h"
#include "GameShared/GameClasses/Geometric/Intersection/CgsTriangleBox.h"
#include "GameSource/Physics/PhysicsUtilities/ExternalPhysicsBody.h"
extern "C" const unsigned int R6GuiFacts[] = {
 sizeof(BrnGui::GuiOverlayRequest),__alignof(BrnGui::GuiOverlayRequest),sizeof(CgsGui::GuiPopupParameter),
 offsetof(BrnGui::GuiOverlayRequest,mOverlayID),offsetof(BrnGui::GuiOverlayRequest,maMessageParams),
 offsetof(BrnGui::GuiOverlayRequest,mButton1Param),offsetof(BrnGui::GuiOverlayRequest,mButton2Param),
 offsetof(BrnGui::GuiOverlayRequest,miNumMessages),offsetof(BrnGui::GuiOverlayRequest,mbButton1Used),offsetof(BrnGui::GuiOverlayRequest,mbButton2Used),
 sizeof(BrnGui::GuiEventPlayVideo),sizeof(BrnGui::GuiEventStopVideo),sizeof(BrnGui::MovieManager::VideoDefinition),
 sizeof(CgsDev::PerfMonGpuInstance),offsetof(CgsDev::PerfMonGpuInstance,macName),offsetof(CgsDev::PerfMonGpuInstance,mColour),offsetof(CgsDev::PerfMonGpuInstance,mu64Counter),offsetof(CgsDev::PerfMonGpuInstance,mfGPUBudget),
 sizeof(CgsGeometric::BT::TriangleData),__alignof(CgsGeometric::BT::TriangleData)
};
typedef void (CgsGeometric::BT::TriangleData::*TriangleCtor)(Vector3,Vector3,Vector3);
TriangleCtor gR6TriangleCtor=&CgsGeometric::BT::TriangleData::Construct;
typedef void (BrnPhysics::ExternalPhysicsBody::*MassSetter)(VecFloat);
MassSetter gR6MassSetter=&BrnPhysics::ExternalPhysicsBody::SetMass;
typedef void (BrnGui::GuiOverlayRequest::*OverlayCtor)(const char*);
OverlayCtor gR6OverlayCtor=&BrnGui::GuiOverlayRequest::Construct;
'''
boot_path=Path('D:/codex-native/bootlegal-terra/native_bootlegal_probe.cpp');boot=boot_path.read_text()
freq_path=Path('D:/Reverse/BP-X360-Experimental-Artifacts/perfmon-gpu-surface-r4/observed-main-frequency.cpp');frequency=freq_path.read_text()
actual_main=WORK/'src/GameSource/Game/BrnGameModule.cpp'
def branch(s):
 start=s.index('if (liRefreshRate == 50)');end=s.index('CgsDev::Assert::EndAssert();',start)+len('CgsDev::Assert::EndAssert();');end=s.index('}',end)+1
 return '\n'.join(line.strip() for line in s[start:end].splitlines() if line.strip())
assert branch(frequency)==branch(actual_main.read_text())
negative='''#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModule.h"
CGS_STATIC_ASSERT((sizeof(BrnWorld::WorldEntityModule)==0x79f0),"Independent WorldEntity79F0 native guard remains negative");
'''
SOURCES={'CombinedLayouts':combined,'GuiAndTypedABI':gui,'BootLegalLayout':boot,'ObservedMainFrequencyBranch':frequency,'WorldEntityNativeNegative':negative}
flags=[a for a in RAW['results'][0]['command'][1:-1] if not a.startswith(('/Fo','/Fa'))]+['/FAcs','/showIncludes']
native=['src/SDKs/EATech/rw/math/vpu/vecfloat.cpp','src/GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.cpp']
jobs=[(Path(p).stem,p,False)for p in native]
for n,text in SOURCES.items():
 p=OUT/(n+'.cpp');p.write_text(text,encoding='utf-8');jobs.append((n,str(p),n=='WorldEntityNativeNegative'))
check();rows=[]
for name,source,negative_expected in jobs:
 obj,asm,log=[OUT/(name+s)for s in ['.obj','.asm','.log']]
 cmd=[RAW['results'][0]['command'][0],*flags,'/Fo'+str(obj),'/Fa'+str(asm),str(Path(source))]
 r=subprocess.run(cmd,cwd=WORK,env={**os.environ,'TEMP':str(TEMP),'TMP':str(TEMP)},capture_output=True,timeout=180)
 log.write_bytes(r.stdout+r.stderr);ok=r.returncode==0 and obj.exists() and struct.unpack_from('<H',obj.read_bytes())[0]==0x1f2
 satisfied=(r.returncode!=0 and b'C2118' in log.read_bytes() and not obj.exists())if negative_expected else ok
 headers={}
 for line in log.read_text(errors='replace').splitlines():
  if 'including file:' in line:
   p=Path(line.split('including file:',1)[1].strip())
   if p.is_file():headers[str(p.resolve())]=sha(p)
 cpp=WORK/source if source in native else Path(source)
 rows.append({'source':source,'name':name,'passed':ok,'returncode':r.returncode,'expected_negative':negative_expected,'gate_satisfied':satisfied,'command':cmd,'object':str(obj)if obj.exists()else None,'assembly':str(asm)if asm.exists()else None,'log':str(log),'source_sha256':sha(cpp),'actual_included_headers':headers,'artifacts':{str(p):sha(p)for p in [obj,asm,log]if p.exists()}})
 print(name,r.returncode,'satisfied',satisfied,flush=True)
check()
expected_gui=[0x120,8,0x44,0,8,0x90,0xd4,0x118,0x11c,0x11d,0x30,0x30,0x30,0x38,0,0x20,0x28,0x30,0xb0,16]
facts={}
for n,sym,count in [('CombinedLayouts','R4LayoutFacts',25),('GuiAndTypedABI','R6GuiFacts',20),('BootLegalLayout','boot_layout',16)]:
 if (OUT/(n+'.obj')).exists():facts[n]=symbol_words(OUT/(n+'.obj'),sym,count)
ref=Path('D:/Reverse/BP-X360-Experimental-Artifacts/bootlegal-final-caller/native_bootlegal_probe-8a5c8ce8.obj')
result={'head':META['head'],'rows':rows,'emitted_facts':facts,'expected_gui':expected_gui,'BootLegal_reference':str(ref),'BootLegal_reference_sha256':sha(ref),'frequency_source_island':{'owner_recipe':str(freq_path),'owner_recipe_sha256':sha(freq_path),'actual_Main_source':str(actual_main),'actual_Main_source_sha256':sha(actual_main),'exact_branch_text_matched_ignoring_indentation_only':True,'branch_sha256':hashlib.sha256(branch(frequency).encode()).hexdigest(),'scope':'Diagnostic source island only; actual whole GameModule remains a visible selected failure, never bypassed.'},'source_inputs_rechecked':6298,'scope':'Two real current native providers plus fresh typed/layout/source-island probes. Production guards unchanged, independent WorldEntity79E0/79F0 remains negative. No full class, frame, physical hardware or link/launch claim.','created_utc':datetime.now(timezone.utc).isoformat()}
with (OUT/'gate-results.json').open('x')as f:json.dump(result,f,indent=2)
with (OUT/'compile-results.json').open('x')as f:json.dump({'state':'complete','head':META['head'],'requested':2,'passed':sum(r['passed']for r in rows[:2]),'failed':sum(not r['passed']for r in rows[:2]),'results':rows[:2],'scope':'Additional actual current providers required by named ABI/owner checks; not part of selected277 count.'},f,indent=2)
assert all(r['gate_satisfied']for r in rows)
assert facts['GuiAndTypedABI']==expected_gui,(facts['GuiAndTypedABI'],expected_gui)
assert facts['CombinedLayouts']==json.loads((HERE.parent/'repair-r5/named-header-probes/probe-results.json').read_text())['emitted_layout_facts']['CombinedLayouts']
assert facts['BootLegalLayout']==symbol_words(ref,'boot_layout',16)
print('SATISFIED',sha(OUT/'gate-results.json'),flush=True)
