// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnRoadRageModeScoring.cpp
// ============================================================================
// BrnGameState::RoadRageModeScoring -- the offline Road Rage sub-scorer ScoringSystem
// embeds by value at ss+0x4B40. This is the REAL TU for the type; it defines exactly the
// fifteen symbols BrnRoadRageModeScoringLinkStubs.cpp used to carry, so the mount swap is
// one-for-one (the two files must never be in one build -- LNK2005 on all fifteen).
//
// EVIDENCE MAP. Only ONE method has an out-of-line X360 symbol:
//   IncrementPlayerNumTakedowns          X360 0x823445D0 (asm read end to end below).
// Every other body is console-inlined at a caller; each is recovered from the site named
// on the method (the sub-scorer sits at ss+0x4B40, so its members are ss+0x4B40..+0x4B57:
// +0x00 miNumTakedownsAchieved, +0x04 miNumTakedownsAchievedForNextExtention,
// +0x08 muRoadRageTriggerExtension (s16), +0x0A muRoadRageExtensionTime (u16),
// +0x0C miTargetNumTakedowns, +0x10 miNextTimeIncreaseIndex, +0x14 mbDamageCritical-
// MessageNeedToBeSent, +0x15 mbPlayerDamageCritical, +0x16 mbPlayerCarDestroyed,
// +0x17 mbGameModeActive -- the DWARF member run, BrnRoadRageModeScoring.h:112-122):
//   Construct     ScoringSystem::Construct      0x8233809C..0x823380C8
//   ClearData     ScoringSystem::ClearData      0x8232A508..0x8232A534
//   Prepare       ModeManager::SetupGameMode    0x8234B588..0x8234B5C4
//   Update        ModeManager::PostWorldUpdate  0x8234AD6C..0x8234AD9C (mode-3 arm)
//   IsActive / GetNumTakedownsAchieved / GetTargetNumTakedowns
//                 ScoringSystem::WriteDataToOutput 0x8232AE98 (+0x4B57 / +0x4B40 / +0x4B4C)
//   SetTakeDownTarget
//                 ScoringSystem::CheckRoadRageMedalAwarded 0x823128FC (`stw r11, 0x4B4C`)
//   HasBeatenRoadRageTarget
//                 ModeManager::FinishCurrentMode 0x8234BBB4..0x8234BBC4 (signed `>=`)
//   DoesDamageCriticalMessageNeedToBeSent / ResetDamageCriticalMessageFlag
//                 HUDMessageLogic::GenerateCriticalDamageMessage 0x82395BE8 / 0x82395C10
//   Release / Destruct / PlayerCarWasDestroyed
//                 NO inline site found (see the FLAG on each). An exhaustive sweep of every
//                 ARTIST export for stores/loads at 0x4B50(..0x4B57( finds ONLY the sites
//                 listed above plus GUI code on unrelated bases, and ScoringSystem::Release
//                 @0x823124A0 never touches ss+0x4B40..0x4B57 -- consistent with empty bodies.
//
// A CONSOLE FACT WORTH STATING: nothing in the X360 image ever stores 1 into +0x14
// (mbDamageCriticalMessageNeedToBeSent), +0x15 (mbPlayerDamageCritical) or +0x16
// (mbPlayerCarDestroyed). The damage-critical flow that reaches the HUD on the X360 goes
// through ScoringSystem::OnRoadRagePlayerCrashed @0x823444B0 (action 205 / training 57),
// not through these three flags; their readers (GenerateCriticalDamageMessage) still exist
// and are still served faithfully by the getters below.
//
// Constants (DWARF BrnRoadRageModeScoring.cpp:25/:28): KU_TAKEDOWN_COVER_TIME == 3 is the
// `addi r11, r11, 3` @0x82344698. KF_TIME_REMAINING_AFTER_TAKEDOWN has NO reader in the X360
// body (the DWARF-era `lfTimeRemaining` arithmetic was compiled out) and its value is not in
// the image -- it is deliberately not defined here rather than guessed.
// ============================================================================

#include "GameSource/GameState/ModeManager/Scoring/BrnRoadRageModeScoring.h"

#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"   // ScoringSystem::{IsTimerActive, HasModeTimeExpired, ...}
#include "GameSource/GameState/BrnGameActions.h"                         // PlayerReachedRoadRageTarget / HUDMessageRoadRageTimeExtensionAction
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // CgsModule::VariableEventQueue<13312,16>::AddEvent
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT

namespace BrnGameState
{
    // DWARF BrnRoadRageModeScoring.cpp:28. Seconds of "cover" added on top of the rank's
    // extension time when a time extension is granted (`addi r11, r11, 3` @0x82344698).
    const u32 KU_TAKEDOWN_COVER_TIME = 3;

    // ---- lifecycle -----------------------------------------------------------------

    // Inlined at ScoringSystem::Construct 0x8233809C..0x823380C8: the identical store block
    // ClearData emits (miTargetNumTakedowns := -1 via r29, everything else := 0 via r30).
    void RoadRageModeScoring::Construct()
    {
        ClearData();
    }

    // Inlined at ModeManager::SetupGameMode 0x8234B588..0x8234B5C4 (r24 == 0, r15 == 1 from the
    // prologue `li r15, 1` @0x8234B388, r29 == GetRoadRageTakedownTarget(), r11 == rank+0x56 ==
    // ProgressionRankData::GetRoadRageExtensionTime()):
    //   stw r24, 0x4B40  miNumTakedownsAchieved                 = 0
    //   stw r24, 0x4B44  miNumTakedownsAchievedForNextExtention = 0
    //   stw r24, 0x4B50  miNextTimeIncreaseIndex                = 0
    //   stb r24, 0x4B54  mbDamageCriticalMessageNeedToBeSent    = false
    //   stb r24, 0x4B55  mbPlayerDamageCritical                 = false
    //   stb r24, 0x4B56  mbPlayerCarDestroyed                   = false
    //   stw r29, 0x4B4C  miTargetNumTakedowns                   = liTargetNumTakedowns
    //   stb r15, 0x4B57  mbGameModeActive                       = true
    //   sth r15, 0x4B48  muRoadRageTriggerExtension             = 1
    //   sth r11, 0x4B4A  muRoadRageExtensionTime                = luRoadRageExtensionTime
    // The DWARF declares the bool return; the inlined site has no failure leg, so it is `true`.
    bool RoadRageModeScoring::Prepare(s32 liTargetNumTakedowns, u16 luRoadRageExtensionTime)
    {
        miNumTakedownsAchieved                 = 0;
        miNumTakedownsAchievedForNextExtention = 0;
        muRoadRageTriggerExtension             = 1;
        muRoadRageExtensionTime                = luRoadRageExtensionTime;
        miTargetNumTakedowns                   = liTargetNumTakedowns;
        miNextTimeIncreaseIndex                = 0;
        mbDamageCriticalMessageNeedToBeSent    = false;
        mbPlayerDamageCritical                 = false;
        mbPlayerCarDestroyed                   = false;
        mbGameModeActive                       = true;
        return true;
    }

    // Inlined at ModeManager::PostWorldUpdate 0x8234AD6C..0x8234AD9C, the E_MODE_ROAD_RAGE (3)
    // arm: the WHOLE arm is this one assert (baked BrnRoadRageModeScoring.cpp:91 == 0x5B), then a
    // branch to the common exit. No store into ss+0x4B40..0x4B57 and no other call -- the X360
    // body is the assert and nothing else, so nothing else is written here.
    void RoadRageModeScoring::Update(const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
                                     f32 /* lfSimTimeStep */)
    {
        CGS_ASSERT(lpActiveRaceCarInterface != NULL, "lpActiveRaceCarInterface != NULL");
    }

    // FLAG no inline site found. ScoringSystem::Release @0x823124A0 (the only lifecycle caller)
    // touches none of ss+0x4B40..0x4B57, which is what an empty body inlines to. The bool return
    // follows the sibling scorers' convention (a Release that cannot fail answers true).
    bool RoadRageModeScoring::Release()
    {
        return true;
    }

    // FLAG no inline site found. No ARTIST export stores into ss+0x4B40..0x4B57 on a destruct path;
    // an empty body inlines to nothing, and nothing is what the image shows.
    void RoadRageModeScoring::Destruct()
    {
    }

    // Inlined at ScoringSystem::ClearData 0x8232A508..0x8232A534: every member zeroed EXCEPT
    // miTargetNumTakedowns, which the console stores as -1 (`li r27,-1; stw r27,0x4B4C`).
    void RoadRageModeScoring::ClearData()
    {
        miNumTakedownsAchieved                 = 0;
        miNumTakedownsAchievedForNextExtention = 0;
        muRoadRageTriggerExtension             = 0;
        muRoadRageExtensionTime                = 0;
        miTargetNumTakedowns                   = -1;
        miNextTimeIncreaseIndex                = 0;
        mbDamageCriticalMessageNeedToBeSent    = false;
        mbPlayerDamageCritical                 = false;
        mbPlayerCarDestroyed                   = false;
        mbGameModeActive                       = false;
    }

    // ------------------------------------------------------------------------
    // X360 0x823445D0 -- the type's one out-of-line symbol. Register map: r31 == this,
    // r30 == lpScoringSystem, r28 == &lCurrentTime, r29 == lpOutputActionQueue.
    //
    //   0x823445EC  lbz  r11, 0x17(r31)  ; mbGameModeActive == 0            -> return
    //   0x823445F8  lwz  r11, 0(r30)     ; ss+0 == mStartTime.miSeconds < 0 -> return
    //                                      (== !IsTimerActive(), the predicate the Timer
    //                                      partfile pins on exactly this word)
    //   0x8234460C  bl   HasModeTimeExpired(ss, &time) ; true               -> return
    //   0x8234461C..0x82344638
    //               miNumTakedownsAchieved++ ; miNumTakedownsAchievedForNextExtention++
    //               cmpw miTargetNumTakedowns, new achieved -> equal:
    //   0x82344640  AddEvent(queue, &stack-byte, 103 == 'g', size 1)        [PlayerReachedRoadRageTarget]
    //   0x82344654  lwz  r11, 0x5D0C(r30) ; meCurrentMedalAchieved == 0 (GOLD) -> return
    //   0x82344668  bl   CheckRoadRageMedalAwarded(ss, miNumTakedownsAchieved)
    //   0x8234466C  lhz+extsh muRoadRageTriggerExtension ; miNum...ForNextExtention < it -> return
    //   0x8234468C  bl   GetModeTimeRemaining(&var_38, ss, &time)   ; result stored, then
    //                                      OVERWRITTEN by the next two stores -- the DWARF's
    //                                      `lfTimeRemaining` local is dead on the X360; only the
    //                                      call (it writes ss.mTimeRemaining) survives.
    //   0x82344690..0x823446B0
    //               f1 = (f32)(u32)(muRoadRageExtensionTime + 3) ; bl IncreaseTimeLimit(ss, f1)
    //   0x823446B4..0x823446E0
    //               var_38.u32 = muRoadRageExtensionTime
    //               AddEvent(queue, &var_38, 255, 4)   ; TWICE, back to back, same record
    //               AddEvent(queue, &var_38, 255, 4)
    //   0x823446E8  miNumTakedownsAchievedForNextExtention = 0
    //
    // THE DOUBLE 255 POST IS THE CONSOLE'S OWN. The DWARF body hint for this function lists
    // AddGameAction<HUDMessageRoadRageTimeExtensionAction> TWICE as well, so it is source-level
    // (two explicit posts), not a decompiler artefact; reproduced as two posts.
    // ------------------------------------------------------------------------
    void RoadRageModeScoring::IncrementPlayerNumTakedowns(ScoringSystem* lpScoringSystem,
                                                          CgsSystem::Time lCurrentTime,
                                                          GameStateModuleIO::GameActionQueue* lpOutputActionQueue)
    {
        if (!mbGameModeActive)
        {
            return;
        }
        if (!lpScoringSystem->IsTimerActive())
        {
            return;
        }
        if (lpScoringSystem->HasModeTimeExpired(lCurrentTime))
        {
            return;
        }

        ++miNumTakedownsAchieved;
        ++miNumTakedownsAchievedForNextExtention;

        if (miNumTakedownsAchieved == miTargetNumTakedowns)
        {
            // DWARF .cpp:188 `PlayerReachedRoadRageTarget lPlayerReachedRoadRageTarget;` -- an
            // empty record, posted with size 1 (`li r6,1 / li r5,0x67`).
            GameStateModuleIO::PlayerReachedRoadRageTarget lPlayerReachedRoadRageTarget;
            lpOutputActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lPlayerReachedRoadRageTarget),
                                          GameStateModuleIO::E_ACTION_PLAYER_REACHED_ROAD_RAGE_TARGET,
                                          sizeof(lPlayerReachedRoadRageTarget));
        }

        // The medal ladder runs only until gold is banked (ss+0x5D0C == meCurrentMedalAchieved,
        // tested against 0 == E_CURRENT_MEDAL_TARGET_TIME_GOLD -- the same word / same test
        // CheckRoadRageMedalAwarded opens with).
        if (lpScoringSystem->GetCurrentMedalAchieved() == E_CURRENT_MEDAL_TARGET_TIME_GOLD)
        {
            return;
        }

        lpScoringSystem->CheckRoadRageMedalAwarded(static_cast<u32>(miNumTakedownsAchieved));

        if (miNumTakedownsAchievedForNextExtention >= static_cast<s32>(muRoadRageTriggerExtension))
        {
            // Kept for its side effect (GetModeTimeRemaining refreshes ss.mTimeRemaining); the
            // returned value is dead on the X360 (see the register map above).
            const CgsSystem::Time lTimeRemaining = lpScoringSystem->GetModeTimeRemaining(lCurrentTime);
            (void)lTimeRemaining;

            lpScoringSystem->IncreaseTimeLimit(
                static_cast<f32>(static_cast<u32>(muRoadRageExtensionTime) + KU_TAKEDOWN_COVER_TIME));

            // DWARF .cpp:172 `HUDMessageRoadRageTimeExtensionAction lHUDMessageAction;` -- the
            // u32 extension time WITHOUT the cover seconds (`lhz r11, 0xA(r31)` @0x823446B4).
            GameStateModuleIO::HUDMessageRoadRageTimeExtensionAction lHUDMessageAction;
            lHUDMessageAction.muExtensionTime = muRoadRageExtensionTime;
            lpOutputActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lHUDMessageAction),
                                          GameStateModuleIO::E_ACTION_HUD_MESSAGE_ROAD_RAGE_TIME_EXTENSION,
                                          sizeof(lHUDMessageAction));
            lpOutputActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lHUDMessageAction),
                                          GameStateModuleIO::E_ACTION_HUD_MESSAGE_ROAD_RAGE_TIME_EXTENSION,
                                          sizeof(lHUDMessageAction));

            miNumTakedownsAchievedForNextExtention = 0;
        }
    }

    // ---- queries -------------------------------------------------------------------

    // ScoringSystem::WriteDataToOutput 0x8232AE98: `lwz 0x4B40(ss)` inside the IsActive gate.
    s32 RoadRageModeScoring::GetNumTakedownsAchieved() const
    {
        return miNumTakedownsAchieved;
    }

    // ScoringSystem::WriteDataToOutput 0x8232AE98: `lwz 0x4B4C(ss)` inside the IsActive gate.
    s32 RoadRageModeScoring::GetTargetNumTakedowns() const
    {
        return miTargetNumTakedowns;
    }

    // FLAG no inline site found: no ARTIST export reads ss+0x4B56 (and none ever writes 1 to it).
    // The DWARF name fixes the member; the getter is the only body such a name can have.
    bool RoadRageModeScoring::PlayerCarWasDestroyed() const
    {
        return mbPlayerCarDestroyed;
    }

    // HUDMessageLogic::GenerateCriticalDamageMessage 0x82395BE8: `lbz r11, 0x4B54(ss)` gates the
    // GUI post (VariableEventQueue<256,16>::AddEvent id 52, size 1).
    bool RoadRageModeScoring::DoesDamageCriticalMessageNeedToBeSent() const
    {
        return mbDamageCriticalMessageNeedToBeSent;
    }

    // HUDMessageLogic::GenerateCriticalDamageMessage 0x82395C10: `stb 0, 0x4B54(ss)` right after
    // that post.
    void RoadRageModeScoring::ResetDamageCriticalMessageFlag()
    {
        mbDamageCriticalMessageNeedToBeSent = false;
    }

    // ScoringSystem::WriteDataToOutput 0x8232AE98: `lbz 0x4B57(ss)` selects the road-rage arm.
    bool RoadRageModeScoring::IsActive()
    {
        return mbGameModeActive;
    }

    // ScoringSystem::CheckRoadRageMedalAwarded 0x823128FC: `stw r11, 0x4B4C(ss)` with r11 ==
    // mauiMedalScores[meCurrentMedalTarget] (the committed body in BrnScoringSystem_UpdateB.cpp
    // calls this setter at that point).
    void RoadRageModeScoring::SetTakeDownTarget(s32 liTargetNumTakedowns)
    {
        miTargetNumTakedowns = liTargetNumTakedowns;
    }

    // ModeManager::FinishCurrentMode 0x8234BBB4..0x8234BBC4 (jumptable case E_MODE_ROAD_RAGE):
    // `lwz 0x58F0(mm); lwz 0x58FC(mm); cmpw; bge` == miNumTakedownsAchieved >= miTargetNumTakedowns,
    // SIGNED. ScoringSystem::HasBeatenRoadRageTarget forwards here (BrnScoringSystem_Lookup.cpp).
    bool RoadRageModeScoring::HasBeatenRoadRageTarget() const
    {
        return miNumTakedownsAchieved >= miTargetNumTakedowns;
    }
}
