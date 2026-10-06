// Production callback extracted unchanged; raw ARTIST gold tests every sensor
// and invalid/absent-rig early-outs. Out-of-range reads hit a fixture sentinel.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include <cstdio>
#include <vector>
namespace BrnPhysics { namespace Deformation {
struct DeformationSensor
{
    s32 id=-1;Vector4 local;f32 scratch;
    const Vector4& GetLocalSphereCentre() const{return local;}
    f32 GetScratchAmount() const{return scratch;}
};
struct Spec { s32 count; s32 GetNumDeformationSensors() const{return count;} };
struct DeformableObject
{
    Spec spec;DeformationSensor sensors[20],sentinel;
    s32 requested=-1;
    const Spec* GetDeformationSpec() const{return &spec;}
    DeformationSensor& GetSensorDebug(s32 i){requested=i;return i>=0&&i<20?sensors[i]:sentinel;}
};
struct DeformationDebugComponent
{
    DeformableObject* mpSelectedRig;DeformationSensor* mpSelectedSensor=nullptr;
    s32 miSelectedSensor;
    f32 mfSensorX=-999,mfSensorY=-999,mfSensorZ=-999,mfSensorScratch=-999;
    std::vector<s32> calls;
    static void OnSelectedSensorChange(void*,void*);
    void SetReadOnly(void* p,bool value)
    {
        s32 field=p==&miSelectedSensor?0:p==&mfSensorX?1:p==&mfSensorY?2:p==&mfSensorZ?3:4;
        calls.push_back(field);calls.push_back(value);
    }
};
#include "deformation_sensor_methods.inc"
} }
struct Case{s32 count,index;bool rig;};
#include "deformation_sensor_cases.inc"
int main()
{
    using namespace BrnPhysics::Deformation;
    for(u32 k=0;k<sizeof(K_CASES)/sizeof(K_CASES[0]);++k)
    {
        const Case& input=K_CASES[k];DeformableObject rig;rig.spec.count=input.count;
        rig.sentinel={999,{9000,9000,9000,2},9000};
        for(s32 i=0;i<20;++i)rig.sensors[i]={i,{100.0f+i,10.0f+i,-static_cast<f32>(i),2},.01f*(i+1)};
        DeformationDebugComponent debug;debug.mpSelectedRig=input.rig?&rig:nullptr;debug.miSelectedSensor=input.index;
        DeformationDebugComponent::OnSelectedSensorChange(nullptr,&debug);
        std::printf("CASE,%u,%d,%d,%.9g,%.9g,%.9g,%.9g,%u",k,
                    debug.mpSelectedSensor?debug.mpSelectedSensor->id:-1,rig.requested,
                    debug.mfSensorX,debug.mfSensorY,debug.mfSensorZ,debug.mfSensorScratch,
                    static_cast<u32>(debug.calls.size()));
        for(s32 call:debug.calls)std::printf(",%d",call);
        std::puts("");
    }
}
