// Production test-hook scheduling/callback prefix, extracted unchanged until
// diagnostics begin. A parked frozen rig is valid for the original debug menu.
#include "types.hpp"
#include <cstdlib>
#include <cstdio>
#include <vector>
namespace renderengine { u32 guPresentCount=0; }
namespace CgsDev { namespace Log { void WriteToLog(const char*){} } }
namespace BrnDirector { namespace Harness { bool gbArbitratorInRoaming=true; } }
namespace BrnPhysics { namespace Deformation {
struct Vehicle { bool frozen; bool IsFrozen() const{return frozen;} };
struct DeformableObject
{
    Vehicle vehicle;int spec=1;
    int* GetDeformationSpec(){return &spec;}
    Vehicle* GetVehiclePhysics(){return &vehicle;}
};
struct Manager
{
    DeformableObject* player;
    bool active=true;
    s32 GetPlayerModelIndex(){return active?3:-1;}
    DeformableObject* GetPlayerCarModel(){return player;}
    bool IsDeformableObjectActive(s32 index){return active&&index==3;}
    DeformableObject* GetDeformableObject(s32){return player;}
};
static std::vector<s32> gCalls;
static s32 gWitness=0;
struct DeformationDebugComponent
{
    Manager* mpDeformationManager;DeformableObject* mpSelectedRig=nullptr;
    s32 miSelectedRig=-1,miCompressPreset=-1;
    void RunMaxPresetProbePC(f32,bool);
    static void OnSelectedRigChange(void* value,void* owner)
    {
        auto* self=static_cast<DeformationDebugComponent*>(owner);
        if(value!=&self->miSelectedRig)std::abort();
        self->mpSelectedRig=self->mpDeformationManager->GetPlayerCarModel();gCalls.push_back(1);
    }
    static void OnCompressionPresetChange(void* value,void* owner)
    {
        auto* self=static_cast<DeformationDebugComponent*>(owner);
        if(value!=&self->miCompressPreset||self->miCompressPreset!=2)std::abort();
        gCalls.push_back(2);
    }
};
#include "max_preset_probe_prefix.inc"
} }
int main(int argc,char** argv)
{
    using namespace BrnPhysics::Deformation;
    const bool frozen=argc>1&&std::atoi(argv[1])!=0;
    const bool roaming=argc<=2||std::atoi(argv[2])!=0;
    BrnDirector::Harness::gbArbitratorInRoaming=roaming;
    _putenv_s("BRN_PLAYTEST_MAX_DEFORM_AT","6");
    DeformableObject rig;rig.vehicle.frozen=frozen;Manager manager{&rig};DeformationDebugComponent debug{&manager};
    for(s32 frame=0;frame<20;++frame){debug.RunMaxPresetProbePC(1,false);debug.RunMaxPresetProbePC(1,true);}
    bool pass=roaming?(gCalls==std::vector<s32>{1,2}&&debug.miSelectedRig==3&&debug.miCompressPreset==2&&gWitness>0):gCalls.empty();
    std::printf("frozen=%d roaming=%d callbacks=%u preset=%d witness=%d %s\n",frozen,roaming,
                static_cast<u32>(gCalls.size()),debug.miCompressPreset,gWitness,pass?"PASS":"FAIL");
    return pass?0:1;
}
