// b5-decomp/src/GameSource/GameState/GameStateModule_gUI_00.cpp
//
// Partfile of the BrnGameState::GameStateModule TU (owning header BrnGameStateModule.h; the rest
// of the module's committed bodies are in BrnGameStateModule.cpp).
//
// THE gateui WAVE'S GameState-SIDE PLUMBING -- the four bodies that carry a broken prop from the
// world module all the way to a game action the GUI bridge can translate:
//
//     world OutputBuffer::GetGameEventQueue()            (produced by PropEntityModule::
//                                                         ProcessContacts -> the bridge legs)
//        -> PostWorldUpdateStuntBringUp                  (X360 PostWorldUpdate @0x8238F358)
//             * refresh mLastActiveRaceCarInterface      <- WITHOUT THIS NOTHING ARMS
//             * Append into mGameEventCarryQueue
//             * the stunt scorer tick                    (leg 3, arms mbRecentStunt)
//             * HUDMessageLogic::PostWorldUpdate         (leg 4, DRAINS mbRecentStunt --
//                                                         without it UpdateBufferedScore's
//                                                         !mbRecentStunt assert fires)
//        -> PreWorldUpdateStuntBringUp                   (X360 PreWorldUpdate @0x823A5328)
//             * ProcessGameEventsPropHitBringUp          (X360 ProcessGameEvents @0x823A0A18,
//                                                         case 111 -> StuntManager::OnPropHit)
//             * TriggerQueryManager::UpdateTriggers      (arms maActiveTriggers for NEXT frame)
//             * StuntManager::Update                     (consumes the latch ->
//                                                         ProcessStuntElement -> action 58)
//
// Each function's console attestation, and each deliberate deviation, is written out at its
// declaration in BrnGameStateModule.h and again at its body below. Nothing here is fabricated:
// every reduction is named as a reduction.
#include "GameSource/GameState/BrnGameStateModule.h"

#include <stdlib.h>                                                     // getenv ([UI-gate] diag)
#include <string.h>                                                     // memcpy / memset (the case-20 payload view)

#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // CgsDev::Log::gpDebugPrint
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // VariableEventQueue<1536,16>

#include "GameSource/GameState/BrnGameStateModuleIO.h"                  // OutputBuffer (lock + GetGameActionQueue)
#include "GameSource/GameState/BrnGameEvents.h"                         // RecordPropHitEvent / E_EVENT_RECORD_PROP_HIT / E_EVENT_CHANGE_WORLD_REGION
#include "GameSource/GameState/ImageManager/BrnGameStateImageManagerBase.h" // WorldRegionChangeEvent (the case-115 payload)
#include "GameSource/GameState/Offences/BrnStuntManager.h"              // StuntManager::OnPropHit / Update
#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h" // UpdateTriggers / GetActiveTrigger*
#include "GameSource/GameState/DeveloperChallengeManager/BrnDeveloperChallengeManager.h" // the accessor's return type

#include "SharedClasses/Trigger/BrnTriggerData.h"                       // TriggerData::GetRegion
#include "SharedClasses/Trigger/BrnTriggerBase.h"                       // TriggerRegion::GetType
#include "SharedClasses/Trigger/BrnGenericRegion.h"                     // GenericRegion::Type (SMASH / OVERDRIVE_BOOST)

// ---- [D4 stuntrace WAVE D -- THE PUMP] ------------------------------------------------------
#include "GameSource/GameState/ModeManager/BrnModeManager.h"            // ModeManager::Pre/PostWorldUpdate, StartGameMode
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"     // GameMode::GetCurrentState / GetIntroDurationSeconds
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h" // StartGameModeParams (the case-20 local)
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"  // ScoringSystem::GetStuntScorer
#include "GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring.h" // StuntModeScoring::Update (THE scoring tick)
#include "GameSource/GameState/ModeManager/Hud/BrnHUDMessageLogic.h"     // HUDMessageLogic::PostWorldUpdate (THE latch drain)
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"  // CgsSystem::TimerStatusInterface (the pump's new argument)

namespace BrnGameState
{

// ============================================================================
// â­ [gateui] GetDeveloperChallengeManager -- the body behind the declaration the StreetManager
// wave added with no member behind it.
//
// The console never emits an accessor for this subobject: every call site reaches it through the
// inlined `mpGameStateModule + 185712` pointer adjust, and both of them assert it non-null with
// the accessor spelled out --
//   StreetManager::ProcessNewRoadScore      @0x823496C8 ("mpGameStateModule->GetDeveloperChallengeManager()")
//   StuntManager::ProcessStuntElement       @0x8239CDB0 (same string, BrnStuntManager.cpp:695)
// De-inlined here over the real embedded member (BrnGameStateModule.h,
// mDeveloperChallengeManager), so no reconstructed body has to poke a byte offset.
// ============================================================================
DeveloperChallengeManager* GameStateModule::GetDeveloperChallengeManager()
{
    return &mDeveloperChallengeManager;
}

// ============================================================================
// â­â­ [gateui] PostWorldUpdateStuntBringUp -- the two stunt-chain legs of the console's
// PostWorldUpdate @0x8238F358, which reads (inside its `LockForRead(lpPostWorldInput)` bracket):
//
//     v11 = sub_8231D2C0(a4);                    // PostWorldInputBuffer::GetActiveRaceCarOutputInterface
//     XMemCpy(a1 + 235488, v11, 10480);          // -> mLastActiveRaceCarInterface
//     ...
//     v15 = sub_8231D0C8(a4);                    // PostWorldInputBuffer::GetGameEventQueue
//     VariableEventQueue<1536,16>::Append<1536,16>(a1 + 248384, v15);   // -> the carry queue
//
// See the header for the full FLAG on why the two values arrive as arguments here rather than
// through a PostWorldInputBuffer, and for why the interface refresh is the load-bearing half.
// ============================================================================
void GameStateModule::PostWorldUpdateStuntBringUp(
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
                                                      lpActiveRaceCarOutputInterface,
        const CgsModule::VariableEventQueue<1536, 16>* lpWorldGameEventQueue,
        f32                                           lfDelta)
{
    // ---- leg 1: refresh the cached active-race-car snapshot ---------------------------------
    // âš ï¸ COPIED BY ASSIGNMENT, NEVER AT THE CONSOLE'S LITERAL 10480 BYTES. 10480 is the X360
    // sizeof; on the host every embedded pointer in that interface widened, so a literal byte
    // count would truncate the tail (or, if the host object were smaller, run off the end). Same
    // class of correction the tree already made to StreetManager::LoadDistrictMap's 24-byte
    // acquire record and to CreateIOBuffer<T>'s zero-fill.
    if (lpActiveRaceCarOutputInterface != 0)
    {
        mLastActiveRaceCarInterface = *lpActiveRaceCarOutputInterface;
    }

    // ---- leg 2: fold the world's game events into the carry queue ----------------------------
    // The world module's OutputBuffer::GetGameEventQueue() is the SAME <1536,16> queue type
    // (BrnWorldModuleIO.h typedefs its GameEventQueue to it), so this is the console's own bulk
    // Append<1536,16>, unchanged. The carry queue is Construct()ed by GameStateModule::Construct
    // -- the console does the same, and an un-Constructed VariableEventQueue has no buffer bound.
    if (lpWorldGameEventQueue != 0)
    {
        mGameEventCarryQueue.Append(*lpWorldGameEventQueue);
    }

    // [DIAG] NOT IN THE X360 BINARY -- the runtime probe for fix3bridge's round-3 carry-queue
    // gate, which this lane owns because mGameEventCarryQueue is private here. Same logger and
    // same env guard (BRN_PROP_DIAG) as the "[prop-diag] BREAK" rung and the "[UI-gate] armed"
    // rung below.
    //
    // WHAT IT PROVES: the producer above now runs behind the SAME predicate as the drain in
    // PreWorldUpdateStuntBringUp (BrnGameModule.cpp's lbGameStateWorldLegRuns:
    // !IsVideoState() && !mbDiskError && leState == E_MGS_IN_GAME). Console-correct on a
    // boot-drive is therefore: n never exceeds ONE sub-step worth of events, and reads 0 on
    // the first line after each pre-world leg. The invariant is NOT "0 at end of frame" --
    // the console legitimately carries one frame of events across the world leg.
    // Failure signature if the gate is ever loosened, grep the boot log for:
    //     "ERROR: Overflowed variable event queue when appending another one"
    // First-N guarded so a busy sub-step cannot flood the log.
    {
        static const bool sbCarryDiag  = (getenv("BRN_PROP_DIAG") != 0);
        static s32        siCarryLines = 0;
        const s32         KI_CARRY_DIAG_MAX = 64;
        if (sbCarryDiag && siCarryLines < KI_CARRY_DIAG_MAX && CgsDev::Log::gpDebugPrint != 0)
        {
            ++siCarryLines;
            *CgsDev::Log::gpDebugPrint
                << "[UI-gate] carry n=" << mGameEventCarryQueue.GetLength() << "\n";
        }
    }

    // ============================================================================
    // â­â­â­ [D4 stuntrace WAVE D] LEG 3 -- THE SCORING TICK (console PostWorldUpdate #19).
    //
    // CONSOLE POSITION, exact. GameStateModule::PostWorldUpdate @0x8238F358's `bl` stream:
    //     #6/#7   GetActiveRaceCarOutputInterface + XMemCpy   <- leg 1 above
    //     #14/#15 GetGameEventQueue + Append<1536,16>         <- leg 2 above
    //     #18     GameStateModule::CacheTakedownManagerPostWorldInputData   (no body on this tree)
    //     #19     BrnGameState::ModeManager::PostWorldUpdate   <- THIS LEG, immediately after leg 2
    //     #23     TriggerQueryManager::PostWorldUpdate
    //
    // â›”â›” WHY THIS IS AN EXTRACTED LEG AND NOT `mModeManager.PostWorldUpdate(...)`.
    // The committed ModeManager::PostWorldUpdate (BrnModeManager_WorldTick.cpp:532) takes a
    // `const GameStateModuleIO::PostWorldInputBuffer*` and dereferences it unconditionally
    // (GetActiveRaceCarOutputInterface, CheckForOutOfRangeCarsReachingFinish(buffer),
    // mpCurrentGameMode->PostWorldUpdate(buffer)). NOTHING ON THIS BUILD CREATES ONE: the console
    // stages it in DoUpdate_GameStatePostWorld @0x823E92A8 via CreateIOBuffer<PostWorldInputBuffer>,
    // which is not reconstructed, and -- unlike the PRE-world twin, where the module owns a
    // stand-in (mpPreWorldInputBuffer) -- there is no post-world sibling.
    // âš  AND SYNTHESISING ONE WOULD BE A LAYOUT LIE, which is exactly the bug class this campaign
    // keeps re-catching: PostWorldInputBuffer's active-race-car seat is
    // `u8 mActiveRaceCarOutputInterfaceStorage[0x2890]` -- RAW X360-SIZED BYTES
    // (BrnGameStateModuleIO.h) -- so GetActiveRaceCarOutputInterface() on a home-made buffer hands
    // back storage that is NOT a host RCEntityActiveRaceCarOutputInterface, and every read through
    // it lands at an X360 offset on an x64 object. mLastActiveRaceCarInterface, by contrast, is a
    // REAL host-typed member with a real writer (leg 1, immediately above), so this leg takes it
    // DIRECTLY -- the same "THE ARGUMENTS ARE THE DEVIATION, NOT THE BODY" deviation this entry
    // point already carries, for the same reason.
    //
    // WHAT IS REPRODUCED, verbatim from BrnModeManager_WorldTick.cpp's own reconstruction of
    // the per-mode scorer fork at 0x8234AD2C..0x8234AD90:
    //     if (meCurrentGameModeType == E_MODE_STUNT_ATTACK && IsGameModeInProgress(mode))
    //         mScoringSystem.GetStuntScorer()->Update(lpActiveRaceCarOutput, lfDelta);
    // reached through ModeManager::GetScoringSystem() and ScoringSystem::GetStuntScorer(), both
    // public named accessors -- NO offset off `this` (hazards H9). This IS the call that drives
    // StuntModeScoring::Update -> UpdateStunts -> the four detectors -> UpdateScore.
    //
    // [X] NOT REPRODUCED, named rather than faked -- every other leg of the console's
    // PostWorldUpdate, because each one needs the buffer: CheckForOutOfRangeCarsReachingFinish,
    // GameMode::PostWorldUpdate (vtbl slot 3), the player-scoring-slot binding sweep +
    // PlayerHasSpawned, UpdateTeamStats, UpdateDistanceToPlayer, StoreCarIds, UpdateGeneralStats,
    // UpdateNumberOfCarsInMode, DetectPlayerStationary, and the takedown/crash pair. Behaviour cost
    // on an offline stunt run: the mode's own post-world hook and the general per-car stats do not
    // run; the STUNT SCORE does, which is what this wave's oracle needs.
    //
    // âš  StuntModeScoring::Update opens with CGS_ASSERT(mbStuntModeActive), whose only writer is
    // StuntModeScoring::Activate <- ScoringSystem::OnModeStart(case 7) <- ModeManager::
    // UpdateCurrentMode <- StuntAttackMode::Start. The mode-7 + IN_PROGRESS gate below is what
    // keeps this leg behind that writer, exactly as the console's fork does -- do not widen it.
    //
    // DELETE-WHEN a real PostWorldInputBuffer exists (DoUpdate_GameStatePostWorld lands, or the
    // module grows a post-world stand-in the way it grew mpPreWorldInputBuffer): this whole block
    // then collapses to `mModeManager.PostWorldUpdate(lpPostWorldInput, lfDelta);`.
    // ============================================================================
    {
        ModeManager* lpModeManager = GetModeManager();
        const GameMode* lpCurrentGameMode = lpModeManager->GetCurrentGameMode();

        // ------------------------------------------------------------------------------------
        // â­â­â­ [stuntrace frontier round 3, 2026-08-27] THE PLAYER-SCORING-SLOT BINDING SWEEP --
        // the LAST producer of the three-leg chain, lifted out of ModeManager::PostWorldUpdate
        // (BrnModeManager_WorldTick.cpp:629-676, console 0x8234AAF0..0x8234AB7C) into this
        // extracted-leg block for the SAME reason the stunt-scorer fork below is here: the
        // committed ModeManager::PostWorldUpdate dereferences a PostWorldInputBuffer nothing on
        // this build creates, while mLastActiveRaceCarInterface is a real host-typed member with
        // a real writer (leg 1 of this very function). "THE ARGUMENTS ARE THE DEVIATION, NOT
        // THE BODY" -- the loop below is that function's own, statement for statement.
        //
        // â“˜ POSITION IS THE CONSOLE'S: the sweep runs BEFORE the per-mode scorer fork
        // (0x8234AB10 vs 0x8234AD2C), and inside the same `mpCurrentGameMode != NULL` gate.
        //
        // â›” WHY IT IS SUDDENLY LOAD-BEARING. ScoringSystem::SetPlayerRaceCarIndex is the ONLY
        // writer of a per-car record's active-race-car index anywhere in the tree
        // (BrnScoringSystem_Lifecycle.cpp:220 -- ScoringSystem::AddPlayer stamps
        // E_ACTIVE_RACE_CAR_INDEX_INVALID, never a real car), and ScoringSystem::GetCarData is a
        // linear search for a record carrying the queried index. With the sweep parked, GetCarData
        // could only ever return NULL, and the first consumer to dereference it -- ModeManager::
        // FinishCurrentMode -> ScoringSystem::StopModeTimer, at the end of the first offline
        // stunt run -- crashed. RUN EVIDENCE scratch/flow_run/20260827_140514/BrnGame.log:
        //     [ASSERT 31113] lpCarData (BrnScoringSystem_Timer.cpp:341)
        //     [EXCEPTION] EXCEPTION_ACCESS_VIOLATION ... access violation READING 0x18
        //         StopModeTimer + 0xE5 <- FinishCurrentMode + 0x3FC <- ModeManager::PreWorldUpdate
        // -- assert-is-not-a-guard: StopModeTimer's `lpCarData != NULL` tripwire fires and falls
        // through into lpCarData->GetScoreData()->GetDistanceToFinishLive(), CarScoreData +0x18
        // off a null CarData (rdi == 0 in the register dump). Exactly the shape of the
        // AddFinishedRaceEvent null-queue defect one function earlier, and the same fix: run the
        // console's missing producer, not a guard at the consumer.
        //
        // â“˜ THE OTHER TWO PRODUCERS landed with this one and the sweep is inert without them:
        // RaceCarEntityModule::HandlePrepareForModeAction now writes the module's
        // maActiveRaceCarForPlayerScoringIndex (the extracted SetUpPlayerCarForMode tail), and
        // RaceCarEntityModule::PostPhysicsUpdate now publishes it into the interface this loop
        // reads (CopyActiveRaceCarToPlayerScoringMappingToOutput, console @0x823075D8).
        //
        // âš ï¸ E_ACTIVE_RACE_CAR_INDEX_COUNT (8), NOT _INVALID (-1), is this table's empty-slot
        // value -- console `cmpwi r28, 8`, and RCEntityActiveRaceCarOutputInterface::Clear seeds
        // every cell to it. The two sentinels are distinct on this path; do not merge them.
        //
        // [X] NOT REPRODUCED, named rather than faked: the first-bind
        // `mpCurrentGameMode->PlayerHasSpawned(leActiveRaceCarIndex)` hook (vtbl slot 19).
        // ModeManager::GetCurrentGameMode() is const-only on this tree, and PlayerHasSpawned is a
        // non-const virtual, so calling it from here would need a header change. COST MEASURED,
        // AND IT IS ZERO ON THIS PATH: GameMode's base body is the console's folded `blr`
        // (BrnGameMode.cpp:697) and the image's ONLY override is OnlineFreeBurnLobbyMode's
        // @0x823315A8 -- an online mode. DELETE-WHEN ModeManager grows a non-const
        // GetCurrentGameMode(), or when this block collapses into the real PostWorldUpdate.
        // ------------------------------------------------------------------------------------
        if (lpCurrentGameMode != 0)
        {
            ScoringSystem* lpScoringSystem = lpModeManager->GetScoringSystem();

            for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
            {
                const GameStateModuleIO::EPlayerScoringIndex lePlayerScoringIndex =
                    static_cast<GameStateModuleIO::EPlayerScoringIndex>(liSlot);

                const ::EActiveRaceCarIndex leActiveRaceCarIndex =
                    mLastActiveRaceCarInterface.GetActiveRaceCarIndex(lePlayerScoringIndex);

                if (leActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_COUNT)
                {
                    continue;
                }
                if (!lpScoringSystem->IsPlayerInScoringSystem(lePlayerScoringIndex))
                {
                    continue;
                }

                CarData* lpCarData =
                    lpScoringSystem->GetCarDataFromPlayerScoringIndex(lePlayerScoringIndex);
                const bool lbFirstBind =
                    (lpCarData->GetActiveRaceCarIndex() == E_ACTIVE_RACE_CAR_INDEX_INVALID);

                lpScoringSystem->SetPlayerRaceCarIndex(lePlayerScoringIndex, leActiveRaceCarIndex);

                // [DIAG] NOT IN THE X360 BINARY -- the first-bind rung, one line per binding.
                // It fires exactly where the console would have called PlayerHasSpawned, so it
                // doubles as the park's own tripwire: pairs 1:1 with the "[scoring-map]" line
                // BrnRaceCarEntityModule_ModeArming.cpp emits at prepare-for-mode. Bounded by
                // the first-bind edge (a bound slot never re-binds), so it cannot flood.
                if (lbFirstBind && CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[scoring-bind] player scoring slot " << liSlot
                        << " bound to active race car " << static_cast<s32>(leActiveRaceCarIndex)
                        << "\n";
                }
            }
        }

        if (lpModeManager->GetCurrentGameModeType() == GameStateModuleIO::E_MODE_STUNT_ATTACK &&
            lpCurrentGameMode != 0 &&
            lpCurrentGameMode->GetCurrentState() == GameStateModuleIO::E_GMS_IN_PROGRESS)
        {
            lpModeManager->GetScoringSystem()->GetStuntScorer()->Update(
                &mLastActiveRaceCarInterface, lfDelta);
        }
    }

    // ============================================================================
    // ⭐⭐⭐ [stuntrace 2026-08-27] LEG 4 -- THE STUNT-SCORER LATCH DRAIN
    // (console PostWorldUpdate #~40, HUDMessageLogic::PostWorldUpdate @0x8234B0E8).
    //
    // ⛔ WHY THIS LEG EXISTS -- IT IS A MISSING CONSUMER, NOT A NEW FEATURE.
    // StuntModeScoring::UpdateBufferedScore (0x8232C118) opens with
    //     CGS_ASSERT(!mbRecentStunt, "!mbRecentStunt")
    // and, further down, ARMS that latch itself (`mbRecentStunt = mRecentStunt.miStuntScore > 0`)
    // every time a stunt banks. The invariant only holds because something drains the latch
    // between two frames -- and in the whole X360 image exactly ONE thing does:
    //     HUDMessageLogic::GenerateStuntMessage (0x82394DF8)
    //       -> StuntModeScoring::WasStuntRecentlyPerformed (0x82313280, vtable slot +0x18)
    // reached from HUDMessageLogic::PostWorldUpdate's case-7 arm. Neither query has a single
    // direct xref in the image; both are vtable-dispatched from there and nowhere else.
    // On the console the whole cycle happens inside ONE ModeManager::PostWorldUpdate: the scorer
    // fork at 0x8234AD2C arms the latch (leg 3 above), the HUD pump at 0x8234B0E8 drains it.
    // With the HUDMessageLogic lifecycle parked, leg 3 armed a latch nothing ever read, and the
    // assert fired on the SECOND stunt banked in any offline stunt race:
    //     [ASSERT 1] !mbRecentStunt (BrnStuntModeScoring_UpdatePass.cpp:352)
    //       StuntModeScoring::UpdateBufferedScore <- ::Update <- PostWorldUpdateStuntBringUp
    // ⚠ ASSERT-IS-NOT-A-GUARD: the fix is the missing producer-side consumer, never a softened
    // assert -- the tripwire is correct and stays exactly as it is.
    //
    // ⓘ POSITION IS THE CONSOLE'S: after the per-mode scorer fork, and -- like the console --
    // OUTSIDE the `mpCurrentGameMode != NULL` gate and outside the mode-7/IN_PROGRESS gate.
    // HUDMessageLogic::PostWorldUpdate runs every post-world tick, mode or not; its own latched
    // mode member is what selects the arm, and E_MODE_NONE selects none.
    //
    // ⛔ WHY IT IS AN EXTRACTED LEG, same reason as legs 1-3: the committed
    // ModeManager::PostWorldUpdate dereferences a PostWorldInputBuffer nothing on this build
    // creates, so it has no call site. mLastActiveRaceCarInterface (leg 1's own output) and the
    // ModeManager's public named accessors are real, so this leg takes them directly. THE
    // ARGUMENTS ARE THE DEVIATION, NOT THE BODY -- the console passes ten, the mounted body reads
    // four, and BrnHUDMessageLogic.cpp names every dropped argument and every unmounted arm.
    //
    // DELETE-WHEN a real PostWorldInputBuffer exists: this block collapses into
    // `mModeManager.PostWorldUpdate(lpPostWorldInput, lfDelta)` together with legs 1-3.
    // ============================================================================
    {
        ModeManager* lpModeManager = GetModeManager();

        lpModeManager->GetHUDMessageLogic()->PostWorldUpdate(
            &mLastActiveRaceCarInterface,
            lpModeManager->GetCurrentGameModeType(),
            lpModeManager->GetScoringSystem(),
            lfDelta);
    }
}


// ============================================================================
// â­â­ [gateui] ProcessGameEventsPropHitBringUp -- the extracted CASE-111 arm of
// GameStateModule::ProcessGameEvents @0x823A0A18 (the arm's verbatim asm is in the header).
//
// The console's dispatcher is a ~180-case jump table over the merged event queue; this tree
// extracts it one arm at a time (arms 78 and 94 are already extracted in BrnGameStateModule.cpp).
// The queue walk below is the dispatcher's own -- GetFirstEvent / GetNextEvent, switching on the
// returned type -- and the payload is read BY MEMBER through
// GameStateModuleIO::RecordPropHitEvent, whose committed layout
// { Vector3 mPosition@0x00; u16 muZoneId@0x10; u16 muPropId@0x12; bool mbHitBefore@0x14 } is
// exactly the three fields the console's three loads take.
//
// â“˜ Game EVENT ids are NOT shifted the way game ACTION ids are in this range (see the long
// correction note in BrnGameActions.h): E_EVENT_RECORD_PROP_HIT == 111 matches the X360 jump
// table's case 111 directly.
// ============================================================================
void GameStateModule::ProcessGameEventsPropHitBringUp(
        const CgsModule::VariableEventQueue<1536, 16>* lpGameEventQueue)
{
    if (lpGameEventQueue == 0)
    {
        return;
    }

    const CgsModule::Event* lpEvent = 0;
    s32                     liSize  = 0;
    s32                     liType  = lpGameEventQueue->GetFirstEvent(&lpEvent, &liSize);

    while (lpEvent != 0)
    {
        if (liType == GameStateModuleIO::E_EVENT_RECORD_PROP_HIT)
        {
            const GameStateModuleIO::RecordPropHitEvent* lpPropHit =
                reinterpret_cast<const GameStateModuleIO::RecordPropHitEvent*>(lpEvent);

            // [DIAG] NOT IN THE X360 BINARY -- the gateui ROUND-7 GameState-side RECEPTION rung,
            // the missing middle of the ladder. Rung order is now:
            //     world producer  `[prop-diag] BREAK`                (PropEntityModule_wQ_04.cpp)
            //     world producer  `Hit dont respawn prop:`           (ProcessContacts LEG 1)
            //     world bridge    `[UI-gate] bridged prop-hit`       (WorldBridgeEntityModulesToOutput.cpp)
            //  -> GAMESTATE       `[UI-gate] prop-hit event`         (HERE)
            //     latch           `[UI-gate] OnPropHit ... latch=`   (BrnStuntManager.cpp)
            //
            // WHY IT EARNS ITS PLACE: on the run-9 drive the two rungs either side of this one were
            // in perfect 1:1 lockstep (8 `bridged prop-hit` lines, 8 `OnPropHit` lines, every latch
            // SMASH), which is what proved the first-gate failure is upstream of GameState
            // entirely. â­ ROUND-8 CORRECTIONS: (i) this round-7 note used to add "each pair 2 log
            // lines apart" -- MEASURED, 3 of the 8 pairs are 2 lines apart and 5 are 3, so do NOT
            // use spacing as a matching heuristic; the 1:1 COUNT is the claim that holds.
            // (ii) this rung is instrumentation for a garbled-payload failure, not evidence about
            // defect A: the bridge->OnPropHit segment it sits in was already proven 1:1, and the
            // defect-A break is upstream of the world bridge entirely, in ProcessContacts' LEG-1
            // gate (see the "[prop-diag] LEG1 REJECT" rung in PropEntityModule_wQ2_03.cpp).
            // Without a rung HERE that lockstep has to be inferred
            // from line proximity across two modules; with it, a bridged-but-not-received event
            // (a carry-queue Clear race, a lock-bracket bug, an event-id mismatch) separates from a
            // never-produced one in a single grep. It reports the payload the console's case-111 arm
            // actually reads, so a garbled zone/prop id shows up here rather than as a mystery
            // `latch=none` further down.
            //
            // First-N guarded at the SAME budget as the OnPropHit rung it pairs with, so the two
            // stay aligned line-for-line (a multi-panel gate fires this once per panel).
            {
                static const bool sbDiag       = (getenv("BRN_PROP_DIAG") != 0);
                static s32        siEventLines = 0;
                const s32         KI_PROP_HIT_EVENT_DIAG_FIRST_N = 16;
                if (sbDiag && siEventLines < KI_PROP_HIT_EVENT_DIAG_FIRST_N &&
                    CgsDev::Log::gpDebugPrint != 0)
                {
                    ++siEventLines;
                    *CgsDev::Log::gpDebugPrint
                        << "[UI-gate] prop-hit event zone=" << lpPropHit->muZoneId
                        << " prop=" << lpPropHit->muPropId
                        << " hitBefore=" << (lpPropHit->mbHitBefore ? 1 : 0)
                        << " pos=(" << lpPropHit->mPosition.x
                        << "," << lpPropHit->mPosition.y
                        << "," << lpPropHit->mPosition.z << ")\n";
                }
            }

            mStuntManager.OnPropHit(lpPropHit->muZoneId, lpPropHit->muPropId, lpPropHit->mPosition);
        }

        // GetNextEvent takes the CURRENT event and writes the next one through its second
        // parameter; sequenced through a local so the two uses of lpEvent do not alias.
        const CgsModule::Event* lpCurrent = lpEvent;
        liType = lpGameEventQueue->GetNextEvent(lpCurrent, &lpEvent, &liSize);
    }
}

// ============================================================================
// â­ [H1 district wave 2026-08-25] ProcessGameEventsWorldRegionBringUp -- the extracted
// CASE-115 arm of GameStateModule::ProcessGameEvents @0x823A0A18 (banner + the console
// arm's three statements in the header). The queue walk is the dispatcher's own; the
// payload is read BY MEMBER through GameStateImageManagerBase.h's WorldRegionChangeEvent
// ({ meCounty @+0x00, meDistrict @+0x04 } -- the exact 8-byte pair the world's
// UpdateCurrentWorldRegion posts).
// ============================================================================
void GameStateModule::ProcessGameEventsWorldRegionBringUp(
        const CgsModule::VariableEventQueue<1536, 16>* lpGameEventQueue,
        GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    if (lpGameEventQueue == 0 || lpActionQueue == 0)
    {
        return;
    }

    const CgsModule::Event* lpEvent = 0;
    s32                     liSize  = 0;
    s32                     liType  = lpGameEventQueue->GetFirstEvent(&lpEvent, &liSize);

    while (lpEvent != 0)
    {
        if (liType == GameStateModuleIO::E_EVENT_CHANGE_WORLD_REGION)
        {
            const WorldRegionChangeEvent* lpChange =
                reinterpret_cast<const WorldRegionChangeEvent*>(lpEvent);

            // FLAG deferred: GameStateImageManagerBase::HandleWorldRegionChangeEvent
            // (this+185520 on the console) -- the image-manager sub-object is not a PC
            // member yet (its Prepare is the stage-24 deferral in BrnGameStateModule.cpp).
            // FLAG deferred: the console's `*(this+181512) = meDistrict` store -- the
            // member is un-homed; not fabricated.

            // The load-bearing hop: game ACTION 112 {county, district}, 8 bytes -- the
            // console's own AddEvent literal (@0x823A3470's arm). The bridge's case 112
            // turns it into GUI event 169 for the HUD district marker.
            lpActionQueue->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(lpChange), 112,
                static_cast<s32>(sizeof(WorldRegionChangeEvent)));

            // [DIAG] NOT IN THE X360 BINARY -- the district chain's GameState rung (the
            // [UI-gate] ladder idiom; region changes are rare, no first-N cap needed).
            if (CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[district] event 115 -> action 112 (county "
                    << static_cast<s32>(lpChange->meCounty) << " district "
                    << static_cast<s32>(lpChange->meDistrict) << ")\n";
            }
        }

        const CgsModule::Event* lpCurrent = lpEvent;
        liType = lpGameEventQueue->GetNextEvent(lpCurrent, &lpEvent, &liSize);
    }
}

// ============================================================================
// â­ [P1 sim-pause] PostWorldInput -- the free-function accessor BridgeGuiToGameState posts
// through (X360: returns the module's post-world input GameEventQueue). PC body: the CARRY
// QUEUE, the named reduction spelled out at the declaration (BrnGameStateModule.h).
// ============================================================================
namespace GameStateModuleIO
{
    CgsModule::VariableEventQueue<1536, 16>* PostWorldInput(GameStateModule* lpModule)
    {
        return &lpModule->mGameEventCarryQueue;
    }
}

// ============================================================================
// â­ [P1 sim-pause] ProcessGameEventsPauseBringUp -- the extracted pause-family arms of
// GameStateModule::ProcessGameEvents @0x823A0A18 (cases 33 / 35 / 36 / 93; the console
// bodies are quoted at the declaration). Same queue walk, same must-run-before-the-Clear
// position as the case-111/113/115 arms.
// ============================================================================
void GameStateModule::ProcessGameEventsPauseBringUp(
        const CgsModule::VariableEventQueue<1536, 16>* lpGameEventQueue,
        GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    if (lpGameEventQueue == 0 || lpActionQueue == 0)
    {
        return;
    }

    const CgsModule::Event* lpEvent = 0;
    s32                     liSize  = 0;
    s32                     liType  = lpGameEventQueue->GetFirstEvent(&lpEvent, &liSize);

    while (lpEvent != 0)
    {
        const u8* lpuPayload = reinterpret_cast<const u8*>(lpEvent);
        switch (liType)
        {
        case GameStateModuleIO::E_EVENT_PLAYER_PAUSE_STATE_CHANGED:   // 33
            if (lpuPayload[0] != 0)
                RequestPause(2, lpActionQueue, lpuPayload[1], lpuPayload[2]);
            else
                RequestUnpause(2, lpActionQueue);
            break;

        case GameStateModuleIO::E_EVENT_ENTER_REPLAY:                 // 35
            RequestPause(16, lpActionQueue, 0, 0);
            break;

        case GameStateModuleIO::E_EVENT_LEAVE_REPLAY:                 // 36
            RequestUnpause(16, lpActionQueue);
            break;

        case GameStateModuleIO::E_EVENT_CRASHNAV_STATE_CHANGED:       // 93
            // âš ï¸ inverted by design: payload 1 == the crash-nav map DEACTIVATED -> pause.
            // [DIAG] NOT IN THE X360 BINARY -- the pause spine's middle rung.
            if (CgsDev::Log::gpDebugPrint != 0)
                *CgsDev::Log::gpDebugPrint
                    << "[sim-pause] game event 93 payload " << static_cast<s32>(lpuPayload[0])
                    << (lpuPayload[0] ? " -> RequestPause(4)" : " -> RequestUnpause(4)") << "\n";
            if (lpuPayload[0] != 0)
                RequestPause(4, lpActionQueue, 0, 0);
            else
                RequestUnpause(4, lpActionQueue);
            break;

        default:
            break;
        }

        const CgsModule::Event* lpCurrent = lpEvent;
        liType = lpGameEventQueue->GetNextEvent(lpCurrent, &lpEvent, &liSize);
    }
}

// ============================================================================
// â­â­ [gateui] PreWorldUpdateStuntBringUp -- the three stunt-chain legs of the console's
// PreWorldUpdate @0x823A5328, IN THE CONSOLE'S OWN ORDER. The header carries the line-by-line map
// of the source function and both named reductions; the body annotates each leg again.
// ============================================================================
void GameStateModule::PreWorldUpdateStuntBringUp(
        f32 lfGameTimestep, bool lbIsAGameModeActive,
        const CgsSystem::TimerStatusInterface& lrTimerStatusInterface)
{
    if (mpOutputBuffer == 0)
    {
        return;
    }

    // The console holds the output buffer's write lock across this whole span of PreWorldUpdate:
    // TriggerQueryManager::UpdateTriggers publishes add/remove-trigger events onto the buffer's
    // trigger-management input interface, and StuntManager::Update AddEvents onto its game-action
    // queue. Same bracket here -- the idiom the other extracted PreWorldUpdate legs in
    // BrnGameStateModule.cpp already use.
    mpOutputBuffer->LockForWrite();
    GameStateModuleIO::GameActionQueue* lpActionQueue = mpOutputBuffer->GetGameActionQueue();
    CGS_ASSERT(lpActionQueue != 0, "lpActionQueue != NULL");   // BrnGameStateModule.cpp:1149
    mbIsUpdating = true;

    // ---- 0) DRIVE-THRU TICK (console #? -- PreWorldUpdate @0x823A5328 pseudocode line 220) ---
    // ⭐⭐⭐ [drive-thru wave 2026-08-27] DriveThruManager::Update @0x8239EEF0. This is the leg that
    // turns a latched drive-thru into its game action: HandleDriveThru (leg 2b below) only CACHES
    // the region type in meDriveThruCache; Update is what dispatches it through ProcessDriveThru
    // and posts action 100 / 97 / 98. It also ages the 46 activation timers.
    //
    // ⚠️ POSITION IS THE CONSOLE'S AND IT IS DELIBERATELY *BEFORE* THE TRIGGER FAN-OUT. The
    // console's own body order is
    //     220  DriveThruManager::Update            <-- HERE
    //     252  ProcessGameEvents
    //     305  CopyScoringDataToOutput
    //     310  TriggerQueryManager::PreWorldUpdate  <-- calls HandleDriveThru (leg 2b)
    //     332  StuntManager::Update
    // so a region entered on frame N is PROCESSED ON FRAME N+1 -- the same one-frame deferral the
    // game-event carry queue uses. Moving this call after leg 2b would "fix" a latency that is the
    // console's own and is what gives the presentation timer a frame to arm. Do not reorder.
    //
    // ARGUMENTS FROM THE ASM (0x823A56C0..0x823A5708), not from the pseudocode's numbering: the
    // f32 timestep goes in f1 and consumes the r6 slot, so the integer args are
    // r4,r5,[r6 skipped],r7,r8,r9,r10 then stack. Pinned by the last stack slot, which the console
    // loads from r31+0x456E8 == the module's mpVehicleList == this call's trailing argument.
    //
    // [FLAG PC bring-up] TWO arguments are PC derivations, named rather than hidden:
    //   * lbIsFreeburn -- the console reads it off the current game mode; no accessor for it
    //     exists in this tree. Derived as "no game mode is running", which is what freeburn IS on
    //     this build. It gates ONLY mbPlayerCanUseJunkyards, not the shop path.
    //   * lbIsInJunkyard / lbInviteInProgress -- both false on this offline build: nothing here
    //     runs the junkyard-occupancy latch or the invite manager.
    // The rest are real: IsOnlineGameMode(), IsShowtimeGameMode() and IsSimPaused() are committed
    // X360 reconstructions on this module, and the vehicle list is the console's own +0x456E8.
    //
    // THE TIMER INTERFACE IS THE CONSOLE'S OWN SLOT, reached through a cast rather than a fake.
    // The console passes OutputBuffer::GetTimerRequest(lpOutput); this tree's OutputBuffer models
    // that member as the opaque `OutputBufferTimerRequestInterface { u8 maOpaque[16]; }`
    // (BrnGameStateModuleIO.h:279) only because nothing had needed its shape yet. It IS a
    // CgsSystem::TimerRequestInterface: that type is two TimerRequests (each {u32 muFlags;
    // f32 mfMultiplier}) at +0 and +8, i.e. exactly 16 bytes, and the storage slot is exactly 16
    // bytes at +16420. So this is a re-type of the same object at the same address, not a
    // substitute -- the identical move, and the identical justification, as AsActionQueue() in
    // BrnDriveThruManager.cpp. DELETE-WHEN BrnGameStateModuleIO.h declares the slot's real type.
    mDriveThruManager.Update(
        lpActionQueue,
        reinterpret_cast<CgsSystem::TimerRequestInterface*>(
            mpOutputBuffer->GetTimerRequestInterface()),
        lfGameTimestep,
        &mLastActiveRaceCarInterface,
        IsOnlineGameMode(),
        /*lbIsFreeburn*/ !lbIsAGameModeActive,
        IsShowtimeGameMode(),
        IsSimPaused(true, false),
        /*lbIsInJunkyard*/ false,
        /*lbInviteInProgress*/ false,
        GetVehicleList());

    // ---- 1) the merged queue -> ProcessGameEvents (case 111 LATCHES) ------------------------
    // X360 lines 239-245 Construct a LOCAL <1536,16> queue and Append THREE sources into it -- the
    // carry queue (+248384), the PreWorldInputBuffer's queue, and the InviteManager's (+2032) --
    // then Clear the carry queue; line 252 hands that local queue to ProcessGameEvents.
    // REDUCED to the carry queue alone: the other two sources have no producer on this build
    // (nothing creates a PreWorldInputBuffer, and the InviteManager's queue is never written), so
    // the local queue would be a byte-for-byte copy of the carry queue. The Clear IS the
    // console's, and it is what makes the queue a strict one-frame buffer.
    ProcessGameEventsPropHitBringUp(&mGameEventCarryQueue);
    // â­ [tut-ticker] the dispatcher's CASE-113 arm, over the same merged queue in the same
    // walk position (the console's ProcessGameEvents handles every case in one pass; this
    // tree extracts one arm per function -- see the arm's banner in BrnGameStateModule.cpp).
    // MUST run before the Clear below, for the same reason the prop-hit arm does.
    ProcessGameEventsTrainingRequestBringUp(&mGameEventCarryQueue);
    // â­ [H1 district wave] the dispatcher's CASE-115 arm (the HUD district marker's feed),
    // same walk, same must-run-before-the-Clear constraint; it posts onto the action queue
    // this function already holds the write lock for.
    ProcessGameEventsWorldRegionBringUp(&mGameEventCarryQueue, lpActionQueue);
    // â­ [P1 sim-pause] the dispatcher's pause-family arms (cases 33/35/36/93), same walk,
    // same must-run-before-the-Clear constraint; RequestPause/RequestUnpause post actions
    // 86/87/88 onto the action queue this function already holds the write lock for --
    // CheckGameActions (BrnGameModule, the console's DoUpdate_GameStatePreWorld tail) reads
    // them back this same sub-step and stops/starts the sim timer.
    ProcessGameEventsPauseBringUp(&mGameEventCarryQueue, lpActionQueue);
    // â­â­ [D4 stuntrace WAVE D] the dispatcher's CASE-20 arm (E_EVENT_PLAYER_ACCEPTED_MODE ->
    // ModeManager::StartGameMode) and the INTRO/RESULTS exit arms (cases 24/25/26/27). Same walk,
    // same must-run-before-the-Clear constraint as every arm above; the case-20 arm needs the
    // OutputBuffer because ModeManager::StartGameMode takes it (console r27).
    ProcessGameEventsStartGameModeBringUp(&mGameEventCarryQueue, mpOutputBuffer);
    ProcessGameEventsModeIntroBringUp(&mGameEventCarryQueue);
    mGameEventCarryQueue.Clear();

    // ---- 1b) THE MODE MANAGER'S PRE-WORLD TICK (console #86) ---------------------------------
    // â­â­â­ [D4 stuntrace WAVE D] X360 PreWorldUpdate @0x823A5328 reaches ModeManager through ONE
    // hop, and this is that hop de-inlined:
    //     0x823A5A9C  bl GameStateModule::EmmPreWorldUpdate      (#86)
    //       @0x8238EF50, whose own body:
    //         v22 = PreWorldInputBuffer::GetTimerStatusInterface(a2);
    //         *(a1 + 208328 .. +208372) = v22[0..11]                 ; the 48-byte timer copy
    //         if (IsSimPaused(a1,1,0))  ModeManager::PausedUpdate(a1 + 4128, a3);
    //         else                      ModeManager::PreWorldUpdate(a1 + 4128, a3 /*out*/,
    //                                       a2 /*in*/, a1 + 208328 /*timer*/,
    //                                       GetPlayerActiveRaceCarIndex(a1),
    //                                       GetPlayerGlobalRaceCarIndex(a1),
    //                                       a5 /*isOnline byte, gsm+0x3C041*/, <action queue>, ...);
    // POSITION IS THE CONSOLE'S: after ProcessGameEvents (#68), before TriggerQueryManager::
    // PreWorldUpdate (#93). Do not move it below the trigger legs.
    //
    // âš  gsm+4128 (0x1020) IS mModeManager, and it is reached BY NAME through GetModeManager()
    // here -- never as an offset. (The stale campaign note "ModeManager is embedded at gsm+46640"
    // is wrong; +46640 is mTrainingManager.)
    //
    // â›” THE READ LOCK IS LOAD-BEARING. ModeManager::PreWorldUpdate calls
    // lpPreWorldInputBuffer->GetPlayerStatusInterface() and ->GetNetworkPlayerResultsInterface(),
    // and BOTH are the read-lock halves ("Not locked for reading", BrnGameStateModuleIO.h:147/149).
    // The console holds IOBuffer::LockForRead over the whole span (@0x823A5328 `bl` #16). Without
    // the bracket those two accessors assert every frame the moment a mode starts. They are the
    // ONLY buffer derefs in the function and both sit inside `if (mpCurrentGameMode != NULL)`, so
    // in free-burn the buffer is never touched -- which is why this leg is safe to run always.
    //
    // [FLAG PC bring-up] FOUR named deviations, none of them silent:
    //   (a) THE TIMER INTERFACE comes from the caller (BrnGameModule::mTimerStatusInterface, filled
    //       every sub-step by the console's own TimerStatusInterface::StoreTimers at
    //       BrnGameModule.cpp:1411) instead of from the 48-byte copy out of the PreWorldInputBuffer.
    //       Nothing on PC fills that buffer's timer block, so the console route would hand
    //       ModeManager an all-zero interface and every mode clock, the countdown and the mode
    //       timer would stand still. Same data, one copy earlier. See the header for the full note.
    //   (b) lpGlobalRaceCarOutput is 0. The console's is the world's
    //       RCEntityGlobalRaceCarOutputInterface; BrnWorldIO::UpdateOutputBuffer exposes only the
    //       ACTIVE one (BrnWorldModuleIO.h typedefs the global flavour but hands out no getter), so
    //       there is nothing to pass. MEASURED SAFE ON THIS PATH: ModeManager::PreWorldUpdate only
    //       forwards it, UpdateCurrentMode only forwards it, and GameMode::PreWorldUpdate
    //       (BrnGameMode.cpp:274) does `(void)lpGlobalRaceCars;`. Race-position work is the
    //       consumer that will need it. DELETE-WHEN the world output buffer grows the getter.
    //   (c) lbPaused is false. The console's tenth argument is a stacked byte the IDA export
    //       renders as register residue (v50..v64), and EmmPreWorldUpdate reaches PreWorldUpdate
    //       only down its NOT-sim-paused arm, so "paused" here is not the sim pause. Not guessed
    //       at a value it might have had; passed the arm's own falsity.
    //   (d) [X] ModeManager::PausedUpdate (the console's sim-paused arm) is PARKED: it has no
    //       declaration and no body anywhere in the tree. While the sim is paused this leg is
    //       skipped entirely, which is what the console does with PreWorldUpdate on that arm --
    //       what is lost is PausedUpdate's own work, not this one's.
    if (mpPreWorldInputBuffer != 0 && !IsSimPaused(true, false))
    {
        mpPreWorldInputBuffer->LockForRead();
        mModeManager.PreWorldUpdate(
            mpOutputBuffer,
            mpPreWorldInputBuffer,
            lrTimerStatusInterface,
            GetPlayerActiveRaceCarIndex(),
            static_cast<::EGlobalRaceCarIndex>(GetPlayerGlobalRaceCarIndex()),
            IsOnlineGameMode(),
            lpActionQueue,
            /*lpGlobalRaceCarOutput -- FLAG (b)*/ 0,
            &mLastActiveRaceCarInterface,
            /*lbPaused -- FLAG (c)*/ false);
        mpPreWorldInputBuffer->UnlockForRead();

        // â­ [D4] THE TEMPORARY OFFLINE-INTRO SELF-TRIGGER (see the body of
        // ProcessGameEventsModeIntroBringUp for why it is needed and when it dies).
        HarnessOfflineIntroSelfTriggerBringUp(lfGameTimestep);
    }

    // ---- 1c) THE SECOND LEG OF THE SAME HOP (console #86, EmmPreWorldUpdate's own tail) -------
    // â­â­â­ [bounce wave] EmmPreWorldUpdate @0x8238EF50 does not stop at ModeManager. Its `bl`
    // stream continues:
    //     0x8238F168  bl ModeManager::PreWorldUpdate            <- leg 1b above
    //     0x8238F170  bl PerfMonCpu::StopMonitor
    //     0x8238F198  bl PerfMonCpu::StartMonitor
    //     0x8238F1A0  bl GameStateModuleIO::PreWorl<dInputBuffer::Get...>
    //     0x8238F1B0  bl GameStateModule::UpdateRoadRulesManager    <- THIS LEG
    // and the console guards it with `if (!IsSimPaused)` -- the SAME arm test leg 1b sits on,
    // which is why it is staged immediately after it and inside nothing new.
    //
    // âš ï¸ IT IS ITS OWN `if`, NOT AN `else`. In the console the ModeManager call sits inside
    // `if (IsSimPaused) PausedUpdate else PreWorldUpdate`, and THEN a separate
    // `if (!IsSimPaused) { UpdateRoadRulesManager }` follows. Both arms are the not-paused arm,
    // so the observable order and gating are identical either way; kept as a separate statement
    // so the shape matches the binary rather than reading as an else-branch that is not there.
    //
    // â­ WHY THIS LEG EXISTS AT ALL: its action-42 post is the ONLY producer of impact time in
    // the entire image, and without it VehiclePhysics::UpdateCrashing's aftertouch gate never
    // opens, so RaceCarPhysics::UpdateShowtimePhysics -- the whole P6 bounce chain -- never runs.
    // The full derivation, the four arms deliberately NOT landed, and the method that found the
    // post are all in GameStateModule_RoadRules.cpp.
    //
    // â›” NOT staged inside the 1b block above: 1b additionally requires mpPreWorldInputBuffer to
    // be non-null (it passes the buffer to ModeManager), and this leg does not touch that buffer
    // at all. Nesting it there would add a condition the console does not have -- and on a build
    // where nothing constructs a PreWorldInputBuffer that condition is exactly the kind of
    // invented gate that would silently keep the chain dead [[invented-arms-and-the-c4715-ratchet]].
    if (!IsSimPaused(true, false))
    {
        UpdateRoadRulesManagerImpactTimeBringUp(lpActionQueue);
    }

    // (merge 2026-08-27: both waves added a leg at this seam the same day -- the bounce wave's
    // 1c above is EmmPreWorldUpdate's own tail; the scoring publish below runs AFTER
    // EmmPreWorldUpdate returns, per its console position. Both kept, console order.)

    // ---- 1d) PUBLISH THE SCORING SNAPSHOT (console #(RumbleManager::UpdatePauseState + 1)) ----
    // â­â­â­ [A9 scoring-feed wave 2026-08-27] GameStateModule::CopyScoringDataToOutput @0x8236CDC0.
    //
    // POSITION IS THE CONSOLE'S, and it is exact. GameStateModule::PreWorldUpdate @0x823A5328 --
    // this function's source and CopyScoringDataToOutput's SOLE xref-to -- runs it here:
    //     bl GameStateModule::EmmPreWorldUpdate            (#86; the ModeManager tick above)
    //     bl RumbleManager::UpdatePauseState
    //     bl CgsDev::PerfMonCpu::StartMonitor(*(this+292348))
    //     bl GameStateModule::CopyScoringDataToOutput(this, lpOutput)     <-- THIS CALL
    //     bl CgsDev::PerfMonCpu::StopMonitor(*(this+292348))
    //     if (a6 & 8) { bl TriggerQueryManager::PreWorldUpdate ... }      <-- leg 2 below
    // i.e. AFTER the mode tick and BEFORE the trigger legs. Do not move it: ModeManager::
    // PreWorldUpdate is what advances mStartTime/mEndTime and the per-car score records this
    // publishes, so running the copy first would publish a one-frame-stale snapshot.
    //
    // â“˜ UNCONDITIONAL, deliberately. The console's ModeManager tick sits inside
    // EmmPreWorldUpdate's not-sim-paused arm (and here inside the same guard, plus the PC-only
    // `mpPreWorldInputBuffer != 0`), but this call is OUTSIDE it -- while the sim is paused the
    // console still republishes the last scoring state every frame, which is what keeps the HUD
    // clock showing its frozen value instead of collapsing to zero.
    //
    // â“˜ The write lock this function already holds is the console's own
    // (`IOBuffer::LockForWrite(lpOutput)` at PreWorldUpdate's top) and it is required: the two
    // scoring-interface accessors CopyScoringDataToOutput goes through are write-lock asserted.
    // The mbIsUpdating bracket is required too -- GetPlayerActiveRaceCarIndex() and
    // IsOnlineGameMode() both assert it.
    //
    // [FLAG PC bring-up] the TimerStatusInterface argument is the ONE deviation, the same one this
    // function's own ModeManager call carries and for the same measured reason (nothing on PC
    // fills the module's copy of the PreWorldInputBuffer timer block at gsm+208328, which is where
    // the console reads its "now" from). Fully written up at the declaration in
    // BrnGameStateModule.h. DELETE-WHEN DoUpdate_GameStatePreWorld stages a real
    // PreWorldInputBuffer whose timer block is filled.
    CopyScoringDataToOutput(mpOutputBuffer, lrTimerStatusInterface);

    // ---- 2) TriggerQueryManager: ARM the trigger set -----------------------------------------
    // X360 line 310 calls TriggerQueryManager::PreWorldUpdate @0x8239F5C8, whose FIRST statement
    // is `UpdateTriggers(this, lpOutput, lpActiveRaceCarInterface)`. That is the ONLY writer of
    // maActiveTriggers anywhere in the image -- the array StuntManager::OnPropHit walks -- so it
    // is the leg this wave needs. Its siblings inside that function (SubmitTriggerQueries, the
    // per-player-trigger fan-out that posts action 109 and calls ProcessPlayerTriggers, the
    // killzone-action drain) walk Array<u16,32> members this tree's TriggerQueryManager slice does
    // not model; parked, not faked.
    // â“˜ IT RUNS AFTER ProcessGameEvents, so OnPropHit above walked the PREVIOUS frame's armed
    // set. That is the console's own order and it is deliberate -- do not "fix" it.
    mTriggerQueryManager.UpdateTriggers(mpOutputBuffer, &mLastActiveRaceCarInterface);

    // ---- 2b) TriggerQueryManager: FAN THE PLAYER'S TRIGGER HITS OUT --------------------------
    // [bugwave 2026-08-23] THE SUPER-JUMP ROOT-CAUSE FIX. The park note directly above used to
    // stop at UpdateTriggers and record "the per-player-trigger fan-out that posts action 109 and
    // calls ProcessPlayerTriggers ... parked, not faked". That park is what made super jumps
    // uncountable: ProcessPlayerTriggers is the ONLY caller of StuntManager::LatchJumpElement,
    // which is the ONLY writer of mpLastJumpElement, which is the gate on StuntManager::
    // UpdateJumps -- so with the park in place the jump state machine never ran, no game action
    // 56 (OnJumpStart -> the jump camera) was ever posted, and ProcessStuntElement was never
    // reached with lbIsJump == true, so the super-jump tally never moved.
    // The leg is the console's own (X360 PreWorldUpdate @0x8239F5C8, 0x8239F714..0x8239F83C);
    // see BrnTriggerQueryManager.cpp for the leg-by-leg map and for the ONE documented PC
    // bring-up stand-in it carries (the producer of maLastPlayerTriggers, whose console producer
    // -- the world TriggerEntityModule line-test chain -- is inert on this build).
    // â“˜ ORDER IS THE CONSOLE'S: the fan-out runs AFTER UpdateTriggers (it reads the set
    // UpdateTriggers just armed) and BEFORE StuntManager::Update (which consumes the latch it
    // writes). Both halves of that sandwich are load-bearing -- do not reorder.
    // ⭐ [drive-thru wave 2026-08-27] THE DriveThruManager ARGUMENT IS REAL NOW. The FLAG that
    // stood here ("the argument is NULL ... DELETE-WHEN BrnDriveThruManager.cpp compiles and the
    // sub-object is modelled") is paid on both counts: the TU compiles and GameStateModule embeds
    // mDriveThruManager at the console's this+44240 position. The console passes exactly this
    // sub-object (PreWorldUpdate @0x823A5328 -> `a1 + 44240`).
    mTriggerQueryManager.PreWorldUpdatePlayerTriggersBringUp(
        mpOutputBuffer, &mLastActiveRaceCarInterface, &mStuntManager,
        &mDriveThruManager, GetVehicleList());

    // [DIAG] NOT IN THE X360 BINARY. Rung 0 of the `[UI-gate]` ladder, one-shot on the first frame
    // the armed set is non-empty: how many armed regions are SMASH (generic-region sub-type 8) and
    // BILLBOARD (sub-type 12). This is the line that separates "the prop was outside every smash
    // region" from "the trigger pump never ran" -- the wave's known blocker. Same logger and same
    // env guard (BRN_PROP_DIAG) as the "[prop-diag] BREAK" rung this ladder hangs off.
    //
    // â“˜ ROUND-7 NOTE -- READ THIS BEFORE DRAWING A CONCLUSION FROM THIS LINE. It is a ONE-SHOT and
    // it fires at the FIRST non-empty set, which on a junk-yard start is the junk-yard interior:
    // run 9 printed `armed smash=0 billboard=0 of=3` (BrnGame.log:863) and that line says NOTHING
    // about what was armed later. The per-rebuild timeline that does answer that question lives in
    // BrnTriggerQueryManager.cpp :: UpdateTriggers (`[UI-gate] trig rebuild #<n> pos=(...)`), which
    // reports every rebuild of the set with the position it was keyed on. Use that rung, not this
    // one, to decide whether a given gate's region was armed when the gate broke.
    {
        static bool       sbArmedLogged = false;
        static const bool sbDiag        = (getenv("BRN_PROP_DIAG") != 0);
        const u32         luArmedCount  = mTriggerQueryManager.GetActiveTriggerCount();
        if (sbDiag && !sbArmedLogged && luArmedCount > 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            sbArmedLogged = true;
            const BrnTrigger::TriggerData* lpTriggerData = mTriggerQueryManager.GetTriggerData();
            s32 liSmash     = 0;
            s32 liBillboard = 0;
            if (lpTriggerData != 0)
            {
                for (u32 lu = 0; lu < luArmedCount; ++lu)
                {
                    const BrnTrigger::TriggerRegion* lpRegion =
                        lpTriggerData->GetRegion(mTriggerQueryManager.GetActiveTrigger(lu));
                    if (lpRegion->GetType() != BrnTrigger::TriggerRegion::E_TYPE_GENERIC_REGION)
                    {
                        continue;
                    }
                    const BrnTrigger::GenericRegion* lpGeneric =
                        static_cast<const BrnTrigger::GenericRegion*>(lpRegion);
                    if (lpGeneric->GetType() == BrnTrigger::GenericRegion::E_TYPE_SMASH)
                    {
                        ++liSmash;
                    }
                    else if (lpGeneric->GetType() == BrnTrigger::GenericRegion::E_TYPE_OVERDRIVE_BOOST)
                    {
                        ++liBillboard;
                    }
                }
            }
            *CgsDev::Log::gpDebugPrint
                << "[UI-gate] armed smash=" << liSmash
                << " billboard=" << liBillboard
                << " of=" << luArmedCount << "\n";
        }
    }

    // ---- 2c) JUNCTION DETECTION + THE START ARM (console #96 and #98) ------------------------
    // â­â­â­ [D4 stuntrace WAVE D] The two D3-owned functions, staged in the console's own body
    // order. From PreWorldUpdate @0x823A5328's `bl` stream:
    //     #93   TriggerQueryManager::PreWorldUpdate            <- the two legs immediately above
    //     #95   ProgressionManager::PreWorldUpdate             <- [X] NOT STAGED (see below)
    //     #96   GameStateModule::CheckIfPlayerIsAtJunctionWithAnEvent   @0x82390418
    //     #97   GameStateModule::SendSetUpAllDriveThrusMessage  <- [X] NOT STAGED (see below)
    //     #98   GameStateModule::DetectModeStarts               @0x8239A428
    //     #103  StuntManager::Update                            <- the leg immediately below
    // Both take (r3 = this, r4 = r30, r5 = r29), and 0x823A5B2C..0x823A5B3C pins that pair as
    // (lpPreWorldInputBuffer, lpOutputBuffer) -- the identical r30/r29 the TriggerQueryManager call
    // four instructions earlier takes as its "in, out".
    //
    // âš  THE ORDER IS LOAD-BEARING IN BOTH DIRECTIONS.
    //   * #96 runs AFTER #93 because it reads TriggerQueryManager::mbPlayerInTrafficLightRegion /
    //     mPlayerCurrentTrafficLightId, and GetPlayerCurrentTrafficLightId() asserts
    //     IsPlayerInTrafficLightRegion() -- so it must see THIS frame's light-region state.
    //   * #98 runs AFTER #96 because ShouldStartSnapRaceMode gates on the junction cache #96 fills;
    //     with the two swapped the first frame of a hold would test last frame's junction.
    //   * both run BEFORE #103 (StuntManager::Update), which is the console's own placement.
    //
    // [X] #95 ProgressionManager::PreWorldUpdate NOT STAGED: its console argument list
    // (0x823A5B84..0x823A5B9C) carries two floats plus four interface pointers this build does not
    // stage, and nothing in the junction/start chain reads what it writes. Named, not forgotten.
    // [X] #97 SendSetUpAllDriveThrusMessage NOT STAGED: the console gates it on a one-shot byte at
    // gsm+0x2C988 that it clears in the same breath (0x823A5BB4..0x823A5BD4), and the tree's
    // DriveThruManager is itself parked (see PreWorldUpdatePlayerTriggersBringUp's own FLAG).
    //
    // â›” CROSS-LANE: THE BODIES ARE AGENT D3'S. This lane owns the CALL SITES and the two
    // declarations in BrnGameStateModule.h (see the [D4 PUMP SEAM] block there). Until D3's landing
    // is consolidated these two are unresolved externals at LINK time -- the per-TU compile gate
    // passes, the exe does not link. That is the parallel-wave contract, stated rather than hidden.
    //
    // Read-locked for the same reason leg 1b is: whatever D3's bodies read off the buffer, they
    // read through the read-lock halves of its accessors.
    if (mpPreWorldInputBuffer != 0)
    {
        mpPreWorldInputBuffer->LockForRead();
        CheckIfPlayerIsAtJunctionWithAnEvent(mpPreWorldInputBuffer, mpOutputBuffer);
        // [!] D3's DetectModeStarts carries a THIRD argument the console does not: the
        // module's cached game timestep at +292284, whose producer (PreWorldUpdate's own
        // timer leg) is not reconstructed -- see its declaration. Fed the same game-timer
        // product every other leg in this function uses.
        DetectModeStarts(mpPreWorldInputBuffer, mpOutputBuffer, lfGameTimestep);

        // â­ [D4] THE HARNESS START INJECTION (NOT IN THE X360 BINARY). Runs immediately after
        // DetectModeStarts, inside the same read-lock bracket, because it stands in for exactly
        // what DetectModeStarts' gesture gate decides. Env-gated off; see the body.
        HarnessInjectEventStartBringUp(mpOutputBuffer);

        // â­â­â­ [showtime S7b-a, 2026-08-27] THE SHOWTIME START INJECTION (NOT IN THE X360 BINARY).
        // Same bracket, same reason, same discipline as the line above -- but for the SECOND
        // gesture DetectModeStarts carries: both bumpers (ControllerInput +0x42
        // mbCrashModePressed), whose whole `else` arm is a named PARK in GameStateModule_gSR_00.cpp.
        // Its TRIGGER is that real byte; what it bypasses is ShouldStartShowtimeMode @0x82356B18's
        // hold/speed/facing gate stack. Env-gated off (BRN_START_SHOWTIME); body in
        // GameStateModule_Showtime.cpp.
        // â›” DELETE-WHEN the else arm lands -- this call and that function go together.
        HarnessInjectShowtimeBringUp(mpPreWorldInputBuffer, mpOutputBuffer);

        mpPreWorldInputBuffer->UnlockForRead();
    }

    // ---- 3) StuntManager::Update: CONSUME the latch ------------------------------------------
    // X360 line 332:
    //     v51 = GameStateModuleIO::OutputBuffer::GetGameActionQueue(a5);
    //     StuntManager::Update(a1 + 183952, v51, a1 + 235488, <f1 == the game timestep>, v50);
    // The timestep rides f1 and consumes NO GPR slot (the PPC float-arg rule), which is why the
    // pseudocode renders only four arguments. The interface argument is the module's own cached
    // snapshot -- which PostWorldUpdateStuntBringUp above is what keeps alive.
    mStuntManager.Update(lpActionQueue, &mLastActiveRaceCarInterface,
                         lfGameTimestep, lbIsAGameModeActive);

    // [DIAG BRN_QUEUE_WATERMARK] NOT IN THE X360 BINARY -- THE 13312 GAME-ACTION QUEUE WATERMARK.
    // âš âš  WHY IT EARNS ITS PLACE, and why a plain assert does not cover it: this queue is
    // GameStateModuleIO::GameActionQueue == CgsModule::VariableEventQueue<13312,16>, and its
    // AddEvent (CgsVariableEventQueue.h:427) DOES NOT RETURN after the overflow assert -- it fires
    // "Queue overflow." at CgsVariableEventQueue.h:469 and then MEMCPYS ANYWAY, past macData[13312],
    // into whatever follows the queue in the OutputBuffer. On a build with asserts routed to the
    // log rather than to a break, a single overflow silently corrupts the buffer.
    // The exposure is NEW this wave and it is large: ModeManager::PrepareForMode posts action 23 at
    // 2272 BYTES (BrnModeManager.h's own size table), and action 24 at 48 on top of it -- three
    // mode starts in one sub-step is over half the buffer. Nothing before wave D could post 2 KB in
    // one action.
    // Reports the running PEAK of GetSizeInBytes() (== miBufferWritePos - miFirstEventOffset, the
    // bytes actually written this sub-step) so a near-miss shows up BEFORE the overflow does, and
    // only when the peak moves, so a quiet drive prints a handful of lines and a mode start prints
    // one. Off unless BRN_QUEUE_WATERMARK is set.
    {
        static const bool sbWatermarkDiag = (getenv("BRN_QUEUE_WATERMARK") != 0);
        static s32        siPeakBytes     = 0;
        if (sbWatermarkDiag && CgsDev::Log::gpDebugPrint != 0)
        {
            const s32 liBytes = lpActionQueue->GetSizeInBytes();
            if (liBytes > siPeakBytes)
            {
                siPeakBytes = liBytes;
                *CgsDev::Log::gpDebugPrint
                    << "[queue-wm] game-action queue peak " << siPeakBytes
                    << " / 13312 bytes, n=" << lpActionQueue->GetLength() << "\n";
            }
        }
    }

    mbIsUpdating = false;
    mpOutputBuffer->UnlockForWrite();
}


// ============================================================================
// [D4 stuntrace WAVE D] ProcessGameEventsStartGameModeBringUp -- the extracted CASE-20 arm
// of GameStateModule::ProcessGameEvents @0x823A0A18 (asm 0x823A2680..0x823A2718; source
// BrnGameStateModule.cpp:2456 per the DWARF unity dump, which lists exactly this callee set:
// StartGameModeParams ctor, RCEntityActiveRaceCarOutputInterface::GetPlayerPosition,
// StartGameModeParams::Construct, LightTriggerId::SetInvalid, Array<CheckpointData,16>::Construct,
// StartGameModeParams::AddCheckpoint, and a four-shift StrStream debug print).
//
// The full asm is quoted at the declaration in BrnGameStateModule.h. Reproduced here:
//   * the !mpCurrentGameMode gate (console `lwz r11, 0x1DB8(r31)` == gsm+7608 ==
//     mModeManager.mpCurrentGameMode -- ModeManager sits at gsm+4128 and mpCurrentGameMode at
//     ModeManager+3480, and 4128+3480 == 7608),
//   * the local StartGameModeParams and its Construct(mode, playerPosition, mechanism),
//   * the AddCheckpoint loop over the event's landmark/section pairs,
//   * ModeManager::StartGameMode(&mModeManager, lpOutputBuffer, &params).
//
// [!][!] THE HEADLINE CORRECTION, PROVEN FROM THE ASM (restated here because it changes what this
// arm is FOR): `li r4, 0` / `li r5, 0` at 0x823A26C0 / 0x823A26BC are Construct's two GPR
// arguments, and Construct @0x8231C1F8 stores r4 to +0x2D0 (meGameModeType) and r5 to +0x310
// (meStartMechanism) -- verified in that function's own store cluster, not inferred from the
// pseudocode. So case 20 unconditionally starts E_MODE_OFFLINE_RACE (0) with
// E_GAMEMODESTARTMECHANISM_DEFAULT (0), and it reads NEITHER the event's meModeType (+0x48) NOR
// its mRaceId (+0x00). CASE 20 IS NOT THE STUNT-RACE START. The offline stunt start is
// GameStateModule::StartModeAtLights @0x82396CF8 (mechanism 2, runtime mode resolved through
// ProgressionManager::GetEvent) -- agent D3's function, staged at console position #98 above.
// Do not "fix" the hard-coded zeros; they are the binary's.
// ============================================================================
namespace
{
    // ------------------------------------------------------------------------
    // The CASE-20 payload, as a READ-ONLY VIEW pinned to the offsets the console arm actually
    // loads. It is deliberately NOT promoted into BrnGameEvents.h yet, because the asm and the
    // DWARF disagree and this lane will not mint a layout it cannot prove:
    //
    //   ASM (0x823A26D0..0x823A26FC), unambiguous -- r30 starts at event+8 and steps by 2:
    //       lbz r11, 0x4C(r25)   -> muNumLandmarks   @ +0x4C  (u8)
    //       lhz r4,  0(r30)      -> section id [i]   @ +0x08 + 2i  (u16)
    //       lhz r5,  0x24(r30)   -> landmark idx [i] @ +0x2C + 2i  (u16)
    //
    //   DWARF (BrnGameEvents.h:1279-1286) declares, in this order: CgsID mRaceId;
    //       uint16_t mauLandmarkSectionIds[16]; LandmarkIndex maLandmarkIndices[16];
    //       EGameModeType meModeType; uint8_t muNumLandmarks;
    //   which would put maLandmarkIndices at +0x28, not +0x2C. The 4-byte discrepancy is real and
    //   unresolved (the only self-consistent reading of the asm is that meModeType sits at +0x28,
    //   i.e. BETWEEN the two arrays -- which the DWARF's source-line order contradicts).
    //
    // [!] FLAG: only the three offsets the console arm READS are claimed below. meModeType and
    // mRaceId are deliberately absent -- the arm does not touch them and this view will not guess
    // where they live. When the discrepancy is settled, promote a real
    // GameStateModuleIO::PlayerAcceptedModeEvent into BrnGameEvents.h and delete this view.
    // ------------------------------------------------------------------------
    struct D4_PlayerAcceptedModeEventView
    {
        static const s32 KI_OFFSET_SECTION_IDS      = 0x08;   // lhz 0(r30),   r30 = event + 8
        static const s32 KI_OFFSET_LANDMARK_INDICES = 0x2C;   // lhz 0x24(r30)
        static const s32 KI_OFFSET_NUM_LANDMARKS    = 0x4C;   // lbz 0x4C(r25)
        static const s32 KI_MAX_LANDMARKS           = 16;     // Array<CheckpointData,16u>

        static u8 GetNumLandmarks(const CgsModule::Event* lpEvent)
        {
            return reinterpret_cast<const u8*>(lpEvent)[KI_OFFSET_NUM_LANDMARKS];
        }
        static u16 GetSectionId(const CgsModule::Event* lpEvent, s32 liIndex)
        {
            const u8* lpuBytes = reinterpret_cast<const u8*>(lpEvent);
            u16 luValue = 0;
            memcpy(&luValue, lpuBytes + KI_OFFSET_SECTION_IDS + 2 * liIndex, sizeof(u16));
            return luValue;
        }
        static u16 GetLandmarkIndex(const CgsModule::Event* lpEvent, s32 liIndex)
        {
            const u8* lpuBytes = reinterpret_cast<const u8*>(lpEvent);
            u16 luValue = 0;
            memcpy(&luValue, lpuBytes + KI_OFFSET_LANDMARK_INDICES + 2 * liIndex, sizeof(u16));
            return luValue;
        }
    };
}

void GameStateModule::ProcessGameEventsStartGameModeBringUp(
        const CgsModule::VariableEventQueue<1536, 16>* lpGameEventQueue,
        GameStateModuleIO::OutputBuffer*               lpOutputBuffer)
{
    if (lpGameEventQueue == 0 || lpOutputBuffer == 0)
    {
        return;
    }

    const CgsModule::Event* lpEvent = 0;
    s32                     liSize  = 0;
    s32                     liType  = lpGameEventQueue->GetFirstEvent(&lpEvent, &liSize);

    while (lpEvent != 0)
    {
        // Game EVENT ids are NOT subject to the +5 game-ACTION shift (see BrnGameActions.h's
        // correction note): X360 jump-table case 20 == DWARF E_EVENT_PLAYER_ACCEPTED_MODE == 20.
        // Pinned by its neighbours in the same table, each of which names its own callee:
        // 21 -> MarkedManLoaded, 23 -> FinishedSplashScreen, 24 -> FinishedMapPan,
        // 25 -> FinishOfflineModeIntro, 26 -> ResultsAccept, 27 -> UserCancelCurrentMode.
        if (liType == 20)
        {
            // The console's own gate: only when nothing is running (0x823A2680..0x823A2688).
            // (i) INSTRUMENTED ON PURPOSE: a SECOND start attempt must read as "a mode is already
            // running", not as "the start failed". Both arms log.
            const GameMode* lpCurrentGameMode = mModeManager.GetCurrentGameMode();

            if (lpCurrentGameMode != 0)
            {
                // [DIAG] NOT IN THE X360 BINARY.
                if (CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[start] event 20 IGNORED -- mpCurrentGameMode is live (mode type "
                        << static_cast<s32>(mModeManager.GetCurrentGameModeType())
                        << ", state " << lpCurrentGameMode->GetCurrentState()
                        << "); the console's case-20 gate rejects it too\n";
                }
            }
            else
            {
                // The console's local. Its embedded Array<CheckpointData,16u> is default-
                // constructed by the declaration (the pseudocode's
                // _vector_constructor_iterator_(v569, 44, 16, CheckpointData::CheckpointData) and
                // the -1 sentinel store at +704 are that construction, inlined) and then
                // Construct()ed to empty-but-usable inside StartGameModeParams::Construct.
                StartGameModeParams lStartGameModeParams;

                // 0x823A26AC..0x823A26B4: sub_823102F0(&tmp, gsm + 235488) is
                // RCEntityActiveRaceCarOutputInterface::GetPlayerPosition -- it asserts
                // mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT and
                // IsPlayerCarActive() and returns maRaceCars[playerIndex] + 1360. Reached here
                // through the module's own cached snapshot, which is the same object the console
                // passes (gsm+235488 == mLastActiveRaceCarInterface).
                const Vector3 lPlayerPosition = mLastActiveRaceCarInterface.GetPlayerPosition();

                // [!] THE TWO ZEROS ARE THE CONSOLE'S -- see the banner above. E_MODE_OFFLINE_RACE
                // and E_GAMEMODESTARTMECHANISM_DEFAULT, spelled by name so nobody reads them as
                // placeholders.
                lStartGameModeParams.Construct(GameStateModuleIO::E_MODE_OFFLINE_RACE,
                                               lPlayerPosition,
                                               E_GAMEMODESTARTMECHANISM_DEFAULT);

                // The AddCheckpoint loop, 0x823A26D0..0x823A2704. AddCheckpoint's declared order is
                // (landmarkIndex, aiSectionIndex) and the console passes r4 = the +0x08 array,
                // r5 = the +0x2C array -- so the +0x08 run is the LANDMARK argument and the +0x2C
                // run is the SECTION argument, which is the opposite of the DWARF member names.
                // [!] FLAG: the arm follows the ASM's argument positions, not the member names; the
                // two disagree and the asm is what runs. Named, not silently reconciled.
                const s32 liNumLandmarks =
                    static_cast<s32>(D4_PlayerAcceptedModeEventView::GetNumLandmarks(lpEvent));

                // The console has no bound check -- its loop runs to the event's own count. The
                // clamp below is NOT a behaviour change on any well-formed event (the payload
                // carries 16 slots and the destination Array is 16 deep); it exists because the
                // 4-byte layout discrepancy in the view above means a MALFORMED count would walk
                // off both arrays. Stated as a deviation rather than hidden.
                const s32 liClampedCount =
                    (liNumLandmarks > D4_PlayerAcceptedModeEventView::KI_MAX_LANDMARKS)
                        ? D4_PlayerAcceptedModeEventView::KI_MAX_LANDMARKS
                        : liNumLandmarks;

                for (s32 liIndex = 0; liIndex < liClampedCount; ++liIndex)
                {
                    lStartGameModeParams.AddCheckpoint(
                        static_cast<LandmarkIndex>(
                            D4_PlayerAcceptedModeEventView::GetSectionId(lpEvent, liIndex)),
                        D4_PlayerAcceptedModeEventView::GetLandmarkIndex(lpEvent, liIndex));
                }

                // [DIAG] NOT IN THE X360 BINARY -- but the console DOES print here (the DWARF unity
                // dump lists a four-shift StrStream in this exact scope), so a line at this
                // position is console-shaped. This one names what the arm is about to start.
                if (CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[start] event 20 -> StartGameMode mode="
                        << static_cast<s32>(GameStateModuleIO::E_MODE_OFFLINE_RACE)
                        << " mechanism=" << static_cast<s32>(E_GAMEMODESTARTMECHANISM_DEFAULT)
                        << " checkpoints=" << liClampedCount
                        << " (event count " << liNumLandmarks << ")\n";
                }

                // 0x823A2708..0x823A2714. gsm+0x1020 == mModeManager, reached by name.
                mModeManager.StartGameMode(lpOutputBuffer, &lStartGameModeParams);
            }
        }

        const CgsModule::Event* lpCurrent = lpEvent;
        liType = lpGameEventQueue->GetNextEvent(lpCurrent, &lpEvent, &liSize);
    }
}

// ============================================================================
// [D4 stuntrace WAVE D] ProcessGameEventsModeIntroBringUp -- the extracted CASES 24/25/26/27
// of ProcessGameEvents @0x823A0A18 (pseudocode lines 1121-1134; case 26's asm at 0x823A272C).
// The console arms, verbatim:
//     case 24: ModeManager::FinishedMapPan(v23 + 4128)
//     case 25: ModeManager::FinishOfflineModeIntro(v23 + 4128)
//     case 26: ModeManager::ResultsAccept(v23 + 4128); *(v23 + 181413) = 1
//     case 27: ModeManager::UserCancelCurrentMode(v23 + 4128);
//              TakedownManager::ClearRaceCarData(v23 + 568)
//
// [!] ONLY CASE 25 IS ARMED. FinishOfflineModeIntro @0x823119B0 is bodied
// (BrnModeManager_IntroPlay.cpp:540). FinishedMapPan / ResultsAccept / UserCancelCurrentMode and
// TakedownManager::ClearRaceCarData have NO declaration and NO body anywhere in the tree -- a
// tree-wide grep, not an assumption -- so their arms are written out and PARKED. Nothing faked.
// DELETE-WHEN those four land: un-park each arm exactly as quoted above.
//
// (i) WHY CASE 25 IS THE ONE THAT MATTERS TODAY. IntroState::OnEnter sets mbUseCountdown only for
// online modes and offline Showtime, so an offline stunt run's intro state has NO timer at all --
// its ONLY exit is this event. The producer half is already live and mounted
// (BridgeGuiToGameState case 163 -> game event 25, GameBridgeGUIToX_GameState.cpp:153); what is
// missing is the pre-event GUI that sends 163, which is why HarnessOfflineIntroSelfTriggerBringUp
// exists.
//
// [!][!] AND WHAT HAPPENS NEXT IS NOT THIS FUNCTION'S FAULT: FinishOfflineModeIntro advances the
// mode to E_GMS_COUNTDOWN, and CountdownState::Update advances only when
// (mfCountdownSeconds <= 0 && mpGameMode->ShouldCountdownEnd()). StuntAttackMode::ShouldCountdownEnd
// returns mbPlayerPointingInStartDirection, whose ONLY writer is StuntAttackMode::PreWorldUpdate
// (@0x82344EE0, BrnStuntAttackMode.cpp:467) -- so the countdown DELIBERATELY HOLDS until the car
// faces the junction's start direction. A "stuck countdown" is very probably a car pointing the
// wrong way, not a missing clock. The diag rung below says so in the log.
// ============================================================================
void GameStateModule::ProcessGameEventsModeIntroBringUp(
        const CgsModule::VariableEventQueue<1536, 16>* lpGameEventQueue)
{
    if (lpGameEventQueue == 0)
    {
        return;
    }

    const CgsModule::Event* lpEvent = 0;
    s32                     liSize  = 0;
    s32                     liType  = lpGameEventQueue->GetFirstEvent(&lpEvent, &liSize);

    while (lpEvent != 0)
    {
        switch (liType)
        {
        case 24:   // E_EVENT_FINISHED_MAP_PAN
            // [X] PARKED: ModeManager::FinishedMapPan has no declaration and no body on this tree.
            //     mModeManager.FinishedMapPan();
            break;

        case 25:   // E_EVENT_GUI_FINISHED_OFFLINE_PRE_EVENT
            // The console's whole arm. FinishOfflineModeIntro asserts IsInGameMode() &&
            // !IsOnlineGameMode() and then sends E_GME_NEXT to the current mode, so it must not be
            // called with no mode running -- the console does not guard it either, and its own
            // asserts are the guard. The gate here is the same predicate its first assert names,
            // so on a stray event the log says which, instead of the assert storming.
            if (mModeManager.GetCurrentGameMode() != 0)
            {
                if (CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[start] event 25 -> FinishOfflineModeIntro (mode state "
                        << mModeManager.GetCurrentGameMode()->GetCurrentState()
                        << " -> countdown; the countdown then HOLDS until "
                        << "StuntAttackMode::mbPlayerPointingInStartDirection is true, i.e. until "
                        << "the car faces the junction start direction)\n";
                }
                mModeManager.FinishOfflineModeIntro();
            }
            else if (CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[start] event 25 DROPPED -- no game mode running (console asserts "
                       "IsInGameMode() here)\n";
            }
            break;

        case 26:   // E_EVENT_RESULTS_FINISHED
            // [X] PARKED: ModeManager::ResultsAccept has no body on this tree, and the console's
            // companion store *(gsm + 181413) = 1 writes a byte with no member on this build.
            //     mModeManager.ResultsAccept();
            //     <gsm+181413> = 1;
            break;

        case 27:   // E_EVENT_POST_EVENT_LEAVE
            // [X] PARKED: neither ModeManager::UserCancelCurrentMode nor
            // TakedownManager::ClearRaceCarData exists on this tree.
            //     mModeManager.UserCancelCurrentMode();
            //     mTakedownManager.ClearRaceCarData();
            break;

        default:
            break;
        }

        const CgsModule::Event* lpCurrent = lpEvent;
        liType = lpGameEventQueue->GetNextEvent(lpCurrent, &lpEvent, &liSize);
    }
}

// ============================================================================
// [D4 stuntrace WAVE D] HarnessOfflineIntroSelfTriggerBringUp -- TEMPORARY, NOT CONSOLE CODE.
// The full justification and the DELETE-WHEN are at the declaration in BrnGameStateModule.h.
// Shape: while the current mode sits in E_GMS_INTRO, accumulate the frame timestep; once it passes
// the mode's own GameMode::GetIntroDurationSeconds() (vtable slot 8; StuntAttackMode's override
// returns 6.0f, X360 0x827E2538 loading flt [0x82021240]), post game event 25 -- ONE byte, the
// same "signal" shape BridgeGuiToGameState uses for its 163 -> 25 relay
// (GameBridgeGUIToX_GameState.cpp:153-155) -- into the carry queue, where the case-25 arm above
// drains it on the next pre-world leg. The accumulator resets whenever the mode leaves INTRO, so a
// second event gets its own full intro.
// ============================================================================
void GameStateModule::HarnessOfflineIntroSelfTriggerBringUp(f32 lfGameTimestep)
{
    static f32  sfIntroElapsed = 0.0f;
    static bool sbPosted       = false;

    // â­ 2026-08-27 (frontier round 2): STAND DOWN when the REAL 163 producer exists. With
    // BRN_EVENT_FSM armed the HUD flow runs the real PreRaceFlyByState, whose Update tail
    // posts GUI 163 at the end of the fly-by (proven: run 20260827_134528 log:6815 carried a
    // SECOND event 25 at intro+6.15 s -- the fly-by's own). Both firing made
    // FinishOfflineModeIntro run TWICE and the second SendEvent(E_GME_NEXT) short-circuited
    // CountdownState to 0.13 s. This stand-in exists ONLY for runs where the fly-by cannot
    // run (the FSM hop gated off); it dies entirely with the BRN_EVENT_FSM exe gate.
    static const bool sbRealFlyByArmed = (getenv("BRN_EVENT_FSM") != 0);
    if (sbRealFlyByArmed)
    {
        return;
    }

    const GameMode* lpCurrentGameMode = mModeManager.GetCurrentGameMode();

    // E_GMS_INTRO is the state IntroState owns; anything else (or no mode) rearms the trigger.
    if (lpCurrentGameMode == 0 ||
        lpCurrentGameMode->GetCurrentState() != GameStateModuleIO::E_GMS_INTRO)
    {
        sfIntroElapsed = 0.0f;
        sbPosted       = false;
        return;
    }

    if (sbPosted)
    {
        return;
    }

    sfIntroElapsed += lfGameTimestep;

    const f32 lfIntroDuration = lpCurrentGameMode->GetIntroDurationSeconds();
    if (sfIntroElapsed < lfIntroDuration)
    {
        return;
    }

    sbPosted = true;

    if (CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "[start] HARNESS-ONLY offline-intro self-trigger: intro ran "
            << sfIntroElapsed << " s >= GetIntroDurationSeconds() " << lfIntroDuration
            << " -- posting game event 25 in place of the unreconstructed pre-event GUI "
               "(GUI command 163)\n";
    }

    // The 1-byte signal payload, exactly as the GUI bridge posts it.
    unsigned char lSignal = 0;
    mGameEventCarryQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSignal), 25, 1);
}

// ============================================================================
// [D4 stuntrace WAVE D] HarnessInjectEventStartBringUp -- HARNESS-ONLY, NOT IN THE X360 BINARY.
//
// WHAT IT IS FOR: the console's offline start gesture is ANALOGUE -- SetButtonPressed @0x823BA240
// computes the pre-world buffer's ControllerInput byte +0x45 (mbRaceModePressed) as
// (padAction[+0x00] > 0.25f && padAction[+0x08] > 0.25f), i.e. both analogue triggers
// (accelerator AND brake), and ShouldStartSnapRaceMode @0x82363700 then requires it held for
// 0.35 s at speed <= 30 before it writes start mechanism 2 and DetectModeStarts calls
// StartModeAtLights. A scripted boot-drive (tools/diagnostics/flow_run.ps1) cannot hold two
// analogue triggers, so this leg stands in for the HOLD -- and for nothing else.
//
// WHAT IT SUBSTITUTES AND WHAT IT DOES NOT. It calls StartModeAtLights @0x82396CF8 directly with
// E_GAMEMODESTARTMECHANISM_SPIN_WHEELS_AT_LIGHTS (2) -- the exact value ShouldStartSnapRaceMode
// writes at a junction, and the value StartModeAtLights EARLY-RETURNS without
// (`cmpwi cr6, r31, 2 / bne loc_82397300` @0x82396D64). Everything downstream of that point is
// the console's own: the junction lookup, the RaceEventData fetch, the mu8Mode -> runtime-mode map
// through ProgressionManager::GetEvent, the special-event-car gate, ModeManager::StartGameMode.
// So this bypasses the GESTURE, not the START. If the junction is wrong, or the event does not
// resolve, or the special-event-car gate rejects the current car (it posts action 272 and returns
// -- expected on Burning Route junctions with an arbitrary car), this leg fails exactly the way
// the real gesture would, which is the point.
//
// GATES, all three required, and it fires AT MOST ONCE per process:
//   1. BRN_START_EVENT=1 in the environment (read once, like every other diag gate in this file),
//   2. TriggerQueryManager::IsPlayerInTrafficLightRegion() -- the player is actually standing in a
//      traffic-light trigger box. This is the console's own precondition for mechanism 2, and it
//      is also what makes GetPlayerCurrentTrafficLightId() safe to read (it asserts the same
//      predicate). Its writer is TriggerQueryManager's light-region leg; if this never becomes
//      true, the junction detection is the thing that is broken, not this hook.
//   3. no game mode already running (the same !mpCurrentGameMode gate the console's own start
//      arms carry).
//
// [!] IT IS NOT THE CASE-20 PATH, DELIBERATELY. The case-20 arm in this same file hard-codes
// E_MODE_OFFLINE_RACE / mechanism DEFAULT -- that is the binary's, proven at that arm's banner --
// so injecting through case 20 would start an offline RACE, never a stunt run.
//
// DELETE-WHEN the offline event flow can be driven by a real pad in the harness (or the gesture is
// scriptable): this function and its one call site go together.
// ============================================================================
void GameStateModule::HarnessInjectEventStartBringUp(GameStateModuleIO::OutputBuffer* lpOutputBuffer)
{
    static const bool sbHarnessStart = (getenv("BRN_START_EVENT") != 0);
    static bool       sbFired        = false;

    if (!sbHarnessStart || sbFired || lpOutputBuffer == 0 || mpPreWorldInputBuffer == 0)
    {
        return;
    }

    if (!mTriggerQueryManager.IsPlayerInTrafficLightRegion())
    {
        return;
    }

    if (mModeManager.GetCurrentGameMode() != 0)
    {
        return;
    }

    sbFired = true;

    if (CgsDev::Log::gpDebugPrint != 0)
    {
        // GetPlayerCurrentTrafficLightId asserts IsPlayerInTrafficLightRegion(), which the gate
        // above has already established -- this read is safe here and nowhere else.
        *CgsDev::Log::gpDebugPrint
            << "[start] ***** HARNESS-ONLY START INJECTION (BRN_START_EVENT=1) ***** "
            << "light trigger id "
            << static_cast<s32>(mTriggerQueryManager.GetPlayerCurrentTrafficLightId())
            << " -- bypassing the 0.35 s analogue accelerator+brake hold and calling "
            << "StartModeAtLights with mechanism "
            << static_cast<s32>(E_GAMEMODESTARTMECHANISM_SPIN_WHEELS_AT_LIGHTS)
            << " (the value ShouldStartSnapRaceMode writes at a junction). Everything downstream "
            << "is the console's own. One-shot; will not fire again this process.\n";
    }

    // The caller holds the pre-world buffer's READ lock and the output buffer's WRITE lock, which
    // is the same bracket DetectModeStarts runs under one line above.
    StartModeAtLights(mpPreWorldInputBuffer, lpOutputBuffer,
                      E_GAMEMODESTARTMECHANISM_SPIN_WHEELS_AT_LIGHTS);
}

}
