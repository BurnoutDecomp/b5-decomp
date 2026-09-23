#include "types.hpp"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/World/AI/SharedIO/BrnRaceCarAIInterfaces.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostStrategy.h"
#include "SharedClasses/World/BrnCollisionTag.h"
#include "SharedClasses/Trigger/BrnRegion.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpMessage, const char*, int) { std::fprintf(stderr, "%s\n", lpMessage); std::abort(); }
void* EndAssert() { return nullptr; }
} }
namespace CgsDev { namespace Log { DebugPrint* gpDebugPrint=nullptr; } namespace Message { u64 gxMessageFilterFlags=0; } }
namespace Fixture {
using namespace BrnWorld;
using BrnWorld::CollisionTag;
struct BoostStrategy {
    OncomingState mePreviousOncomingState = E_ONCOMING_STATE_FALSE;
    bool mbCrashing=false, mbOncoming=false, mbBoosting=false, mbForceBoost=false;
    f32 mfSpeed=30, mfOncomingFade=0;
    static constexpr f32 KF_ONCOMING_MINSPEED=0;
    OncomingState GetPreviousOncomingState() const { return mePreviousOncomingState; }
    void SetOncomingState(OncomingState);
    void SetForceBoost(bool);
};
struct BoostManager {
    BoostStrategy mStrategy;
    BoostStrategy* GetBoostStrategy() { return &mStrategy; }
};
struct ActiveRaceCar {
    BrnPhysics::Vehicle::RaceCarState mState{};
    Vector3 mPosition{1,2,3,0}, mDirection{0,0,1,0}, mVelocity{0,0,30,0};
    Vector3 mPlacedPosition{}, mPlacedDirection{};
    bool mbActive=true;
    f32 mfPlacedSpeed=-1;
    s32 miRequests=0;
    u16 muSection=32767;
    bool IsActive() const { return mbActive; }
    const auto* GetPhysicsState() const { return &mState; }
    Vector3 GetPosition() const { return mPosition; }
    Vector3 GetDirection() const { return mDirection; }
    Vector3 GetVelocity() const { return mVelocity; }
    void SetAISection(u16 luSection) { muSection=luSection; }
    void RequestPlaceOnTrack(const Vector3& lrPosition,const Vector3& lrDirection,f32 lfSpeed) {
        ++miRequests; mPlacedPosition=lrPosition; mPlacedDirection=lrDirection; mfPlacedSpeed=lfSpeed;
    }
};
struct AIInterface { CgsModule::VariableEventQueue<16384,16> mManagementQueue; };
struct Output {
    AIInterface mAI;
    Output() { mAI.mManagementQueue.Construct(); }
    AIInterface* GetRaceCarAIInterface() { return &mAI; }
};
struct RaceCarEntityModule {
    ActiveRaceCar maCars[8];
    EActiveRaceCarIndex mePlayerActiveRaceCarIndex=E_ACTIVE_RACE_CAR_INDEX_0;
    BoostManager mBoostManager;
    bool mbOncomingTimerActive=false, mbIsInGameMode=true, mbIsInOnlineGameMode=false;
    f32 mfOncomingNoClueTimer=0, mfTimeStep=0.25f, mfLastPlayerCarSpeed=0;
    ActiveRaceCar* GetActiveRaceCar(EActiveRaceCarIndex leIndex) { return &maCars[leIndex]; }
    void UpdateRaceCarCollisionTagging(s32,const BrnPhysics::Vehicle::RaceCarState*);
    void HandleDriverAction(const CgsModule::Event*,Output*);
};
#include "player_ai_methods.inc"
}
int main() {
    using namespace Fixture;
    s32 liChecks=0;
    auto Check=[&](bool lbPass,const char* lpName) { ++liChecks; if(!lbPass) { std::printf("FAIL: %s\n",lpName); std::exit(1); } };
    Fixture::RaceCarEntityModule lModule;
    auto& lrBoost=lModule.mBoostManager.mStrategy;
    auto& lrState=lModule.maCars[0].mState;
    auto Tag=[&](u32 luCode,f32 lfHeading,s32 liCar=0) {
        lrState.mAboveGroundTestResult.mbValid=true;
        lrState.mAboveGroundTestResult.mCollisionTag.muValue=(42u<<16)|0x2000u|luCode;
        lrState.mTransform.zAxis={std::cos(lfHeading),0,std::sin(lfHeading),0};
        lModule.UpdateRaceCarCollisionTagging(liCar,&lrState);
    };
    Tag(2,3.14159265f);
    Check(lrBoost.mbOncoming && lModule.maCars[0].muSection==42,"opposite lane activates oncoming and preserves section tagging");
    Tag(1,0); Check(lrBoost.mbOncoming && !lModule.mbOncomingTimerActive,"unknown direction preserves oncoming without starting no-lane timer");
    for(s32 li=0;li<3;++li) Tag(0,0);
    Check(lrBoost.mePreviousOncomingState==E_ONCOMING_STATE_PREVIOUS && lModule.mfOncomingNoClueTimer==0.75f,"junction grace includes exact 0.75 seconds");
    Tag(0,0); Check(lrBoost.mePreviousOncomingState==E_ONCOMING_STATE_FALSE && !lModule.mbOncomingTimerActive,"lane-less road expires grace after 0.75 seconds");
    lrBoost.mfOncomingFade=0; Tag(2,0); Check(!lrBoost.mbOncoming,"normal direction does not activate oncoming");
    Tag(2,1.5707963f); Check(!lrBoost.mbOncoming,"crossing lanes at right angles is not oncoming");
    Tag(15,-3.14159265f/7); Check(!lrBoost.mbOncoming,"negative heading wraps correctly on a valid lane");
    Tag(15,2.6927937f); Check(lrBoost.mbOncoming,"oncoming detection works across angular wrap");
    lrBoost.mfOncomingFade=0; lrBoost.mfSpeed=0; Tag(2,3.14159265f); Check(!lrBoost.mbOncoming,"stationary car does not earn oncoming");
    lrBoost.mfSpeed=30; lrBoost.mbCrashing=true; Tag(2,3.14159265f); Check(!lrBoost.mbOncoming,"crashing suppresses oncoming");
    lrBoost.mbCrashing=false; Tag(2,0); Tag(2,3.14159265f,1);
    Check(!lrBoost.mbOncoming && lModule.maCars[1].muSection==42,"rival tags never change player boost state");
    lrState.mAboveGroundTestResult.mbValid=false; lrState.mAboveGroundTestResult.mCollisionTag.muValue=0;
    lModule.UpdateRaceCarCollisionTagging(0,&lrState);
    Check(lModule.maCars[0].muSection==42,"missing ground leaves the prior section intact");
    Output lOutput;
    BrnGameState::GameStateModuleIO::SetPlayerCarDriverAction lAction{};
    auto Dispatch=[&]() {
        lOutput.mAI.mManagementQueue.Clear();
        lModule.HandleDriverAction(reinterpret_cast<const CgsModule::Event*>(&lAction),&lOutput);
        const CgsModule::Event* lpEvent=nullptr; s32 liSize=0;
        s32 liType=lOutput.mAI.mManagementQueue.GetFirstEvent(&lpEvent,&liSize);
        Check(liType==BrnAI::AIModuleIO::E_EVENT_PLAYER_TAKEN_OVER && liSize==sizeof(BrnAI::AIModuleIO::PlayerControlChangedEvent),"handoff posts the typed AI control event");
        return static_cast<const BrnAI::AIModuleIO::PlayerControlChangedEvent*>(lpEvent)->mbPlayerIsInControl;
    };
    lAction.meCarControl=E_CAR_CONTROL_AI_MODULE; lrState.mfSpeedMPH=100; lrBoost.mbBoosting=true;
    Check(!Dispatch() && lrBoost.mbForceBoost && std::fabs(lModule.mfLastPlayerCarSpeed-44.704f)<0.001f,"camera takeover retains boost and records speed in m/s");
    Check(lModule.maCars[0].miRequests==0,"takedown handoff does not reset car position or speed");
    lAction.meCarControl=E_CAR_CONTROL_ENTITY_MODULE;
    Check(Dispatch() && !lrBoost.mbForceBoost,"camera return restores player and releases forced boost");
    lAction.mbIsDriveThru=true; lAction.mfMaxResetSpeed=50;
    Dispatch(); Check(std::fabs(lModule.maCars[0].mfPlacedSpeed-44.704f)<0.001f,"drive-through return restores entry speed");
    // 0x8230CC04..0x8230CC1C: min(last, cap) then fsel-max with flt_82FAD3F4 (0.44704f*60, CRT 0x82C4BC70):
    // a 20 m/s cap is floored at the 60 mph entry speed (G68-D3; this check used to expect the pre-fix 20).
    lAction.mfMaxResetSpeed=20; Dispatch(); Check(lModule.maCars[0].mfPlacedSpeed==0.44704f*60.0f,"drive-through exit: authored cap below 60 mph is floored at KF_DRIVE_THRU_ENTRY_SPEED");
    lAction.meCarControl=E_CAR_CONTROL_AI_MODULE;
    auto& lrCar=lModule.maCars[0];
    BrnTrigger::BoxRegion lBox;
    const f32 lafBoxData[9]={10,0,20,0,0,0,8,4,12};
    std::memcpy(&lBox,lafBoxData,sizeof(lBox));
    std::memcpy(lAction.maDriveThruBoxRegion,&lBox,sizeof(lBox));
    lrCar.mVelocity={0,0,30,0}; lrBoost.mbBoosting=false;
    // 0x8230CC70/0x8230CC7C `lfs f0, flt_82FAD3F4 ; fabs f1` -- the entrance seats the car at 60 mph, not
    // at the .bss zero the pre-G68-D3 code read.
    Dispatch(); Check(lrCar.mfPlacedSpeed==0.44704f*60.0f && lrCar.mPlacedPosition.z==14 && lrCar.mPlacedDirection.z==1,
                      "drive-through entrance aligns the car with the approach side of the box");
    Check(!lrBoost.mbForceBoost,"handoff cannot create boost when none was active");
    lrCar.mVelocity={0,0,-30,0}; Dispatch();
    Check(lrCar.mPlacedPosition.z==26 && lrCar.mPlacedDirection.z==-1,"reverse-side drive-through approach preserves travel direction");
    lModule.maCars[0].mbActive=false; lAction.meCarControl=E_CAR_CONTROL_AI_MODULE;
    Check(!Dispatch(),"inactive car still informs AI about control handoff");
    std::printf("PASS: %d player handoff and oncoming checks\n",liChecks);
}
