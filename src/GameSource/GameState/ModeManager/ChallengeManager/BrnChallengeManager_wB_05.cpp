// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_05.cpp
// ============================================================================
// Wave-B partfile (group 5) for BrnGameState::ChallengeManager. Player-car action gates,
// store-for-store from the X360 ARTIST asm over the keystone's named members + committed
// interface accessors:
//   CheckForModifiers  @ 0x823166C0
//   UpdateBurnouts     @ 0x823345B0
//
// BLOCKED (reported to the conductor): CheckCurrentCar @ 0x823336E8 -- its car-class gate
// reads a boost/restriction-class byte at BrnResource::VehicleListEntry + 0xE8
// (`lbz r11,0xE8(r31)`), which the committed VehicleListEntry.h models only as unnamed pad
// (maPad224[16], +0xE0..0xEF). No named accessor exists for that byte and this partfile may
// not edit the header, so the function cannot be bodied faithfully here (see funcs_blocked).
// ============================================================================

#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h"
#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManagerDebugComponent.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "SharedClasses/DataLists/ChallengeListEntry.h"                  // BrnResource::ChallengeListEntryAction (GetModifier, KX_MODIFIER_IN_AIR)
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntityActiveRaceCarOutputInterface, BoostOutputInfo

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// CheckForModifiers -- X360 0x823166C0. Gate an action's success on its modifier flags: with
// no modifier byte the action is unconditionally allowed; otherwise (currently only the IN_AIR
// modifier is decoded) the player must be off the gas -- the X360 returns whether the player
// race-car state's mfGas is non-zero. `this` is unused (the check is over the passed action +
// interface only), exactly as the X360 body.
// ----------------------------------------------------------------------------
bool ChallengeManager::CheckForModifiers(
    const BrnResource::ChallengeListEntryAction*                                 lpAction,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface)
{
    CGS_ASSERT(lpAction, "lpAction");                                        // X360 line 2818
    CGS_ASSERT(lpActiveRaceCarOutputInterface, "lpActiveRaceCarOutputInterface"); // X360 line 2819

    bool lbResult;
    if (lpAction->GetModifier())                                             // X360 lbz +2 (mxModifier) non-zero
    {
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface::RaceCarState* lpRaceCarState =
            lpActiveRaceCarOutputInterface->GetPlayerRaceCarState();         // X360 sub_82310240 (ICF)
        CGS_ASSERT(lpRaceCarState, "lpRaceCarState");                        // X360 line 2832

        lbResult = true;
        if ((lpAction->GetModifier() & BrnResource::ChallengeListEntryAction::KX_MODIFIER_IN_AIR) ==
            BrnResource::ChallengeListEntryAction::KX_MODIFIER_IN_AIR)       // X360 rlwinm 0,28,28 (& 8) == 8
        {
            lbResult = (lpRaceCarState->mfGas != 0.0f);                      // X360 lfs +0x404 (mfGas) != 0.0
        }
    }
    else
    {
        lbResult = true;
    }
    return lbResult;
}

// ----------------------------------------------------------------------------
// UpdateBurnouts -- X360 0x823345B0. Feed the player's boost telemetry into the freeburn skill
// scorer: with a live boost chain, score the chain length under the BURNOUTS skill; with the
// current boosting time positive, score it under the BOOST_TIME skill. The two boost-active /
// boost-inactive branches carry IDENTICAL bodies in the X360 asm (only the SetCurrentSkillScore
// bank-immediately flag differs: false while boosting, true when not) -- the duplicated shape is
// kept verbatim (spec pitfall 9).
// ----------------------------------------------------------------------------
void ChallengeManager::UpdateBurnouts(
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface)
{
    CGS_ASSERT(lpActiveRaceCarOutputInterface, "lpActiveRaceCarOutputInterface"); // X360 line 4104

    const EActiveRaceCarIndex lePlayerRaceCarIndex =
        lpActiveRaceCarOutputInterface->GetPlayerActiveRaceCarIndex();      // inlined +10328 read + "index set" assert (h:980)
    CGS_ASSERT(lePlayerRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "lePlayerRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");        // X360 line 4108
    CGS_ASSERT(lePlayerRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "lePlayerRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");     // X360 line 4109

    const BrnWorld::RaceCarEntityModuleIO::BoostOutputInfo* lpBoostInfo =
        lpActiveRaceCarOutputInterface->GetBoostOutputInfoN(lePlayerRaceCarIndex);
    CGS_ASSERT(lpBoostInfo, "lpBoostInfo");                                 // X360 line 4111

    if (lpBoostInfo->mbIsBoosting)                                          // X360 lbz +0 (mbIsBoosting)
    {
        if (lpBoostInfo->muNumChained)                                      // X360 lwz +0xC (muNumChained)
        {
            SetCurrentSkillScore(E_FREEBURN_SKILL_BURNOUTS,
                                 static_cast<f32>(lpBoostInfo->muNumChained), false);
        }
    }
    if (!lpBoostInfo->mbIsBoosting)                                         // duplicated branch (spec pitfall 9)
    {
        if (lpBoostInfo->muNumChained)
        {
            SetCurrentSkillScore(E_FREEBURN_SKILL_BURNOUTS,
                                 static_cast<f32>(lpBoostInfo->muNumChained), true);
        }
    }

    if (lpBoostInfo->mfCurrentBoostingTime > 0.0f)                          // X360 lfs +0x18 (mfCurrentBoostingTime) > 0.0
    {
        SetCurrentSkillScore(E_FREEBURN_SKILL_BOOST_TIME, lpBoostInfo->mfCurrentBoostingTime, false);
    }
}

} // namespace BrnGameState
