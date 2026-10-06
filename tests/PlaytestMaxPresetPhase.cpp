// Compile unchanged diagnostic scheduling and caller regions. The captured engine
// boundary changes the frozen flag between debug and deformation solely to make
// callback ordering observable; this fixture does not emulate vehicle physics.
#include "types.hpp"
#include <cstdlib>
#include <cstdio>
#include <vector>
static s32 gPhase=0,gFrame=0,gCallbacks=0,gCallbackPhase=-1,gCallbackFrame=-1;
static bool gCallbackFrozen=false;
static std::vector<s32> gSamplePhases;
static s32 gReadyCount=0,gReadyFrame=-1,gReadyPhase=-1;
namespace renderengine { u32 guPresentCount=0; }
namespace CgsDev { namespace Log { void WriteToLog(const char*){++gReadyCount;gReadyFrame=gFrame;gReadyPhase=gPhase;} } }
struct VecFloat { f32 x,y,z,w; };
namespace BrnDirector { namespace Harness { bool gbArbitratorInRoaming=true; } }
namespace BrnPhysics { namespace Deformation {
struct Vehicle { bool frozen=false; f32 velocity[3]={0,0,0}; bool IsFrozen() const{return frozen;} };
struct DeformableObject
{
    Vehicle vehicle;int spec=1;
    int* GetDeformationSpec(){return &spec;}
    Vehicle* GetVehiclePhysics(){return &vehicle;}
};
struct DeformationDebugComponent;
struct DeformationManager
{
    DeformableObject* player=nullptr;bool active=true;
    static DeformationDebugComponent mDebugComponent;
    s32 miPlayerModelIndex=3;
    struct Bits { bool* active;bool IsBitSet(u32 index){return *active&&index==3;} } mModelsAdded{&active};
    s32 GetPlayerModelIndex(){return active?miPlayerModelIndex:-1;}
    bool IsDeformableObjectActive(s32 index){return active&&index==3;}
    DeformableObject* GetDeformableObject(s32){return player;}
    DeformableObject* GetPlayerCarModel(){if(!active)std::abort();return player;}
    static void RunMaxPresetProbePCDebugUpdate(f32);
    void Before(VecFloat lvfTimeStep);
    void Post(VecFloat lvfTimeStep);
};
struct DeformationDebugComponent
{
    DeformationManager* mpDeformationManager=nullptr;DeformableObject* mpSelectedRig=nullptr;
    s32 miSelectedRig=-1,miCompressPreset=-1;
    void RunMaxPresetProbePC(f32,bool);
    static void OnSelectedRigChange(void* value,void* owner)
    {
        auto* self=static_cast<DeformationDebugComponent*>(owner);
        if(value!=&self->miSelectedRig)std::abort();
        self->mpSelectedRig=self->mpDeformationManager->GetPlayerCarModel();
    }
    static void OnCompressionPresetChange(void* value,void* owner)
    {
        auto* self=static_cast<DeformationDebugComponent*>(owner);
        if(value!=&self->miCompressPreset||self->miCompressPreset!=2)std::abort();
        ++gCallbacks;gCallbackPhase=gPhase;gCallbackFrame=gFrame;
        gCallbackFrozen=self->mpSelectedRig->vehicle.frozen;
    }
};
DeformationDebugComponent DeformationManager::mDebugComponent;
#include "max_phase_prefix.inc"
#include "max_phase_bridge.inc"
void DeformationManager::Before(VecFloat lvfTimeStep){
#include "max_phase_before.inc"
}
void DeformationManager::Post(VecFloat lvfTimeStep){
#include "max_phase_post.inc"
}
} }
struct Timer { f32 GetRate() const{return .25f;} f32 GetScaleCurrent() const{return .5f;} };
struct DebugManager { void Update(f32 step){if(step!=.125f||gPhase!=1)std::abort();} };
struct Game
{
    Timer mGameTimer;DebugManager mDebugManager;
    void Debug(){
#include "max_phase_game.inc"
    }
};
int main(int argc,char** argv)
{
    using namespace BrnPhysics::Deformation;
    const bool enabled=argc>1&&std::atoi(argv[1])!=0;
    const bool frozen=argc>2&&std::atoi(argv[2])!=0;
    const bool roaming=argc>3&&std::atoi(argv[3])!=0;
    const bool delayed=argc>4&&std::atoi(argv[4])!=0;
    const s32 renders=argc>5?std::atoi(argv[5]):1;
    const bool waitAwake=argc>6&&std::atoi(argv[6])!=0;
    const s32 awakeAt=argc>7?std::atoi(argv[7]):-1;
    _putenv_s("BRN_PLAYTEST_MAX_DEFORM_AT",enabled?"6":"");
    _putenv_s("BRN_PLAYTEST_MAX_DEFORM_WAIT_AWAKE",waitAwake?"1":"");
    BrnDirector::Harness::gbArbitratorInRoaming=roaming;
    DeformableObject rig;DeformationManager manager;manager.player=&rig;
    DeformationManager::mDebugComponent.mpDeformationManager=&manager;
    Game game;
    for(gFrame=0;gFrame<80;++gFrame)
    {
        manager.active=!delayed||gFrame>=4;
        manager.miPlayerModelIndex=manager.active?3:-1;
        rig.vehicle.frozen=frozen && !(awakeAt>=0 && gFrame>=awakeAt);
        gPhase=1;for(s32 render=0;render<renders;++render)game.Debug();
        gPhase=2;rig.vehicle.frozen=true; // captured freezing/contact-generation boundary
        gPhase=3;manager.Before(VecFloat{.125f,.125f,.125f,.125f});
        gPhase=4;manager.Post(VecFloat{.125f,.125f,.125f,.125f});
    }
    s32 failures=0,checks=0;
    auto check=[&](bool pass){++checks;if(!pass)++failures;};
    const bool neverAwakes=waitAwake&&frozen&&awakeAt<0;
    const bool fires=enabled&&roaming&&!neverAwakes;
    check(gCallbacks==(fires?1:0));
    if(fires)
    {
        check(gCallbackPhase==1);check(gCallbackFrozen==(waitAwake?false:frozen));
        const s32 expectedFrame=(waitAwake&&frozen)?awakeAt:(delayed?52:48);
        check(gCallbackFrame==expectedFrame); //6s simulation delay, then next eligible debug tick
        check(gSamplePhases.size()==9); // one callback and eight unchanged post observations
        if(gSamplePhases.size()==9){check(gSamplePhases[0]==1);for(s32 i=1;i<9;++i)check(gSamplePhases[i]==4);}
        check(DeformationManager::mDebugComponent.miSelectedRig==3);
        check(DeformationManager::mDebugComponent.miCompressPreset==2);
    }
    else check(gSamplePhases.empty());
    if(waitAwake)
    {
        check(gReadyCount==((enabled&&roaming)?1:0));
        if(enabled&&roaming){check(gReadyFrame==(delayed?52:48));check(gReadyPhase==1);}
        check(rig.vehicle.velocity[0]==0&&rig.vehicle.velocity[1]==0&&rig.vehicle.velocity[2]==0);
    }
    std::printf("enabled=%d frozen=%d roaming=%d startup=%d renders=%d wait=%d awakeAt=%d checks=%d failures=%d phase=%d frame=%d ready=%d samples=%u\n",
        enabled,frozen,roaming,delayed,renders,waitAwake,awakeAt,checks,failures,gCallbackPhase,gCallbackFrame,gReadyCount,static_cast<u32>(gSamplePhases.size()));
    return failures?1:0;
}
