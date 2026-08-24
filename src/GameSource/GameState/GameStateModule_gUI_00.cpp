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

namespace BrnGameState
{

// ============================================================================
// ⭐ [gateui] GetDeveloperChallengeManager -- the body behind the declaration the StreetManager
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
// ⭐⭐ [gateui] PostWorldUpdateStuntBringUp -- the two stunt-chain legs of the console's
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
        const CgsModule::VariableEventQueue<1536, 16>* lpWorldGameEventQueue)
{
    // ---- leg 1: refresh the cached active-race-car snapshot ---------------------------------
    // ⚠️ COPIED BY ASSIGNMENT, NEVER AT THE CONSOLE'S LITERAL 10480 BYTES. 10480 is the X360
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
}

// ============================================================================
// ⭐⭐ [gateui] ProcessGameEventsPropHitBringUp -- the extracted CASE-111 arm of
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
// ⓘ Game EVENT ids are NOT shifted the way game ACTION ids are in this range (see the long
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
            // entirely. ⭐ ROUND-8 CORRECTIONS: (i) this round-7 note used to add "each pair 2 log
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
// ⭐ [H1 district wave 2026-08-25] ProcessGameEventsWorldRegionBringUp -- the extracted
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
// ⭐⭐ [gateui] PreWorldUpdateStuntBringUp -- the three stunt-chain legs of the console's
// PreWorldUpdate @0x823A5328, IN THE CONSOLE'S OWN ORDER. The header carries the line-by-line map
// of the source function and both named reductions; the body annotates each leg again.
// ============================================================================
void GameStateModule::PreWorldUpdateStuntBringUp(f32 lfGameTimestep, bool lbIsAGameModeActive)
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

    // ---- 1) the merged queue -> ProcessGameEvents (case 111 LATCHES) ------------------------
    // X360 lines 239-245 Construct a LOCAL <1536,16> queue and Append THREE sources into it -- the
    // carry queue (+248384), the PreWorldInputBuffer's queue, and the InviteManager's (+2032) --
    // then Clear the carry queue; line 252 hands that local queue to ProcessGameEvents.
    // REDUCED to the carry queue alone: the other two sources have no producer on this build
    // (nothing creates a PreWorldInputBuffer, and the InviteManager's queue is never written), so
    // the local queue would be a byte-for-byte copy of the carry queue. The Clear IS the
    // console's, and it is what makes the queue a strict one-frame buffer.
    ProcessGameEventsPropHitBringUp(&mGameEventCarryQueue);
    // ⭐ [tut-ticker] the dispatcher's CASE-113 arm, over the same merged queue in the same
    // walk position (the console's ProcessGameEvents handles every case in one pass; this
    // tree extracts one arm per function -- see the arm's banner in BrnGameStateModule.cpp).
    // MUST run before the Clear below, for the same reason the prop-hit arm does.
    ProcessGameEventsTrainingRequestBringUp(&mGameEventCarryQueue);
    // ⭐ [H1 district wave] the dispatcher's CASE-115 arm (the HUD district marker's feed),
    // same walk, same must-run-before-the-Clear constraint; it posts onto the action queue
    // this function already holds the write lock for.
    ProcessGameEventsWorldRegionBringUp(&mGameEventCarryQueue, lpActionQueue);
    mGameEventCarryQueue.Clear();

    // ---- 2) TriggerQueryManager: ARM the trigger set -----------------------------------------
    // X360 line 310 calls TriggerQueryManager::PreWorldUpdate @0x8239F5C8, whose FIRST statement
    // is `UpdateTriggers(this, lpOutput, lpActiveRaceCarInterface)`. That is the ONLY writer of
    // maActiveTriggers anywhere in the image -- the array StuntManager::OnPropHit walks -- so it
    // is the leg this wave needs. Its siblings inside that function (SubmitTriggerQueries, the
    // per-player-trigger fan-out that posts action 109 and calls ProcessPlayerTriggers, the
    // killzone-action drain) walk Array<u16,32> members this tree's TriggerQueryManager slice does
    // not model; parked, not faked.
    // ⓘ IT RUNS AFTER ProcessGameEvents, so OnPropHit above walked the PREVIOUS frame's armed
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
    // ⓘ ORDER IS THE CONSOLE'S: the fan-out runs AFTER UpdateTriggers (it reads the set
    // UpdateTriggers just armed) and BEFORE StuntManager::Update (which consumes the latch it
    // writes). Both halves of that sandwich are load-bearing -- do not reorder.
    // [FLAG PC bring-up] the DriveThruManager argument is NULL: the console passes
    // GameStateModule+44240, a sub-object this tree's GameStateModule does not model, and
    // ProcessPlayerTriggers' drive-thru arm is itself parked ((void)lpDriveThruManager, see the
    // measured LNK cost recorded there), so the pointer is never dereferenced.
    // DELETE-WHEN BrnDriveThruManager.cpp compiles and the sub-object is modelled.
    mTriggerQueryManager.PreWorldUpdatePlayerTriggersBringUp(
        mpOutputBuffer, &mLastActiveRaceCarInterface, &mStuntManager,
        /*lpDriveThruManager*/ 0, GetVehicleList());

    // [DIAG] NOT IN THE X360 BINARY. Rung 0 of the `[UI-gate]` ladder, one-shot on the first frame
    // the armed set is non-empty: how many armed regions are SMASH (generic-region sub-type 8) and
    // BILLBOARD (sub-type 12). This is the line that separates "the prop was outside every smash
    // region" from "the trigger pump never ran" -- the wave's known blocker. Same logger and same
    // env guard (BRN_PROP_DIAG) as the "[prop-diag] BREAK" rung this ladder hangs off.
    //
    // ⓘ ROUND-7 NOTE -- READ THIS BEFORE DRAWING A CONCLUSION FROM THIS LINE. It is a ONE-SHOT and
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

    // ---- 3) StuntManager::Update: CONSUME the latch ------------------------------------------
    // X360 line 332:
    //     v51 = GameStateModuleIO::OutputBuffer::GetGameActionQueue(a5);
    //     StuntManager::Update(a1 + 183952, v51, a1 + 235488, <f1 == the game timestep>, v50);
    // The timestep rides f1 and consumes NO GPR slot (the PPC float-arg rule), which is why the
    // pseudocode renders only four arguments. The interface argument is the module's own cached
    // snapshot -- which PostWorldUpdateStuntBringUp above is what keeps alive.
    mStuntManager.Update(lpActionQueue, &mLastActiveRaceCarInterface,
                         lfGameTimestep, lbIsAGameModeActive);

    mbIsUpdating = false;
    mpOutputBuffer->UnlockForWrite();
}

}
