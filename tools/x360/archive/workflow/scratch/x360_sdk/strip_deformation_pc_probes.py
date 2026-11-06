import ast,re,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]/'b5-decomp'
assert subprocess.check_output(['git','branch','--show-current'],cwd=ROOT,text=True).strip()=='codex/experimental-x360-byte-match'
old=ast.parse((ROOT.parent/'scratch/x360_sdk/strip_vehicle_pc_diagnostics.py').read_text(encoding='utf-8'))
N={'LOG':[]}
exec(compile(ast.Module([x for x in old.body if isinstance(x,(ast.Import,ast.ImportFrom,ast.FunctionDef))],type_ignores=[]),'<balanced helpers>','exec'),N)
mask,pair,cut,ifs,block,anon=[N[x] for x in ('mask','pair','cut','ifs','block','anon')]
PENDING=[]
CALLS=['CGS_ASSERT','CGS_STATIC_ASSERT','AddWorldSpaceImpulse','AddWorldSpaceAngularImpulse','ApplyCrashedContactImpulse','ApplyWallContactImpulse','ApplyCarContactImpulse','ApplyShowtimeContactImpulse','ApplyCarCarImpulse','ApplyCarWorldImpulse','CalculateNewVelocity','SetRaceCarCrashing','InstantTakedown','AddAirRam','DetachWheel']
def counts(s):
 m=mask(s);return {x:len(re.findall(r'\b'+x+r'\s*\(',m)) for x in CALLS}
def pending(name,before,s):
 assert counts(before)==counts(s),(name,counts(before),counts(s))
 PENDING.append((ROOT/name,s));print(name,before.count('\n')-s.count('\n'),'lines removed')
def remove_pc_block(s,tag):return block(s,tag,'PC observation '+tag)
def literal(s,a,b=''):
 assert s.count(a)==1,(a[:80],s.count(a));return s.replace(a,b)

name='src/GameSource/Physics/DeformationManager/DeformationPhysics/BrnVehicleRigidBody.h'
before=(ROOT/name).read_text(encoding='utf-8');s=literal(before,'    extern CarCarDvArrivalTap gCarCarDvArrivalTap;\n')
pending(name,before,s)
name='src/GameSource/Physics/DeformationManager/DeformationPhysics/BrnVehicleRigidBody.cpp'
before=(ROOT/name).read_text(encoding='utf-8')
s='#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnVehicleRigidBody.h"\n\n// ARTIST ApplyLocalImpulse8260E068 and RecievePassedOnImpulse8260DFA0.\n// Both route the params\' impulse through the same state/contact-kind selection.\n// The passed-on magnitude argument is overwritten by the original params load.\nnamespace BrnPhysics {\nnamespace Deformation {\n    static __forceinline void ApplyImpulseToVehicle(\n        BrnPhysics::Vehicle::VehiclePhysics* lpVehicle,\n        const ImpulseParams* lpImpulseParams)\n    {\n        const Vector3& lrDirection = KA_IMPULSE_DIRECTIONS[lpImpulseParams->meImpulseDirection];\n        const f32 lfMagnitude = lpImpulseParams->mvfImpulseMagnitude.x;\n        const Vector3 liImpulse = {\n            lrDirection.x * lfMagnitude, lrDirection.y * lfMagnitude,\n            lrDirection.z * lfMagnitude, lrDirection.w * lfMagnitude\n        };\n        if (lpVehicle->IsCrashing())\n        {\n            // ARTIST8260E0B4/8260E018 loads the world-contact byte into r6.\n            lpVehicle->ApplyCrashedContactImpulse(liImpulse, rw::physics::BODY_SPACE,\n                lpImpulseParams->mImpulsePosition, lpImpulseParams->mePositionSpace,\n                lpImpulseParams->mbWorldContact);\n        }\n        else if (lpImpulseParams->mbWorldContact)\n        {\n            lpVehicle->ApplyWallContactImpulse(liImpulse, rw::physics::BODY_SPACE,\n                lpImpulseParams->mWorldImpulseDirection, lpImpulseParams->mImpulsePosition,\n                lpImpulseParams->mePositionSpace);\n        }\n        else\n        {\n            lpVehicle->ApplyCarContactImpulse(liImpulse, rw::physics::BODY_SPACE,\n                lpImpulseParams->mWorldImpulseDirection, lpImpulseParams->mImpulsePosition,\n                lpImpulseParams->mePositionSpace);\n        }\n    }\n\n    void VehicleRigidBody::ApplyLocalImpulse(ImpulseParams* lpImpulseParams)\n    {\n        ApplyImpulseToVehicle(mpAttachedVehicle, lpImpulseParams);\n    }\n\n    void VehicleRigidBody::RecievePassedOnImpulse(const ImpulseParams* lpImpulseParams,\n                                                 VecFloat /*lvfPassedMagnitude*/)\n    {\n        if (mpAttachedVehicle->IsPlayerVehicleInShowtime())\n            return;\n        ApplyImpulseToVehicle(mpAttachedVehicle, lpImpulseParams);\n    }\n}\n}\n'
pending(name,before,s)

name='src/GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject_Update.cpp'
before=(ROOT/name).read_text(encoding='utf-8');s=before
a=s.index('// [T5-sens] DIAG state, DEFINED');b=s.index('namespace BrnPhysics\n{\nnamespace Deformation',a);s=cut(s,a,b,'PC probe externs')
a=s.index('        // ---- [chain] PC bring-up instrument');b=s.index('        // ⭐⭐ TABLE RETIRED',a);s=cut(s,a,b,'PC chain state')
s=anon(s,'struct CarCarDvBody');s=anon(s,'struct IkCadenceTrack')
for tag in ['siStMagProbe','siTanProbe','siAbsorbProbe','siWheelProbe']:s=remove_pc_block(s,tag)
s,n=ifs(s,{'lbCarCarDv','lbWedgeCensus','lbProbe','lbProbe&&siWheelProbeLines<400','IkCadenceOn()','BrnPhysics::Vehicle::KerbProbeArmed()'});assert n==10,n
# Camera/body snapshots and output lambdas are diagnostic-only.
a=s.index('        // [carcar-dv] PC witness (see the banner above UpdateContacts):');m=mask(s);brace=m.index('{',m.index('auto lCarCarDvRead',a));end=pair(m,brace);assert s[end]==';';s=cut(s,a,end+1,'PC body snapshot lambda')
a=s.index('            // [carcar-dv] PC witness: capture both race cars');b=s.index('            bool lbApplied;',a);s=cut(s,a,b,'PC contact snapshots')
a=s.index('        const bool lbWedgeCensus');b=s.index('        // [carcar-dv] PC witness',a) if '        // [carcar-dv] PC witness' in s[a:] else -1
# The lambda prefix was removed above; remove the two-line flag and census locals directly.
m=mask(s);a=m.index('const bool lbWedgeCensus');b=m.index(';',a)+1;s=cut(s,a,b,'PC wedge flag')
s=re.sub(r'^.*s32 liWedgeWorld =[^\n]*\n','',s,flags=re.M)
s=re.sub(r'^.*(?:\+\+gChainProbe\.|BrnPhysics::Vehicle::gT5Apply|gpcDvDrainTag\s*=|\+\+liWedgeNoImpulse|if \( lbApplied \) \{ \+\+liWedge)[^\n]*\n','',s,flags=re.M)
s=re.sub(r'^        // \[deform-bbox\] NOT IN THE X360 BINARY[^\n]*\n','',s,flags=re.M)
a=s.index('    // [deform-bbox] NOT IN THE X360 BINARY');b=s.index('    // =============================================================================================',a);s=cut(s,a,b,'PC bbox counter')
s=re.sub(r'^.*\+\+guDeformLimitRowArmApplies[^\n]*\n','',s,flags=re.M)
s=re.sub(r'^.*static u32 suReportedShowtimeContacts[^\n]*\n','',s,flags=re.M)
s=re.sub(r'^.*static s32 siWheelProbe(?:Lines)?[^\n]*\n','',s,flags=re.M)
s=re.sub(r'^.*const bool lbProbe =[^\n]*\n','',s,flags=re.M)
s,n=ifs(s,{'siWheelProbe<0'});assert n==1,n
# Empty else from the removed null-sensor counter.
s=s.replace('            else\n            {\n            }\n','')
for line in ['namespace renderengine { extern u32 guPresentCount; }\n','#include <cstdlib>                                                              // getenv -- the opt-in [chain] bring-up probe only\n','#include <cstring>                                                            // std::memcpy -- the [carcar-dv] witness prints raw IEEE bits\n','#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDetachedPartManager.h"    // guDiagDeformationStep ([ik-cadence] / [absorb] step stamp, DIAG only)\n']:
 s=literal(s,line)
for tag in ['gChainProbe','CarCarDv','IkCadence','liWedge','lbWedge','guDeformLimit','guDiagDeformationStep','gT5Apply','gpcDvDrainTag','KerbProbe']:
 assert tag not in mask(s),(name,tag)
pending(name,before,s)

name='src/GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject_BBox.cpp'
before=(ROOT/name).read_text(encoding='utf-8');s=before
for tag in ['siSweptSelProbe','siBBoxTracePeriod']:s=remove_pc_block(s,tag)
s=literal(s,'namespace renderengine { extern u32 guPresentCount; }\n')
s=literal(s,'\textern u32 guDeformLimitRowArmApplies[2];\n')
assert 'guDeformLimitRowArmApplies' not in mask(s)
pending(name,before,s)
for p,s in PENDING:p.write_text(s,encoding='utf-8',newline='\r\n')
(ROOT.parent/'scratch/x360_sdk/deformation-pc-removal.json').write_text(json.dumps(N['LOG'],indent=2)+'\n',encoding='utf-8')
