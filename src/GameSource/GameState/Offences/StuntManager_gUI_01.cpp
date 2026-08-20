// b5-decomp/src/GameSource/GameState/Offences/StuntManager_gUI_01.cpp
//
// Partfile of the BrnGameState::StuntManager TU (owning header
// GameSource/GameState/Offences/BrnStuntManager.h).
//
// ONE FUNCTION: StuntManager::CompleteAllStuntType @ X360 0x82399210.
//
// WHY IT IS HERE (it is not a gameplay path): StuntManager::Update calls it whenever the debug
// menu latched meDebugCompletedUnlockType (BrnStuntManager.cpp :: Update), so mounting
// BrnStuntManager.cpp WITHOUT this body is an LNK2019 on an already-mounted call site. It was
// the first of the measured UNDEF externals in the round-1 verify pass.
//
// WHAT IT DOES: the debug "collect every jump / smash / billboard" cheat. It walks the track's
// whole generic-region table, adds every region of the matching kind to the player's profile
// (county-classified), then posts game action 60 (E_ACTION_ON_STUNT_ELEMENT_COMPLETE_BY_TYPE) and
// runs the same achievement / trophy / special-car / completion-results fan-out one real
// completion would. It deliberately does NOT post actions 127 / 61 / 58 / 59 -- so it is not a
// second producer of the HUD popup this wave proves.
#include "GameSource/GameState/Offences/BrnStuntManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CgsDev::Assert::Begin/Fire/EndAssert (verbatim X360 strings)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // CgsModule::VariableEventQueue<13312,16>::AddEvent

#include "GameSource/GameState/BrnGameActions.h"            // OnStuntElementCompleteByTypeAction / E_ACTION_..._BY_TYPE
#include "GameSource/GameState/Progression/BrnProgressionManager.h" // GetProfile / GetAchievementManager / OnTrophyUnlock / ...
#include "GameSource/GameState/Progression/BrnProfile.h"             // Profile::AddStuntElement
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h" // OnCollectAllStunts
#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h"            // GetTriggerData

#include "SharedClasses/Trigger/BrnGenericRegion.h"         // BrnTrigger::GenericRegion / Type
#include "SharedClasses/Trigger/BrnTriggerData.h"           // TriggerData::GetGenericRegion / GetGenericRegionCount
#include "SharedClasses/World/BrnWorldRegion.h"             // BrnWorld::ECounty

namespace BrnGameState
{

namespace
{
    // Verbatim X360-baked source path for this TU's asserts (identical to BrnStuntManager.cpp's).
    const char* const KAC_FILE =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Offences/BrnStuntManager.cpp";

    // Same cast-through as the rest of the TU: the DWARF spells the parameter GameActionQueue (an
    // incomplete alias) while the X360 AddEvent call targets the <13312,16> queue directly.
    typedef CgsModule::VariableEventQueue<13312, 16> GameActionQueueImpl;
}

// ----------------------------------------------------------------------------
// CompleteAllStuntType @ 0x82399210
//
// Console body, statement for statement:
//   1. Map the StuntElementType to (a) the GENERIC-REGION kind that carries it and (b) the trophy
//      id its completion unlocks. The X360 immediates:
//        JUMP(0)      -> region kind 7  (E_TYPE_JUMP),            trophy 2
//        SMASH(1)     -> region kind 8  (E_TYPE_SMASH),           trophy 3
//        BILLBOARD(2) -> region kind 12 (E_TYPE_OVERDRIVE_BOOST), trophy 1
//      type >= COUNT asserts "Unknown stunt type." (line 427) and RETURNS -- the console's
//      `goto LABEL_5` jumps over the whole rest of the body.
//      (The 2/3/1 trophy mapping is the same one CheckForTrophyUnlocks uses; the enumerator NAMES
//       are still unrecovered -- the X360 passes raw li immediates. Same FLAG as there.)
//   2. Walk every generic region of the track (`Memor+68` == TriggerData::mpGenericRegions,
//      `Memor+72` == miGenericRegionCount, stride 56 == sizeof(GenericRegion) -- reached here by
//      the named accessors, which are the same pair Prepare's census uses). County-classify EVERY
//      region (FindTriggersCounty runs before the kind test on the console -- kept), and for the
//      matching kind add its element key (group id, or its own id when the group id is 0) to the
//      profile.
//   3. Post action 60 { type } (4 bytes), then OnCollectAllStunts / OnTrophyUnlock /
//      CheckForSpecialCarUnlocks / SendGameCompletionResults.
// ----------------------------------------------------------------------------
void StuntManager::CompleteAllStuntType(StuntElementType                    leStuntElementType,
                                        GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    BrnTrigger::GenericRegion::Type leRegionType;
    s32                             liTrophyId;

    if (leStuntElementType == E_STUNT_ELEMENT_TYPE_JUMP)
    {
        leRegionType = BrnTrigger::GenericRegion::E_TYPE_JUMP;              // X360 li r26, 7
        liTrophyId   = 2;                                                   // X360 `v7 = 7; v8 = 2;`
    }
    else if (leStuntElementType == E_STUNT_ELEMENT_TYPE_SMASH)
    {
        leRegionType = BrnTrigger::GenericRegion::E_TYPE_SMASH;             // X360 li r26, 8
        liTrophyId   = 3;                                                   // X360 li r22, 3
    }
    else if (static_cast<u32>(leStuntElementType) >= static_cast<u32>(E_STUNT_ELEMENT_TYPE_COUNT))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Unknown stunt type.", KAC_FILE, 427);
        CgsDev::Assert::EndAssert();
        return;   // X360 `goto LABEL_5` -- straight to the epilogue, nothing else runs
    }
    else
    {
        leRegionType = BrnTrigger::GenericRegion::E_TYPE_OVERDRIVE_BOOST;   // X360 li r26, 12
        liTrophyId   = 1;                                                   // X360 li r22, 1
    }

    // X360 `TriggerData_::GetMemor(mpTriggerQueryManager + 1568)` == the ResourcePtr<TriggerData>
    // main-memory pointer, i.e. TriggerQueryManager::GetTriggerData().
    const BrnTrigger::TriggerData* lpTriggerData = mpTriggerQueryManager->GetTriggerData();

    const s32 liGenericRegionCount = lpTriggerData->GetGenericRegionCount();
    for (s32 liGenericRegionIndex = 0; liGenericRegionIndex < liGenericRegionCount; ++liGenericRegionIndex)
    {
        const BrnTrigger::GenericRegion* lpGenericRegion =
            lpTriggerData->GetGenericRegion(liGenericRegionIndex);

        // The console classifies EVERY region, then tests the kind (the FindTriggersCounty call is
        // above the `if` in the asm). Kept in that order.
        const BrnWorld::ECounty leCounty = FindTriggersCounty(lpGenericRegion);

        if (lpGenericRegion->GetType() != leRegionType)
            continue;

        const CgsID lElementGroupId = lpGenericRegion->GetGroupId();
        const CgsID lElementKey     =
            (lElementGroupId != 0) ? lElementGroupId : lpGenericRegion->GetId();

        mpProgressionManager->GetProfile()->AddStuntElement(leStuntElementType, lElementKey, leCounty);
    }

    GameActionQueueImpl* lpActionQueueImpl = reinterpret_cast<GameActionQueueImpl*>(lpActionQueue);

    GameStateModuleIO::OnStuntElementCompleteByTypeAction lAllAction;
    lAllAction.meStuntElementType = leStuntElementType;
    lpActionQueueImpl->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAllAction),
                                GameStateModuleIO::E_ACTION_ON_STUNT_ELEMENT_COMPLETE_BY_TYPE, 4);

    // ⚠️ [gateui] ROUND-3 PARK (verify_r2_fixgsm F4 sweep). The console call here is
    //     mpProgressionManager->GetAchievementManager()->OnCollectAllStunts(leStuntElementType);
    // (X360 AchievementManagerBase::OnCollectAllStunts @0x8235AF38). A LINK park, not a
    // missing-body park: the body is real at
    // `AchievementManager/BrnGameStateAchievementManagerBase.cpp :: OnCollectAllStunts`, but that
    // TU is deliberately unmounted and mounting it opens SEVEN externals with no body anywhere in
    // the tree -- identical reason, identical list, to the OnCollectStunt park in
    // StuntManager_gUI_00.cpp :: ProcessStuntElement (see its long FLAG for the re-measurement,
    // which found SEVEN where the bat still says eight). RESTORE-WHEN those seven land.

    // ⚠️ [gateui] PARKED CALLS, NOT FABRICATED -- the ModeManager::HandleWorldStunt treatment
    // (StuntManager_gUI_00.cpp :: ProcessStuntElement, step 6). The console's tail is:
    //     mpProgressionManager->OnTrophyUnlock(liTrophyId);                     // 0x82389740
    //     mpProgressionManager->CheckForSpecialCarUnlocks();                    // 0x82396058
    //     mpProgressionManager->SendGameCompletionResults(lpActionQueueImpl);   // 0x82395C28
    // All three were PARKED by owner `deps` this wave with the measured blocker lists now
    // carried in BrnProgressionManager.h (missing: UnlockCarFromTrophy @0x8237B0E8, the
    // ProgressionData trophy-table header, ComputeCompletionPercentage @0x8238A198 (320 insns),
    // UnlockSpecialCars @0x8237AF38 (106 insns), and an mpModeManager back-pointer at X360
    // +133436 that nothing models). None has a body anywhere in b5-decomp/src and no link stub
    // stands in, so calling them is a hard LNK2019 that blocks the whole gsm mount.
    // ⓘ Off the HUD path: this whole function is the DEBUG-MENU "complete all of this type" arm
    // (StuntManager::CompleteAllStuntType @0x82399210, reached only from the debug menu). Its own
    // HUD-visible output -- game action 60 -> GuiEventStuntAllComplete(220) -- is posted ABOVE,
    // before these calls, and none of them writes lAllAction. Land them when the bodies do.
    (void)liTrophyId;
    (void)lpActionQueueImpl;
}

}
