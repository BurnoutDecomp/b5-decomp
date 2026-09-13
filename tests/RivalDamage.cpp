// Production takedown consumers run against controlled vehicle/boost state.
#include "types.hpp"
#include "GameSource/BurnoutConstants.h"
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarType.h"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include <cstdio>
#include <cstdlib>
#include <set>

namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int)
{ std::fprintf(stderr, "Assertion: %s\n", lpcMessage); std::abort(); }
void* EndAssert() { return nullptr; }
} }

namespace Fixture {
using namespace BrnWorld;
namespace RaceCarEntityModuleIO {
using TakedownEventQueue = CgsModule::EventQueue<BrnGameState::TakedownEvent, 8>;
}
struct RaceCar {
    ERaceCarType meType = E_RACE_CAR_TYPE_AI;
    ERaceCarType GetType() const { return meType; }
    bool IsAIDriven() const { return meType == E_RACE_CAR_TYPE_AI; }
};
struct RenderParams {
    bool mbDamaged = false;
    bool IsDamaged() const { return mbDamaged; }
    void SetDamaged(bool lbDamaged) { mbDamaged = lbDamaged; }
};
struct ActiveRaceCar {
    RaceCar mCar;
    RenderParams mRender;
    bool mbActive = true, mbIsWrecked = false, mbTakenDown = false;
    bool IsActive() const { return mbActive; }
    RaceCar* GetGlobalRaceCar() { return &mCar; }
    RenderParams* GetRenderParams() { return &mRender; }
    const RenderParams* GetRenderParams() const { return &mRender; }
    void SetTakenDown(bool lbTakenDown) { mbTakenDown = lbTakenDown; }
};
struct BoostStrategy {
    int miRewards = 0, miTakenDown = 0;
    void OnTakedown() { ++miRewards; }
    void OnTakenDownByAIOrPlayer() { ++miTakenDown; }
};
struct BoostManager {
    BoostStrategy mStrategy;
    BoostStrategy* GetBoostStrategy() { return &mStrategy; }
};
struct NearMissManager {
    std::set<u32> mTakenDown;
    void AddTakenDownRaceCar(u32 luCar) { mTakenDown.insert(luCar); }
};
struct Input {
    RaceCarEntityModuleIO::TakedownEventQueue mQueue;
    Input() { mQueue.Construct(); }
    const RaceCarEntityModuleIO::TakedownEventQueue* GetTakedownEventQueue() const { return &mQueue; }
};
struct RaceCarEntityModule {
    ActiveRaceCar maActiveRaceCars[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    EActiveRaceCarIndex mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
    BoostManager mBoostManager;
    NearMissManager mNearMissManager;
    ActiveRaceCar* GetActiveRaceCar(EActiveRaceCarIndex leCar) { return &maActiveRaceCars[leCar]; }
    s32 GetDamagedCarCount() const;
    void ProcessTakedownEvents(const RaceCarEntityModuleIO::TakedownEventQueue* lpQueue);
    void UpdateBoostTakedowns(const Input* lpInput);
};
#include "rival_damage_methods.inc"
}

int main()
{
    using namespace Fixture;
    int liChecks = 0;
    auto Check = [&](bool lbPass, const char* lpcName) {
        ++liChecks;
        if (!lbPass) { std::fprintf(stderr,"FAIL: %s\n",lpcName); std::exit(1); }
    };
    auto SetEvent = [](Input& lrInput, s32 liAggressor, s32 liVictim) {
        lrInput.mQueue.Clear();
        BrnGameState::TakedownEvent lEvent{};
        lEvent.meAggressorIndex = static_cast<EActiveRaceCarIndex>(liAggressor);
        lEvent.meVictimIndex = static_cast<EActiveRaceCarIndex>(liVictim);
        lrInput.mQueue.AddEvent(lEvent);
    };
    RaceCarEntityModule lModule;
    lModule.maActiveRaceCars[0].mCar.meType = BrnWorld::E_RACE_CAR_TYPE_PLAYER;
    lModule.maActiveRaceCars[0].mRender.mbDamaged = true;
    Input lInput;
    SetEvent(lInput,0,1);
    lModule.UpdateBoostTakedowns(&lInput);
    Check(lModule.maActiveRaceCars[1].mRender.mbDamaged, "player takedown enables rival deformation rendering");
    Check(lModule.mBoostManager.mStrategy.miRewards == 1, "damage activation preserves boost reward");
    lModule.ProcessTakedownEvents(&lInput.mQueue);
    Check(lModule.maActiveRaceCars[1].mbTakenDown, "victim enters takedown lifecycle");
    Check(lModule.mNearMissManager.mTakenDown.count(1) == 1, "taken-down rival is excluded from near-miss rewards");
    SetEvent(lInput,2,3); lModule.UpdateBoostTakedowns(&lInput);
    Check(!lModule.maActiveRaceCars[3].mRender.mbDamaged && lModule.mBoostManager.mStrategy.miRewards == 1,
          "AI-only takedown does not grant player damage rendering or boost");
    lModule.ProcessTakedownEvents(&lInput.mQueue);
    Check(lModule.maActiveRaceCars[3].mbTakenDown, "AI victim is tracked even without player credit");
    SetEvent(lInput,1,0); lModule.ProcessTakedownEvents(&lInput.mQueue);
    Check(lModule.maActiveRaceCars[0].mbIsWrecked && lModule.maActiveRaceCars[0].mbTakenDown,
          "player victim is wrecked and marked taken down");
    Check(lModule.mBoostManager.mStrategy.miTakenDown == 1, "player victim receives boost penalty");
    lModule.maActiveRaceCars[2].mCar.meType = BrnWorld::E_RACE_CAR_TYPE_NETWORK;
    SetEvent(lInput,0,2); lModule.UpdateBoostTakedowns(&lInput);
    Check(!lModule.maActiveRaceCars[2].mRender.mbDamaged, "network victim keeps its existing rendering state");
    for (int liCar = 0; liCar < 5; ++liCar) lModule.maActiveRaceCars[liCar].mRender.mbDamaged = true;
    SetEvent(lInput,0,5); lModule.UpdateBoostTakedowns(&lInput);
    Check(!lModule.maActiveRaceCars[5].mRender.mbDamaged, "five damaged active cars exhaust the console budget");
    lModule.maActiveRaceCars[4].mbActive = false;
    Check(lModule.GetDamagedCarCount() == 4, "inactive damaged cars do not consume the budget");
    lModule.UpdateBoostTakedowns(&lInput);
    Check(lModule.maActiveRaceCars[5].mRender.mbDamaged, "free budget admits a newly taken-down rival");
    lInput.mQueue.Clear(); lModule.mBoostManager.mStrategy.miRewards = 0;
    lModule.UpdateBoostTakedowns(&lInput); lModule.ProcessTakedownEvents(&lInput.mQueue);
    Check(lModule.mBoostManager.mStrategy.miRewards == 0, "empty event queues have no reward side effects");
    std::printf("PASS: %d rival damage and takedown lifecycle checks\n",liChecks);
}
