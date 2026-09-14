#pragma once

// BrnWorld::BoostManager
//
// Declaration shape is from the DecFIGS DWARF BrnBoostManager.h. The three
// strategies are embedded by value; Breaker SetBoostStrategy @0x822A3288 and
// Prepare @0x822B8D08 independently pin their console offsets as +0x10,
// +0x160 and +0x2A0 and the selected-strategy pointer as +0x450.

#include <cstddef>

#include "types.hpp"
#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout2.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout5.h"

namespace BrnWorld
{

class BoostManager
{
public:
    enum BoostStrategyId
    {
        E_BOOSTSTRATEGY_BURNOUT1 = 1,
        E_BOOSTSTRATEGY_BURNOUT2 = 2,
        E_BOOSTSTRATEGY_BURNOUT3 = 3,
        E_BOOSTSTRATEGY_BURNOUT4 = 4,
        E_BOOSTSTRATEGY_BURNOUT5 = 5
    };

    bool Prepare();                                      // 0x822B8D08
    void SetBoostStrategy(BoostStrategyId leStrategy);   // 0x822A3288
    void OnModeStart(BrnGameState::GameStateModuleIO::EGameModeType leGameModeType);
    void ApplyCarStats(s32 liCurrentCarBoostLevel, s32 liCurrentPlayerBoostLevel);
    void ApplyPreviousCarStats();

    BoostStrategyId GetBoostStrategyId() const { return meBoostStrategy; }
    BoostStrategy* GetBoostStrategy() const { return mpBoostStrategy; }

    void UpdateChainExploits(Vector3 lCurrentPosition)
    {
        mpBoostStrategy->UpdateChainExploits(lCurrentPosition);
    }

    void Update(RaceCarEntityModuleIO::GameEventQueue* lpEventQueue,
                f32 lfTimeStep, f32 lfBoostModifier)
    {
        mpBoostStrategy->Update(lpEventQueue, lfTimeStep, lfBoostModifier);
    }

    void SetBoostEarningEnabled(bool lbEnabled);            // 0x822A33B0
    void SetWrecking(bool lbWrecking, bool lbIsInOnlineGameMode);  // 0x822B8E40

    // The near-miss notification (the X360 inlines the manager hop into a dispatch through
    // the selected strategy's vtable slot 6 -- see NearMissManager::NearMissEvent).
    void OnNearMiss(ENearMissType leNearMissType) { mpBoostStrategy->OnNearMiss(leNearMissType); }

    // ⭐ [boost-ticker wave 2026-09-14] The two notifications RaceCarEntityModule::HandleGameActions
    // @0x8230BE08 raises. Like every other forwarder on this class the console inlines the manager
    // hop and dispatches straight through mpBoostStrategy's vtable, so the slot is the proof:
    //   case 107 @0x8230D198  `(*(**(this+97504) + 48))(strategy)`   slot 12 (+0x30) OnTrafficCheck
    //   case 61  @0x8230CD74  `(*(**(this+97504) + 40))(strategy,x)` slot 10 (+0x28) OnStuntCompletion
    // (module+97504 is mBoostManager+0x450, i.e. mpBoostStrategy.)
    void OnTrafficCheck() { mpBoostStrategy->OnTrafficCheck(); }
    void OnStuntCompletion(BrnGameState::StuntElementType leElementType)
    {
        mpBoostStrategy->OnStuntCompletion(leElementType);
    }

    // [boost-wave2 2026-09-14] The three remaining notifications the module raises. Like every
    // other forwarder on this class the console inlines the manager hop and dispatches straight
    // through mpBoostStrategy's vtable, so the slot IS the proof (module+97504 == mBoostManager
    // +0x450 == mpBoostStrategy):
    //   HandleGameActions case 53 @0x8230C5F8   `(*(**(this+97504) + 20))(strategy, type)`
    //                                           slot 5  (+0x14) OnPlayerAttacksRival
    //   UpdateBoost           @0x82304CD8       `(*(*strategy + 36))(strategy)`
    //                                           slot 9  (+0x24) OnSlammed
    //   ProcessPropContactQueue @0x822D278C     `(*(**(this+97504) + 64))(strategy)`
    //                                           slot 16 (+0x40) OnPropHit
    void OnPlayerAttacksRival(BrnPhysics::Vehicle::EImpactType leImpactType)
    {
        mpBoostStrategy->OnPlayerAttacksRival(leImpactType);
    }
    void OnSlammed()  { mpBoostStrategy->OnSlammed(); }
    void OnPropHit()  { mpBoostStrategy->OnPropHit(); }

    void SetCrashing(bool lbCrashing) { mpBoostStrategy->SetCrashing(lbCrashing); }
    void SetForceBoost(bool lbForceBoost) { mpBoostStrategy->SetForceBoost(lbForceBoost); }
    void SetInfiniteBoost(bool lbInfiniteBoost) { mpBoostStrategy->SetInfiniteBoost(lbInfiniteBoost); }
    void SetSpeed(f32 lfSpeed) { mpBoostStrategy->SetSpeed(lfSpeed); }
    void SetBoostRequested(bool lbRequested) { mpBoostStrategy->SetBoostRequested(lbRequested); }
    void SetTailgating(bool lbTailgating, EActiveRaceCarIndex leCar)
    {
        mpBoostStrategy->SetTailgating(lbTailgating, leCar);
    }
    // DecFIGS manager signature is the raw air time.  KF_MIN_AIR_TIME_FOR_BOOST
    // is 0.5f (big-endian 0x3F000000); the manager reduces it to the strategy's
    // boolean state exactly as the Breaker inline witness does.
    void SetInAir(f32 lfAirTime) { mpBoostStrategy->SetInAir(lfAirTime > 0.5f); }
    void SetDrifting(bool lbDrifting) { mpBoostStrategy->SetDrifting(lbDrifting); }
    void SetSpinAngle(f32 lfAngle) { mpBoostStrategy->SetSpinAngle(lfAngle); }
    void SetOncomingState(OncomingState leState) { mpBoostStrategy->SetOncomingState(leState); }
    // DWARF BrnBoostManager.h:237. Its ONE console call site is the showtime arm of
    // RaceCarEntityModule::UpdateBoost @0x823046FC..0x8230471C, which the console emits as a
    // direct `bctrl` through mpBoostStrategy's slot 39 (vtable byte 0x9C) -- the manager hop
    // is inlined away exactly as it is for every other forwarder on this class.
    void SetBoostAmount(f32 lfBoostFraction) { mpBoostStrategy->SetBoostAmount(lfBoostFraction); }
    void TurnOffBoosting() { mpBoostStrategy->TurnOffBoosting(); }
    void UpdateStuntBoost(
        const BrnGameState::GameStateModuleIO::CompletedStuntAction* lpCompletedStuntAction)
    {
        mpBoostStrategy->UpdateStuntBoost(lpCompletedStuntAction);
    }

    bool IsBoosting() const { return mpBoostStrategy->IsBoosting(); }
    f32 GetBoostAmount() const { return mpBoostStrategy->GetBoostAmount(); }
    f32 GetMaxBoost() const { return mpBoostStrategy->GetMaxBoost(); }

    f32 GetJustBounceBoostedTimer() const { return mfJustBounceBoostedTimer; }
    void UpdateJustBounceBoostedTimer(f32 lfTimeStep)
    {
        if (mfJustBounceBoostedTimer > 0.0f)
            mfJustBounceBoostedTimer -= lfTimeStep;
    }

private:
    bool            mbBoostEarningEnabled;       // X360 +0x000
    BoostStrategyId meBoostStrategy;             // X360 +0x004
    BoostBurnout2   mBoostBurnout2;              // X360 +0x010
    BoostBurnout3   mBoostBurnout3;              // X360 +0x160
    BoostBurnout5   mBoostBurnout5;              // X360 +0x2A0
    BoostStrategy*  mpBoostStrategy;             // X360 +0x450
    s32             miCurrentCarBoostLevel;      // X360 +0x454
    s32             miCurrentPlayerBoostLevel;   // X360 +0x458
    f32             mfJustBounceBoostedTimer;    // X360 +0x45C

    static void _AssertConsoleInvariantPrefix()
    {
        static_assert(offsetof(BoostManager, meBoostStrategy) == 0x4,
                      "Breaker SetBoostStrategy stores the id at +0x4");
        static_assert(offsetof(BoostManager, mBoostBurnout2) == 0x10,
                      "Breaker manager B2 embed is +0x10");
        static_assert(offsetof(BoostManager, mBoostBurnout3) == 0x160,
                      "Breaker manager B3 embed is +0x160");
        static_assert(offsetof(BoostManager, mBoostBurnout5) == 0x2A0,
                      "Breaker manager B5 embed is +0x2A0");
        static_assert(offsetof(BoostManager, mpBoostStrategy) == 0x450,
                      "Breaker selected strategy pointer is +0x450");
    }
};

} // namespace BrnWorld
