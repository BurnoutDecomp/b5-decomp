// =================================================================================================
// BrnAIModule_Events.cpp -- the ACTIVATION half of BrnAI::AIModule's per-frame spine
// (aiwave lane A7, 2026-09-03). Partfile of BrnAIModule.cpp.
//
//   BrnAI::AIModule::HandleManagementEvents      @0x82798620  (586 insns; whole)
//   BrnAI::AIModule::HandleGameActions           @0x82791FD0  (280 insns; whole)
//   BrnAI::AIModule::OnRollingStart              @0x8276E5C8  (37 insns;  whole)
//   BrnAI::AIModule::OnModeStartRacing           @0x8276E4B0  (69 insns;  whole)
//   BrnAI::AIModule::OnModeFinished              @0x8277B970  (67 insns;  whole)
//   BrnAI::AIModule::OnRaceCarReachedFinish      @0x8277B8D0  (40 insns;  whole)
//   BrnAI::AIModule::OnModeStart                 @0x82791DB8  (133 insns; 2 named parks inside)
//   BrnAI::AIModule::OnModeEnd                   @0x8277BA80  (96 insns;  1 named park inside)
//   BrnAI::AIModule::OnPlayerTakedown            @0x8278A720  (34 insns;  NAMED PARK)
//   BrnAI::AIModule::OnRaceCarReachedCheckpoint  @0x8278A658  (NAMED PARK -- ARTIST export hole)
//
// =================================================================================================
// ⭐⭐⭐ WHY THIS FILE EXISTS: NOTHING ELSE ON THIS BUILD ACTIVATES AN AI DRIVER.
//
// Lane A1 landed every drive leg of AIModule::Update (BrnAIModule_Drive.cpp) and found all of them
// silent no-ops: UpdateDrivers skips a slot whose AIDriver::mbIsActive is clear, StoreDrivenCarData
// skips a driver with no AICar, ProcessAIVehicleInputs emits no record for either. `mbIsActive` has
// exactly ONE writer that turns it ON in the whole image -- AIDriver::SetAICar @0x827963C8 -- and
// exactly ONE caller of that: HandleManagementEvents' E_EVENT_ACTIVATE_RACE_CAR arm, right here
// (asm 0x827989C0). Two writers turn it OFF, both also here (the DEACTIVATE arm @0x82798B68).
//
// THE ACTIVATION SEQUENCE, end to end (all four hops are in the tree today):
//
//  1. PRODUCER -- RaceCarEntityModule, writing its OWN OutputBuffer_PreScene's RaceCarAIInterface:
//       RCEM::SpawnRaceCar @0x822FE5D8    -> mManagementQueue.AddEvent<AttachAIControlEvent>(ev, 0)
//                                            (BrnRaceCarEntityModule.cpp:2216..:2222)
//       RCEM::AttachAIControl             -> RaceCarAIInterface::ActivateRaceCar(global, active)
//                                            == AddEvent<ActivateRaceCarEvent>(ev, 1)
//       RCEM::RemoveRaceCar @~0x82304564  -> ::DeactivateRaceCar(global, isInAMode)  (type 2)
//                                            (BrnRaceCarEntityModule.cpp:1981)
//       RCEM::DetachAIControl             -> ::DetachAIControl(global)               (type 3)
//                                            (BrnRaceCarEntityModule.cpp:2084)
//       RCEM::SetUpAIForMode @0x82301620  -> ::SetUpOutOfRangeRaceCar(...)           (type 5)
//                                          + AddEvent<AddCarToCurrentModeEvent>(ev, 6)
//                                            (BrnRaceCarEntityModule_Rivals.cpp:731/:742..:751 and
//                                             BrnRaceCarEntityModule_ModeArming.cpp:731/:742..:751)
//       (types 4 PlayerControlChangedEvent and 7 RemoveCarFromCurrentModeEvent have their typed
//        AddEvent instantiations committed -- VariableEventQueue_16384_16.cpp:41/:42 -- but no
//        reconstructed producer yet; see `## parks` in the lane report.)
//
//  2. BRIDGE -- WorldModule::BridgeRaceCarModuleToAIModule_PreScene @0x827A4FA0
//       (WorldBridgeEntityModulesToAI.cpp:15, called from BrnWorldModule.cpp:2759 -- WIRED, its
//        boot gate was retired 2026-08-26):
//            lpAIInputBuffer->SetRaceCarAIInterface(
//                lpRaceCarOutputBuffer_PreScene->GetRaceCarAIInterface());
//       and InputBuffer::SetRaceCarAIInterface (BrnAIModuleIO_InputBuffer_Accessors.cpp:218) is
//       `mRaceCarAIInterface = *lpInterface;` -- the console's XMemCpy of all 0x43D0 bytes, which
//       INCLUDES the 16 KB management queue at RaceCarAIInterface+0x2F8. The queue therefore
//       arrives at the AI module by value, as a copy, every frame.
//
//  3. CONSUMER -- this file. AIModule::Update row #16 (asm 0x8279B714) calls
//       HandleManagementEvents(lpInputBuffer), which reads
//       `lpInputBuffer->GetRaceCarAIInterface()->mManagementQueue` -- the SAME named member the
//       producer's AddEvent writes and the bridge copies. Both ends match by name; the console
//       reaches it as `RaceCarAIInterface + 760` (0x2F8), which BrnRaceCarAIInterfaces.h pins.
//
//  4. EFFECT -- E_EVENT_ACTIVATE_RACE_CAR (id 1) calls AIDriver::SetAICar, which sets
//       driver->mbIsActive = 1 and car->meCarState = E_AI_CAR_STATE_IN_RANGE. From the next frame
//       on, UpdateDrivers/StoreDrivenCarData/ProcessAIVehicleInputs all see that slot.
//
// WHICH EVENT IDS ACTIVATE A DRIVER (the answer the lane report repeats):
//    id 0  E_EVENT_ATTACH_AI_CONTROL   -- car INACTIVE -> OUT_OF_RANGE. No driver yet.
//    id 1  E_EVENT_ACTIVATE_RACE_CAR   -- ⭐ THE ONE. driver->SetAICar(car): mbIsActive = 1,
//                                          driver->mpCar = car, car->meCarState = IN_RANGE.
//    id 2  E_EVENT_DEACTIVATE_RACE_CAR -- the inverse: mbIsActive = 0, driver->mpCar = NULL,
//                                          car back to OUT_OF_RANGE.
//    id 3  E_EVENT_DETACH_AI_CONTROL   -- car OUT_OF_RANGE -> INACTIVE.
//
// =================================================================================================
// THE TWO QUEUES THIS FILE DRAINS
//
//   HandleManagementEvents: VariableEventQueue<16384,16>, console RaceCarAIInterface+0x2F8, reached
//     as InputBuffer::GetRaceCarAIInterface()->mManagementQueue. Event ids are
//     BrnAI::AIModuleIO::EEvent (BrnRaceCarAIInterfaces.h, DWARF :284) 0..7, and the console's
//     switch is a DENSE 8-entry jump table (asm 0x827987CC `cmplwi r29, 7` -> jpt_827987EC), so
//     every id maps 1:1 onto that enum with no shift.
//
//   HandleGameActions: VariableEventQueue<13312,16>, console InputBuffer+0x103BC, reached as
//     InputBuffer::GetGameActionQueue() (X360 0x8276D530). Its jump table is keyed on
//     `type - 7` (asm 0x82792084 `addi r11, r3, -7`; `cmplwi 0x7C` -> 125 cases), so IDA's
//     "jumptable case N" is action type N + 7. The sixteen live arms are types
//     7, 14, 23, 30, 34, 35, 39, 50, 99, 106, 113, 114, 120, 122, 123, 131 -- see the enum grow in
//     BrnGameActions.h for the six of those this tree did not carry yet.
//
// =================================================================================================
// CONSOLE-vs-HOST NOTES (every deviation is flagged at its site as well)
//
//  * X360 offsets in the comments are IDENTITY, never host addressing. Everything is by member.
//  * The console's asserts are reproduced non-gating (CGS_ASSERT) exactly where they fire; a
//    `[GUARD]` early-out is added ONLY where the console would then dereference a NULL, and each
//    one says so.
//  * `flt_82001CC0` (the f31 both functions preload) is 0.0f -- image.bin file offset 0x1CC0 reads
//    00000000. Spelled 0.0f here.
//  * The console's `stfs f31, <int field>` stores into meBehaviour-adjacent INT fields are 4-byte
//    zero stores; they are written as the typed zero of the real member (e.g. `miNodeCount = 0`),
//    which is the same bit pattern.
//  * TWO DIFFERENT CASTS, for two different record families. The management-event records derive
//    from CgsModule::Event (BrnRaceCarAIInterfaces.h), so the queue payload is downcast with
//    static_cast. The GAME-ACTION records derive from the empty tag `GameAction<T>`
//    (BrnGameActions.h:743) instead, which is NOT an Event, so the payload is reinterpret_cast --
//    the same idiom, for the same reason, as the other HandleGameActions in this tree
//    (BrnRaceCarEntityModule.cpp:2532).
// =================================================================================================

#include "GameSource/World/AI/BrnAIModule.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAIDriver.h"
#include "GameSource/World/AI/BrnAIBuzzBy.h"
#include "GameSource/World/AI/BrnAIAggressiveness.h"                   // Aggressiveness::SetAggression
#include "GameSource/World/AI/BrnAISharedConstants.h"                  // EAIBehaviour / EAICarState / ERouteFindingStyle
#include "GameSource/World/AI/Route/BrnRoute.h"                        // BrnAI::Route (the in-place route invalidations)
#include "GameSource/World/AI/Route/BrnRouteMapModuleIO.h"             // RouteMapModuleIO::InputBuffer / RaceRouteRequest
#include "GameSource/World/AI/SharedIO/BrnAIModuleIO.h"                // AIModuleIO::InputBuffer (+ its GameActionQueue)
#include "GameSource/World/AI/SharedIO/BrnAIModuleIO_OutputBuffer.h"   // AIModuleIO::OutputBuffer (carried, never read)
#include "GameSource/World/AI/SharedIO/BrnRaceCarAIInterfaces.h"       // RaceCarAIInterface + the eight EEvent records
#include "GameSource/World/BrnWorldSharedConstants.h"                  // BrnWorld::E_CAR_CONTROL_AI_MODULE
#include "GameSource/GameState/BrnGameActions.h"                       // EGameActionType + the action records
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"  // BrnGameState::GameModeParams
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cmath>

namespace BrnAI
{

namespace
{
    // The console's "is this a real number" test is a per-lane `vcmpeqfp v,v` self-compare -- a
    // NaN test, NOT a finiteness test (an infinity passes it). Spelled exactly that way.
    inline bool IsNotNaN(f32 lfValue) { return !(lfValue != lfValue); }

    // [FLAG PC witness] first-8 witnesses, one per HANDLED management-event type and one per
    // handled game-action type. Bounded by construction (8 ids x 8 lines, 16 ids x 8 lines) and
    // silent once the counters saturate, so this can never become a per-frame flood.
    // DELETE-WHEN a Road Rage run shows rivals driving and the ledger closes AIModule::Update.
    s32 saiManagementWitness[AIModuleIO::E_EVENT_COUNT] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    inline bool WitnessManagementEvent(s32 liType)
    {
        if (liType < 0 || liType >= AIModuleIO::E_EVENT_COUNT) { return false; }
        if (saiManagementWitness[liType] >= 32 || CgsDev::Log::gpDebugPrint == 0) { return false; }   // (cap raised 8 -> 32, 2026-09-03: 5 rivals + player re-activations exhausted 8)
        ++saiManagementWitness[liType];
        return true;
    }

    // The sixteen game-action ids HandleGameActions handles, in the console's jump-table order.
    // Index into saiGameActionWitness is the position in this table, not the id.
    const s32 KAI_HANDLED_GAME_ACTIONS[16] =
    {
        BrnGameState::GameStateModuleIO::E_ACTION_SET_PLAYER_CAR_DRIVER,              //   7
        BrnGameState::GameStateModuleIO::E_ACTION_ON_PLAYER_TAKEDOWN,                 //  14
        BrnGameState::GameStateModuleIO::E_ACTION_PREPARE_FOR_MODE,                   //  23
        BrnGameState::GameStateModuleIO::E_ACTION_STOP_MODE_INTRO,                    //  30
        BrnGameState::GameStateModuleIO::E_ACTION_START_PLAYING_MODE,                 //  34
        BrnGameState::GameStateModuleIO::E_ACTION_FINISHED_MODE_NOTIFY,               //  35
        BrnGameState::GameStateModuleIO::E_ACTION_STOP_MODE,                          //  39
        BrnGameState::GameStateModuleIO::E_ACTION_REQUEST_ROUTE_INFO,                 //  50
        BrnGameState::GameStateModuleIO::E_ACTION_DRIVE_THRU_JUNK_YARD,               //  99
        BrnGameState::GameStateModuleIO::E_ACTION_DRIVE_THRU_JUNK_YARD_ON_GAME_START, // 106
        BrnGameState::GameStateModuleIO::E_ACTION_RACE_CAR_REACHED_CHECKPOINT,        // 113
        BrnGameState::GameStateModuleIO::E_ACTION_RACE_CAR_REACHED_FINISH,            // 114
        BrnGameState::GameStateModuleIO::E_ACTION_SHUTDOWN,                           // 120
        BrnGameState::GameStateModuleIO::E_ACTION_AWARD_SEQUENCE_START,               // 122
        BrnGameState::GameStateModuleIO::E_ACTION_AWARD_SEQUENCE_END,                 // 123
        BrnGameState::GameStateModuleIO::E_ACTION_UPDATE_ROAD_RAGE_MADNESS            // 131
    };
    s32 saiGameActionWitness[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    inline bool WitnessGameAction(s32 liType)
    {
        if (CgsDev::Log::gpDebugPrint == 0) { return false; }
        for (s32 li = 0; li < 16; ++li)
        {
            if (KAI_HANDLED_GAME_ACTIONS[li] == liType)
            {
                if (saiGameActionWitness[li] >= 8) { return false; }
                ++saiGameActionWitness[li];
                return true;
            }
        }
        return false;
    }

    // The console's streamed assert form (StrStream over CgsDev::Assert::gpcMessageBuffer). Used
    // by the two arms whose message embeds a number: HandleManagementEvents' "AI car in unknown
    // state <n>" (BrnAIModule.cpp:1862) and its default arm's "Unknown RaceCar->AI management
    // event: <n>" (:1983).
    void FireStreamedAssert(const char* lpcPrefix, s32 liValue, const char* lpcSuffix)
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << lpcPrefix << liValue << lpcSuffix;
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
        CgsDev::Assert::EndAssert();
    }

    // The console inlines AICar::SetBehaviour's body (WITHOUT its range assert -- there is no
    // `bl AICar::SetBehaviour` at any of these sites) in nine places across this file:
    //   mfBehaviourTimer = 0; mePreviousBehaviour = meBehaviour; meBehaviour = new.
    // Note the ORDER the asm uses: the old behaviour is loaded BEFORE the new one is stored, so a
    // self-assignment still lands the old value in mePreviousBehaviour.
    inline void SetBehaviourInline(AICar* lpCar, EAIBehaviour leBehaviour)
    {
        const EAIBehaviour lePrevious = lpCar->meBehaviour;
        lpCar->mfBehaviourTimer   = 0.0f;
        lpCar->meBehaviour        = leBehaviour;
        lpCar->mePreviousBehaviour = lePrevious;
    }

    // The console's inlined "this route is stale" pair: Route::meStatus = E_STATUS_UNINITIALISED
    // and Route::miNodeCount = 0, both reached through the AICar pointer (mRoute is at AICar+0).
    inline void InvalidateRouteInline(AICar* lpCar)
    {
        Route* const lpRoute = lpCar->GetRoute();
        lpRoute->meStatus    = Route::E_STATUS_UNINITIALISED;   // car+0x1408
        lpRoute->miNodeCount = 0;                               // car+0x1400
    }
}

// =================================================================================================
// HandleManagementEvents @0x82798620   (DWARF BrnAIModule.cpp:1774)
//
// int __fastcall (r3 = this, r4 = lpInputBuffer). Three non-gating null asserts, then the whole
// body is one GetFirstEvent/GetNextEvent walk over the RaceCarAIInterface's management queue with
// a dense 8-way switch on the event id.
//
// Register map: r28 = this, r27 = the event pointer, r29 = the event id, r24 = the baked source
// path, r26 = 0, r23 = 1, r21 = 0x4EBE2 (== &mBuzzBy.mbResetBuzzTimers), r30 = 0x4EB74
// (== &meDefaultAIRouteFindingStyle), r18..r14 = the hoisted assert strings.
// =================================================================================================
void AIModule::HandleManagementEvents(const AIModuleIO::InputBuffer* lpInputBuffer)
{
    CGS_ASSERT(lpInputBuffer != 0, "lpInputBuffer != NULL");                              // :1784
    if (lpInputBuffer == 0)
    {
        return;   // [GUARD] the console dereferences it two instructions later (0x82798664).
    }

    const AIModuleIO::RaceCarAIInterface* const lpCarInterface = lpInputBuffer->GetRaceCarAIInterface();
    CGS_ASSERT(lpCarInterface != 0, "lpCarInterface != NULL");                            // :1789
    if (lpCarInterface == 0)
    {
        return;   // [GUARD] the console then forms lpCarInterface + 0x2F8 and reads through it.
    }

    // asm 0x82798694 `addi r31, r31, 0x2F8` -- the management queue is the interface's own member.
    const CgsModule::VariableEventQueue<16384, 16>* const lpManagementQueue =
        &lpCarInterface->mManagementQueue;
    CGS_ASSERT(lpManagementQueue != 0, "lpManagementQueue != NULL");                      // :1793

    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    s32 liType = lpManagementQueue->GetFirstEvent(&lpEvent, &liSize);

    while (lpEvent != 0)
    {
        switch (liType)
        {
        // ---- 0  E_EVENT_ATTACH_AI_CONTROL  (asm 0x82798810) --------------------------------
        // The car joins the AI module's roster: reset it to the event's personality, put it in
        // OUT_OF_RANGE, remember its car-asset key, and ask BuzzBy to restart its timers.
        case AIModuleIO::E_EVENT_ATTACH_AI_CONTROL:
        {
            const AIModuleIO::AttachAIControlEvent* const lpAttach =
                static_cast<const AIModuleIO::AttachAIControlEvent*>(lpEvent);
            const EGlobalRaceCarIndex leGlobalRaceCarIndex = lpAttach->meGlobalRaceCarIndex;

            CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,
                       "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");              // :1807
            CGS_ASSERT(leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
                       "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");           // :1808

            AICar* const lpCar = GetAICar(static_cast<u32>(leGlobalRaceCarIndex));
            if (lpCar == 0)
            {
                break;   // [GUARD] GetAICar's own out-of-range bail; the console has no such path.
            }

            // asm 0x82798864..0x82798884: the assert fires when meCarState is IN_RANGE(0) or
            // OUT_OF_RANGE(1) -- i.e. exactly AICar::IsActive().
            CGS_ASSERT(!lpCar->IsActive(), "!lpCar->IsActive()");                         // :1812

            lpCar->Reset(static_cast<EPersonalityType>(lpAttach->mePersonalityType),
                         lpAttach->mbKeepResetSection);                                   // 0x827988B0
            lpCar->meCarState        = E_AI_CAR_STATE_OUT_OF_RANGE;                       // 0x827988B4
            lpCar->mCarAssetAttribKey = lpAttach->mCarAssetAttribKey;                     // 0x827988B8 ld/std
            mBuzzBy.RequestResetBuzzTimers();                                             // 0x827988C0

            if (WitnessManagementEvent(liType))
            {
                *CgsDev::Log::gpDebugPrint
                    << "[ai-evt] ATTACH_AI_CONTROL global " << static_cast<s32>(leGlobalRaceCarIndex)
                    << " -> AICar OUT_OF_RANGE (personality " << lpAttach->mePersonalityType << ")\n";
            }
            break;
        }

        // ---- 1  E_EVENT_ACTIVATE_RACE_CAR  (asm 0x827988C8) --------------------------------
        // ⭐ THE ACTIVATION. Binds an out-of-range car to a free driver slot; AIDriver::SetAICar
        // raises mbIsActive and drops the car into IN_RANGE.
        case AIModuleIO::E_EVENT_ACTIVATE_RACE_CAR:
        {
            const AIModuleIO::ActivateRaceCarEvent* const lpActivate =
                static_cast<const AIModuleIO::ActivateRaceCarEvent*>(lpEvent);
            const EGlobalRaceCarIndex leGlobalRaceCarIndex = lpActivate->meGlobalRaceCarIndex;
            const EActiveRaceCarIndex leDriverActiveRaceCarIndex = lpActivate->meActiveRaceCarIndex;

            CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,
                       "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");              // :1831
            CGS_ASSERT(leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
                       "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");           // :1832
            CGS_ASSERT(leDriverActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                       "leDriverActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");        // :1834
            CGS_ASSERT(leDriverActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "leDriverActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");     // :1835

            AICar* const lpCar = GetAICar(static_cast<u32>(leGlobalRaceCarIndex));
            if (lpCar == 0)
            {
                break;   // [GUARD] as above.
            }
            CGS_ASSERT(lpCar->meCarState == E_AI_CAR_STATE_OUT_OF_RANGE,
                       "lpCar->GetState() == E_AI_CAR_STATE_OUT_OF_RANGE");               // :1838

            AIDriver* const lpDriver = GetAIDriver(leDriverActiveRaceCarIndex);
            if (lpDriver == 0)
            {
                break;   // [GUARD] GetAIDriver's out-of-range bail (BrnAIModule_Drive.cpp).
            }
            CGS_ASSERT(!lpDriver->IsActive(), "!lpDriver->IsActive()");                   // :1841

            lpDriver->SetAICar(lpCar);                                                    // 0x827989C0

            if (WitnessManagementEvent(liType))
            {
                *CgsDev::Log::gpDebugPrint
                    << "[ai-evt] ACTIVATE_RACE_CAR global " << static_cast<s32>(leGlobalRaceCarIndex)
                    << " -> driver slot " << static_cast<s32>(leDriverActiveRaceCarIndex)
                    << " (mbIsActive now " << static_cast<s32>(lpDriver->IsActive() ? 1 : 0)
                    << ", car state " << static_cast<s32>(lpCar->meCarState) << ")\n";
            }
            break;
        }

        // ---- 2  E_EVENT_DEACTIVATE_RACE_CAR  (asm 0x827989C8) ------------------------------
        // The inverse of 1. A car that is ALREADY out of range only logs a warning; an IN_RANGE
        // car is unbound from its driver and put back to OUT_OF_RANGE.
        case AIModuleIO::E_EVENT_DEACTIVATE_RACE_CAR:
        {
            const AIModuleIO::DeactivateRaceCarEvent* const lpDeactivate =
                static_cast<const AIModuleIO::DeactivateRaceCarEvent*>(lpEvent);
            const EGlobalRaceCarIndex leGlobalRaceCarIndex = lpDeactivate->meGlobalRaceCarIndex;
            // (mbIsInAMode is on the wire but this consumer never reads it -- asm 0x827989C8..
            //  0x82798B6C touches only word 0.)

            CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,
                       "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");              // :1852
            CGS_ASSERT(leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
                       "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");           // :1853

            AICar* const lpCar = GetAICar(static_cast<u32>(leGlobalRaceCarIndex));
            if (lpCar == 0)
            {
                break;   // [GUARD]
            }

            if (lpCar->meCarState == E_AI_CAR_STATE_OUT_OF_RANGE)                         // 0x82798A20
            {
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "WARNING: AI car " << static_cast<s32>(leGlobalRaceCarIndex)
                        << " deactivated when already out of range.\n";                   // 0x82798A48
                }
                break;
            }

            if (lpCar->meCarState != E_AI_CAR_STATE_IN_RANGE)                             // 0x82798A60
            {
                FireStreamedAssert("AI car in unknown state ",
                                   static_cast<s32>(lpCar->meCarState), "\n");            // :1862
            }

            // asm 0x82798AB0..0x82798B28: lpDriver is the car's driver ONLY while IN_RANGE;
            // otherwise the console carries a literal NULL into the assert below.
            AIDriver* const lpDriver =
                (lpCar->meCarState == E_AI_CAR_STATE_IN_RANGE) ? lpCar->GetDriver() : 0;
            CGS_ASSERT(lpDriver != 0, "lpDriver");                                        // :1865
            if (lpDriver == 0)
            {
                // [GUARD] the console's assert is non-gating and the very next instruction
                // (`lbz r11, 0x1D69(r31)`) dereferences it -- an AV here. See the lane report's
                // `## risks`: AIDriver::SetAICar does not yet call AICar::SetDriver, so
                // AICar::mpDriverHost is NULL on this build and this guard is LIVE.
                break;
            }

            CGS_ASSERT(lpDriver->IsActive(), "lpDriver->IsActive()");                     // :1866
            // AIDriver::GetGlobalRaceCarIndex() is inlined on the console as
            // `(mpCar && mbIsActive) ? mpCar->miRaceCarIndex : -1` (asm 0x82798B50..0x82798B2C).
            {
                const s32 liDriverGlobalRaceCarIndex =
                    (lpDriver->GetCar() != 0 && lpDriver->IsActive())
                        ? lpDriver->GetCar()->miRaceCarIndex
                        : -1;
                CGS_ASSERT(liDriverGlobalRaceCarIndex == static_cast<s32>(leGlobalRaceCarIndex),
                           "lpDriver->GetGlobalRaceCarIndex() == leGlobalRaceCarIndex");  // :1867
            }

            if (lpDriver->GetCar() != 0)                                                  // 0x82798B50
            {
                lpDriver->GetCar()->meCarState = E_AI_CAR_STATE_OUT_OF_RANGE;             // 0x82798B5C
                lpDriver->SetCar(0);                                                      // 0x82798B60
            }
            lpDriver->mbIsRacingLineInitialised = 0;                                      // 0x82798B64
            lpDriver->mbIsActive                = 0;                                      // 0x82798B68

            if (WitnessManagementEvent(liType))
            {
                *CgsDev::Log::gpDebugPrint
                    << "[ai-evt] DEACTIVATE_RACE_CAR global " << static_cast<s32>(leGlobalRaceCarIndex)
                    << " -> driver released (mbIsActive 0)\n";
            }
            break;
        }

        // ---- 3  E_EVENT_DETACH_AI_CONTROL  (asm 0x82798BD0) --------------------------------
        case AIModuleIO::E_EVENT_DETACH_AI_CONTROL:
        {
            const AIModuleIO::DetachAIControlEvent* const lpDetach =
                static_cast<const AIModuleIO::DetachAIControlEvent*>(lpEvent);
            const EGlobalRaceCarIndex leGlobalRaceCarIndex = lpDetach->meGlobalRaceCarIndex;

            CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,
                       "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");              // :1886
            CGS_ASSERT(leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
                       "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");           // :1887

            AICar* const lpCar = GetAICar(static_cast<u32>(leGlobalRaceCarIndex));
            if (lpCar == 0)
            {
                break;   // [GUARD]
            }
            CGS_ASSERT(lpCar->meCarState == E_AI_CAR_STATE_OUT_OF_RANGE,
                       "lpCar->GetState() == E_AI_CAR_STATE_OUT_OF_RANGE");               // :1890

            lpCar->meCarState = E_AI_CAR_STATE_INACTIVE;                                  // 0x82798C4C
            mBuzzBy.RequestResetBuzzTimers();                                             // 0x82798C50

            if (WitnessManagementEvent(liType))
            {
                *CgsDev::Log::gpDebugPrint
                    << "[ai-evt] DETACH_AI_CONTROL global " << static_cast<s32>(leGlobalRaceCarIndex)
                    << " -> AICar INACTIVE\n";
            }
            break;
        }

        // ---- 4  E_EVENT_PLAYER_TAKEN_OVER  (asm 0x82798C58) --------------------------------
        // "the player took the wheel back / handed it over". Only meaningful while the PLAYER's
        // own driver slot is active; the console calls GetAIDriver TWICE (once for the gate, once
        // for the store) and that is reproduced.
        case AIModuleIO::E_EVENT_PLAYER_TAKEN_OVER:
        {
            const AIModuleIO::PlayerControlChangedEvent* const lpControlChanged =
                static_cast<const AIModuleIO::PlayerControlChangedEvent*>(lpEvent);

            AIDriver* const lpPlayerDriverGate = GetAIDriver(mePlayerActiveRaceCarIndex);
            if (lpPlayerDriverGate == 0 || !lpPlayerDriverGate->IsActive())                // 0x82798C6C
            {
                break;
            }

            AIDriver* const lpPlayerDriver = GetAIDriver(mePlayerActiveRaceCarIndex);      // 0x82798C80
            if (lpPlayerDriver != 0)
            {
                lpPlayerDriver->mbIsRacingLineInitialised = 0;                             // 0x82798C94
            }

            AICar* const lpPlayerCar = GetAICar(static_cast<u32>(mePlayerGlobalRaceCarIndex));
            if (lpPlayerCar == 0)
            {
                break;   // [GUARD]
            }

            const EAICarState leState = lpPlayerCar->meCarState;                           // 0x82798CA4
            lpPlayerCar->mbIsDrivenByPlayer = lpControlChanged->mbPlayerIsInControl;        // 0x82798CAC

            if (leState != E_AI_CAR_STATE_IN_RANGE)                                        // 0x82798CB0
            {
                break;
            }

            AIDriver* const lpCarDriver = lpPlayerCar->GetDriver();                         // 0x82798CB4
            if (lpCarDriver == 0)
            {
                break;   // [GUARD] see the DEACTIVATE arm -- the console derefs unconditionally.
            }
            lpCarDriver->ResetPIDTuningState();                                             // 0x82798CB8

            if (WitnessManagementEvent(liType))
            {
                *CgsDev::Log::gpDebugPrint
                    << "[ai-evt] PLAYER_TAKEN_OVER inControl "
                    << static_cast<s32>(lpControlChanged->mbPlayerIsInControl ? 1 : 0)
                    << " (player global " << static_cast<s32>(mePlayerGlobalRaceCarIndex) << ")\n";
            }
            break;
        }

        // ---- 5  E_EVENT_SET_UP_OUT_OF_RANGE_RACE_CAR  (asm 0x82798CC0) ---------------------
        // The per-frame position/heading refresh for a car the AI is simulating out of range.
        case AIModuleIO::E_EVENT_SET_UP_OUT_OF_RANGE_RACE_CAR:
        {
            const AIModuleIO::SetUpOutOfRangeRaceCarEvent* const lpSetUp =
                static_cast<const AIModuleIO::SetUpOutOfRangeRaceCarEvent*>(lpEvent);
            const EGlobalRaceCarIndex leGlobalRaceCarIndex = lpSetUp->meGlobalRaceCarIndex;

            CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0 &&
                       leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
                       "lpEvent->meGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0 && "
                       "lpEvent->meGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");   // :1917

            AICar* const lpCar = GetAICar(static_cast<u32>(leGlobalRaceCarIndex));
            if (lpCar == 0)
            {
                break;   // [GUARD]
            }
            CGS_ASSERT(lpCar->meCarState == E_AI_CAR_STATE_OUT_OF_RANGE,
                       "lpCar->GetState() == E_AI_CAR_STATE_OUT_OF_RANGE");                // :1921

            // asm 0x82798D28..0x82798D84: three `vspltw` + `vcmpeqfp. v,v` self-compares over the
            // x/y/z lanes of the event's POSITION (r31 == event + 0x10).
            CGS_ASSERT(IsNotNaN(lpSetUp->mPosition.x) &&
                       IsNotNaN(lpSetUp->mPosition.y) &&
                       IsNotNaN(lpSetUp->mPosition.z),
                       "Invalid car position being passed");                                // :1925

            // asm 0x82798DA8..0x82798DC0: v1 = event+0x10 (position), v2 = event+0x20 (at),
            // r4 = lhz event+0x30 (section), r5 = lbz event+0x38 (the record's medal count --
            // AICar::UpdateOutOfRangeData's fourth parameter is spelled luSectionSpeed there).
            lpCar->UpdateOutOfRangeData(lpSetUp->mPosition,
                                        lpSetUp->mAt,
                                        lpSetUp->muSection,
                                        lpSetUp->muNumberOfMedalsToUnlock);
            mBuzzBy.RequestResetBuzzTimers();                                               // 0x82798DC4

            if (WitnessManagementEvent(liType))
            {
                *CgsDev::Log::gpDebugPrint
                    << "[ai-evt] SET_UP_OUT_OF_RANGE global " << static_cast<s32>(leGlobalRaceCarIndex)
                    << " section " << static_cast<s32>(lpSetUp->muSection) << "\n";
            }
            break;
        }

        // ---- 6  E_EVENT_ADD_CAR_TO_MODE  (asm 0x82798DCC) ----------------------------------
        // The rival joins the running game mode. This is the arm RaceCarEntityModule::
        // SetUpAIForMode @0x82301620 feeds (BrnRaceCarEntityModule_Rivals.cpp).
        case AIModuleIO::E_EVENT_ADD_CAR_TO_MODE:
        {
            const AIModuleIO::AddCarToCurrentModeEvent* const lpAddCar =
                static_cast<const AIModuleIO::AddCarToCurrentModeEvent*>(lpEvent);

            AICar* const lpCar = GetAICar(static_cast<u32>(lpAddCar->meGlobalRaceCarIndex));
            if (lpCar == 0)
            {
                break;   // [GUARD]
            }

            // asm 0x82798DD8..0x82798DF8: deviating from the route is only permitted in a RACE.
            bool lbCanDeviateFromRoute = false;
            if (lpAddCar->mbDeviateFromRoute)
            {
                lbCanDeviateFromRoute =
                    (meDefaultAIRouteFindingStyle == E_ROUTE_FINDING_RACE);
            }
            // asm 0x82798DFC..0x82798E18: the player's car gets the player style, everyone else
            // the AI style.
            const ERouteFindingStyle leRouteFindingStyle =
                lpCar->mbIsPlayer ? meDefaultPlayerRouteFindingStyle
                                  : meDefaultAIRouteFindingStyle;

            lpCar->OnModeStart(meSpeedSelectionMethod,                                      // r4
                               lpAddCar->miOpponentIndex,                                   // r5
                               leRouteFindingStyle,                                         // r6
                               lbCanDeviateFromRoute,                                       // r7
                               false,                                                       // r8 == `li r8, 0`
                               lpAddCar->muDestinationAISection,                             // r9
                               lpAddCar->mfProgressionRankAsRatio);                          // f1

            // asm 0x82798E40..0x82798E8C: the two start-mechanism overrides. Both are the inlined
            // SetBehaviour, and DONUT wins because it is applied second.
            if (mbFullRollingStart)
            {
                SetBehaviourInline(lpCar, E_AI_BEHAVIOUR_ROLLING_START);
            }
            if (mbDonutStart)
            {
                SetBehaviourInline(lpCar, E_AI_BEHAVIOUR_DONUT);
            }

            if (WitnessManagementEvent(liType))
            {
                *CgsDev::Log::gpDebugPrint
                    << "[ai-evt] ADD_CAR_TO_MODE global "
                    << static_cast<s32>(lpAddCar->meGlobalRaceCarIndex)
                    << " opponent " << lpAddCar->miOpponentIndex
                    << " style " << static_cast<s32>(leRouteFindingStyle)
                    << " dest section " << static_cast<s32>(lpAddCar->muDestinationAISection)
                    << " behaviour " << static_cast<s32>(lpCar->meBehaviour) << "\n";
            }
            break;
        }

        // ---- 7  E_EVENT_REMOVE_CAR_FROM_MODE  (asm 0x82798B70) -----------------------------
        // The car leaves the mode and goes back to free-roam cruising: route invalidated + a fresh
        // timestamp, mode flags cleared, aggression zeroed.
        case AIModuleIO::E_EVENT_REMOVE_CAR_FROM_MODE:
        {
            const AIModuleIO::RemoveCarFromCurrentModeEvent* const lpRemoveCar =
                static_cast<const AIModuleIO::RemoveCarFromCurrentModeEvent*>(lpEvent);

            AICar* const lpCar = GetAICar(static_cast<u32>(lpRemoveCar->meGlobalRaceCarIndex));
            if (lpCar == 0)
            {
                break;   // [GUARD]
            }

            lpCar->mfWrongWayTime            = 0.0f;                                        // 0x82798B8C
            InvalidateRouteInline(lpCar);                                                   // 0x82798B94/98
            lpCar->meRouteFindingStyle       = E_ROUTE_FINDING_FREE_ROAM;                   // 0x82798B9C
            lpCar->muDestinationSectionIndex = 0x7FFF;                                      // 0x82798BA0
            ++lpCar->miRouteTimeStamp;                                                      // 0x82798BA4
            lpCar->meSpeedSelectionMethod    = E_AI_SPEED_SELECTION_METHOD_FREE_ROAM;       // 0x82798BAC
            SetBehaviourInline(lpCar, E_AI_BEHAVIOUR_CRUISING);                             // 0x82798B90/BB0/BBC
            lpCar->mbIsInGameMode            = false;                                       // 0x82798BB4
            lpCar->mbUseAIShortcuts          = 0;                                           // 0x82798BB8
            lpCar->mbWantsAlternativeRoute   = 0;                                           // 0x82798BC0
            lpCar->mAggressiveness.SetAggression(0.0f);                                     // 0x82798BC4/BC8

            if (WitnessManagementEvent(liType))
            {
                *CgsDev::Log::gpDebugPrint
                    << "[ai-evt] REMOVE_CAR_FROM_MODE global "
                    << static_cast<s32>(lpRemoveCar->meGlobalRaceCarIndex)
                    << " -> CRUISING, route invalidated\n";
            }
            break;
        }

        // ---- default  (asm 0x82798E90) -----------------------------------------------------
        default:
            FireStreamedAssert("Unknown RaceCar->AI management event: ", liType, "");       // :1983
            break;
        }

        liType = lpManagementQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);               // 0x82798F44
    }
}

// =================================================================================================
// HandleGameActions @0x82791FD0   (DWARF BrnAIModule.cpp:1552)
//
// int __fastcall (r3 = this, r4 = lpInputBuffer, r5 = lpOutputBuffer, r6 = lpRouteInputBuffer).
// r5 IS NEVER READ by the console body -- it is carried for the prototype (the caller passes the
// AI output buffer at asm 0x8279B6FC). r6 is the transient RouteMapModuleIO "Route" input buffer
// that only action 50 touches; the caller brackets this call with LockBuffersForIO /
// UnlockBuffersForIO on it (asm 0x8279B6F0 / 0x8279B70C).
//
// Register map: r30 = this, r27 = the action pointer, r31 = lpRouteInputBuffer (reloaded from the
// stack every iteration -- asm 0x82792080), r29 = 0, r22 = 1, r14 = 3, f31 = 0.0f,
// r28 = 0x4E9FC (&mePlayerGlobalRaceCarIndex), r23 = 0x4EBDC (&mBuzzBy),
// r26 = 0x4EBE1 (&mBuzzBy.mbIsInJunkyard), r25 = 0x4ED08 (&mBuzzBy.miCarsAwaitingCollection),
// r19 = 0x4EB70 (&meDefaultPlayerRouteFindingStyle).
// =================================================================================================
void AIModule::HandleGameActions(const AIModuleIO::InputBuffer* lpInputBuffer,
                                 AIModuleIO::OutputBuffer* lpOutputBuffer,
                                 RouteMapModuleIO::InputBuffer* lpRouteInputBuffer)
{
    using namespace BrnGameState::GameStateModuleIO;

    (void)lpOutputBuffer;   // carried for the prototype; the console body never reads r5.

    if (lpInputBuffer == 0)
    {
        return;   // [GUARD] the console has no null test here at all (asm 0x82791FF0 calls
                  // GetGameActionQueue straight away) -- added because every other leg guards.
    }

    const AIModuleIO::InputBuffer::GameActionQueue* const lpGameActionQueue =
        lpInputBuffer->GetGameActionQueue();                                                // 0x8276D530
    if (lpGameActionQueue == 0)
    {
        return;   // [GUARD]
    }

    const CgsModule::Event* lpAction = 0;
    s32 liSize = 0;
    s32 liType = lpGameActionQueue->GetFirstEvent(&lpAction, &liSize);

    while (lpAction != 0)
    {
        switch (liType)
        {
        // ---- 7  E_ACTION_SET_PLAYER_CAR_DRIVER  (jump-table case 0, asm 0x82792538) --------
        // Only the DRIVE-THRU posts of this action reach the AI (`lbz 0x2C` == mbIsDriveThru).
        // When the drive-thru hands the car to the AI module the car runs the DRIVE_THRU
        // behaviour on a straight-line route; when it hands it back the car returns to CRUISING
        // and, if a mode is running, gets the player's default route style and a fresh route.
        case E_ACTION_SET_PLAYER_CAR_DRIVER:
        {
            const SetPlayerCarDriverAction* const lpSetDriver =
                reinterpret_cast<const SetPlayerCarDriverAction*>(lpAction);
            if (!lpSetDriver->mbIsDriveThru)                                                // 0x82792538
            {
                break;
            }

            AICar* const lpPlayerCar = GetAICar(static_cast<u32>(mePlayerGlobalRaceCarIndex));
            CGS_ASSERT(lpPlayerCar != 0, "lpPlayerCar");                                    // :1731
            if (lpPlayerCar == 0)
            {
                break;   // [GUARD] the console's assert is non-gating and it derefs next.
            }

            lpPlayerCar->mfBehaviourTimer = 0.0f;                                           // 0x82792578

            ERouteFindingStyle leRouteFindingStyle;
            if (lpSetDriver->meCarControl == BrnWorld::E_CAR_CONTROL_AI_MODULE)             // 0x8279257C
            {
                lpPlayerCar->mePreviousBehaviour = lpPlayerCar->meBehaviour;                 // 0x82792590
                ++lpPlayerCar->miRouteTimeStamp;                                             // 0x82792598
                lpPlayerCar->meBehaviour         = E_AI_BEHAVIOUR_DRIVE_THRU;                // 0x827925A0
                leRouteFindingStyle              = E_ROUTE_FINDING_ALWAYS_STRAIGHT;          // `li r11, 5`
            }
            else
            {
                lpPlayerCar->mePreviousBehaviour = lpPlayerCar->meBehaviour;                 // 0x827925B8
                lpPlayerCar->meBehaviour         = E_AI_BEHAVIOUR_CRUISING;                  // 0x827925B0
                if (!mbIsInGameMode)                                                          // 0x827925BC
                {
                    break;   // no mode running: the console leaves the rest alone.
                }
                lpPlayerCar->mfWrongWayTime = 0.0f;                                           // 0x827925CC
                InvalidateRouteInline(lpPlayerCar);                                           // 0x827925D0/D8
                ++lpPlayerCar->miRouteTimeStamp;                                              // 0x827925E0
                leRouteFindingStyle = meDefaultPlayerRouteFindingStyle;                       // 0x827925DC
            }

            lpPlayerCar->mfWrongWayTime      = 0.0f;                                          // 0x827925E4
            lpPlayerCar->meRouteFindingStyle = leRouteFindingStyle;                           // 0x827925E8
            InvalidateRouteInline(lpPlayerCar);                                               // 0x827925EC/F0

            if (WitnessGameAction(liType))
            {
                *CgsDev::Log::gpDebugPrint
                    << "[ai-evt] action 7 SET_PLAYER_CAR_DRIVER control "
                    << static_cast<s32>(lpSetDriver->meCarControl)
                    << " -> player car behaviour " << static_cast<s32>(lpPlayerCar->meBehaviour)
                    << ", style " << static_cast<s32>(leRouteFindingStyle) << "\n";
            }
            break;
        }

        // ---- 14  E_ACTION_ON_PLAYER_TAKEDOWN  (jump-table case 7, asm 0x82792448) ----------
        case E_ACTION_ON_PLAYER_TAKEDOWN:
            OnPlayerTakedown(reinterpret_cast<const OnPlayerTakedownAction*>(lpAction));
            if (WitnessGameAction(liType))
            {
                *CgsDev::Log::gpDebugPrint << "[ai-evt] action 14 ON_PLAYER_TAKEDOWN\n";
            }
            break;

        // ---- 23  E_ACTION_PREPARE_FOR_MODE  (jump-table case 16, asm 0x8279230C) -----------
        // `addi r4, r27, 0x30` -- the embedded GameModeParams at action+0x30
        // (BrnGameActions.h: PrepareForModeAction::mGameModeParams, X360 +0x0030).
        case E_ACTION_PREPARE_FOR_MODE:
        {
            const PrepareForModeAction* const lpPrepare =
                reinterpret_cast<const PrepareForModeAction*>(lpAction);
            OnModeStart(lpPrepare->GetGameModeParams());
            if (WitnessGameAction(liType))
            {
                *CgsDev::Log::gpDebugPrint
                    << "[ai-evt] action 23 PREPARE_FOR_MODE -> OnModeStart (inGameMode "
                    << static_cast<s32>(mbIsInGameMode) << ")\n";
            }
            break;
        }

        // ---- 30  E_ACTION_STOP_MODE_INTRO  (jump-table case 23, asm 0x827922BC) ------------
        // A donut start swallows the intro-stop entirely; a full rolling start jumps straight to
        // racing; otherwise the cars roll off the line.
        case E_ACTION_STOP_MODE_INTRO:
            if (!mbDonutStart)                                                              // 0x827922C4
            {
                if (mbFullRollingStart)                                                      // 0x827922DC
                {
                    OnModeStartRacing(true);                                                 // `li r4, 1`
                }
                else
                {
                    OnRollingStart();
                }
            }
            if (WitnessGameAction(liType))
            {
                *CgsDev::Log::gpDebugPrint
                    << "[ai-evt] action 30 STOP_MODE_INTRO (donut " << static_cast<s32>(mbDonutStart)
                    << ", fullRolling " << static_cast<s32>(mbFullRollingStart) << ")\n";
            }
            break;

        // ---- 34  E_ACTION_START_PLAYING_MODE  (jump-table case 27, asm 0x8279231C) ---------
        case E_ACTION_START_PLAYING_MODE:
            OnModeStartRacing(false);                                                        // `li r4, 0`
            if (WitnessGameAction(liType))
            {
                *CgsDev::Log::gpDebugPrint << "[ai-evt] action 34 START_PLAYING_MODE\n";
            }
            break;

        // ---- 35  E_ACTION_FINISHED_MODE_NOTIFY  (jump-table case 28, asm 0x827922FC) -------
        case E_ACTION_FINISHED_MODE_NOTIFY:
            OnModeFinished(reinterpret_cast<const FinishedModeNotifyAction*>(lpAction));
            if (WitnessGameAction(liType))
            {
                *CgsDev::Log::gpDebugPrint << "[ai-evt] action 35 FINISHED_MODE_NOTIFY\n";
            }
            break;

        // ---- 39  E_ACTION_STOP_MODE  (jump-table case 32, asm 0x8279232C) ------------------
        // asm 0x8279232C..0x82792374: OnModeEnd's argument is the NEGATION of
        // (isOnline && !field11 && !lastRound && !field13) -- i.e. an online round that is not the
        // last one does NOT restore the module's driving-input/online flags.
        case E_ACTION_STOP_MODE:
        {
            const StopModeAction* const lpStopMode = reinterpret_cast<const StopModeAction*>(lpAction);
            const bool lbHoldModeState = (lpStopMode->mu8Field10 != 0) &&
                                         (lpStopMode->mu8Field12 == 0) &&
                                         (lpStopMode->mu8Field11 == 0) &&
                                         (lpStopMode->mu8Field13 == 0);
            OnModeEnd(!lbHoldModeState);
            if (WitnessGameAction(liType))
            {
                *CgsDev::Log::gpDebugPrint
                    << "[ai-evt] action 39 STOP_MODE -> OnModeEnd(" << static_cast<s32>(!lbHoldModeState)
                    << ")\n";
            }
            break;
        }

        // ---- 50  E_ACTION_REQUEST_ROUTE_INFO  (jump-table case 43, asm 0x827923A4) ---------
        // Forward somebody else's route question into the transient "Route" input buffer as a
        // RaceRouteRequest. The record's own owner/event ids are carried through -- unlike every
        // AI-side builder, which stamps muOwnerId = E_OWNER_AI.
        case E_ACTION_REQUEST_ROUTE_INFO:
        {
            CGS_ASSERT(lpRouteInputBuffer != 0, "lpRouteInputBuffer");                      // :1644
            CGS_ASSERT(lpAction != 0, "lpRouteRequest");                                    // :1649
            if (lpRouteInputBuffer == 0)
            {
                break;   // [GUARD] the console's assert is non-gating and it derefs at 0x82792438.
            }

            const RequestRouteInfoAction* const lpRouteInfo =
                reinterpret_cast<const RequestRouteInfoAction*>(lpAction);

            RouteMapModuleIO::RaceRouteRequest lRequest;
            lRequest.mStartPosition      = lpRouteInfo->mStartPosition;                     // 0x827923E8/0x82792400
            lRequest.mEndPosition        = lpRouteInfo->mEndPosition;                       // 0x82792424/0x8279242C
            lRequest.muStartSectionIndex = lpRouteInfo->muStartSectionIndex;                // 0x827923E4/0x827923F8
            lRequest.muEndSectionIndex   = lpRouteInfo->muEndSectionIndex;                  // 0x8279240C/0x82792420
            lRequest.mauBlockSections.Construct();                                          // 0x82792434 (count word = 0)
            lRequest.muOwnerId           = static_cast<u16>(lpRouteInfo->miOwnerId);        // 0x827923EC/0x82792404 (lwz -> sth)
            lRequest.muEventId           = lpRouteInfo->muEventId;                          // 0x82792408/0x82792418
            lRequest.meQuality           = E_ASTAR_QUALITY_LOW;                             // 0x82792410 (stw 0)
            lRequest.meDistanceFunction  = E_ASTAR_DISTANCE_EUCLIDEAN;                      // 0x82792414 (stw 0)
            lRequest.mbUseAIShortcuts    = false;                                           // 0x82792430 (stb 0)

            lpRouteInputBuffer->GetRaceRouteRequestQueue()->AddEventSafe(lRequest);         // 0x82792438/0x82792440

            if (WitnessGameAction(liType))
            {
                *CgsDev::Log::gpDebugPrint
                    << "[ai-evt] action 50 REQUEST_ROUTE_INFO owner " << lpRouteInfo->miOwnerId
                    << " event " << static_cast<s32>(lpRouteInfo->muEventId)
                    << " sections " << static_cast<s32>(lpRouteInfo->muStartSectionIndex)
                    << " -> " << static_cast<s32>(lpRouteInfo->muEndSectionIndex) << "\n";
            }
            break;
        }

        // ---- 99  E_ACTION_DRIVE_THRU_JUNK_YARD  (jump-table case 92, asm 0x82792504) -------
        case E_ACTION_DRIVE_THRU_JUNK_YARD:
        {
            const DriveThruJunkYardAction* const lpJunkYard =
                reinterpret_cast<const DriveThruJunkYardAction*>(lpAction);

            mBuzzBy.SetInJunkyard(lpJunkYard->mbIsInJunkYard);                              // 0x82792510/0x82792518

            AICar* const lpPlayerCar = GetAICar(static_cast<u32>(mePlayerGlobalRaceCarIndex));
            if (lpPlayerCar != 0)
            {
                lpPlayerCar->SetIsInJunkyard(lpJunkYard->mbIsInJunkYard);                   // 0x8279252C
            }
            mBuzzBy.ClearCarsAwaitingCollection();                                          // 0x82792530

            if (WitnessGameAction(liType))
            {
                *CgsDev::Log::gpDebugPrint
                    << "[ai-evt] action 99 DRIVE_THRU_JUNK_YARD in "
                    << static_cast<s32>(lpJunkYard->mbIsInJunkYard ? 1 : 0) << "\n";
            }
            break;
        }

        // ---- 106  E_ACTION_DRIVE_THRU_JUNK_YARD_ON_GAME_START (case 99, asm 0x827924C8) ---
        case E_ACTION_DRIVE_THRU_JUNK_YARD_ON_GAME_START:
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint << "<AI> Is in startup junkyard\n";              // 0x827924E0
            }

            AICar* const lpPlayerCar = GetAICar(static_cast<u32>(mePlayerGlobalRaceCarIndex));
            if (lpPlayerCar != 0)
            {
                lpPlayerCar->SetIsInJunkyard(true);                                         // 0x827924F4
            }
            mBuzzBy.ClearCarsAwaitingCollection();                                          // 0x827924F8
            mBuzzBy.SetInJunkyard(true);                                                    // 0x827924FC

            if (WitnessGameAction(liType))
            {
                *CgsDev::Log::gpDebugPrint << "[ai-evt] action 106 JUNK_YARD_ON_GAME_START\n";
            }
            break;
        }

        // ---- 113  E_ACTION_RACE_CAR_REACHED_CHECKPOINT  (case 106, asm 0x827922AC) --------
        case E_ACTION_RACE_CAR_REACHED_CHECKPOINT:
            OnRaceCarReachedCheckpoint(reinterpret_cast<const RaceCarReachedCheckpointAction*>(lpAction));
            if (WitnessGameAction(liType))
            {
                *CgsDev::Log::gpDebugPrint << "[ai-evt] action 113 RACE_CAR_REACHED_CHECKPOINT\n";
            }
            break;

        // ---- 114  E_ACTION_RACE_CAR_REACHED_FINISH  (case 107, asm 0x8279229C) ------------
        case E_ACTION_RACE_CAR_REACHED_FINISH:
            OnRaceCarReachedFinish(reinterpret_cast<const RaceCarReachedFinishAction*>(lpAction));
            if (WitnessGameAction(liType))
            {
                *CgsDev::Log::gpDebugPrint << "[ai-evt] action 114 RACE_CAR_REACHED_FINISH\n";
            }
            break;

        // ---- 120  E_ACTION_SHUTDOWN  (jump-table case 113, asm 0x82792458) ----------------
        // The player car stops dead and one more car is queued for BuzzBy collection.
        case E_ACTION_SHUTDOWN:
        {
            AICar* const lpPlayerCar = GetAICar(static_cast<u32>(mePlayerGlobalRaceCarIndex));
            if (lpPlayerCar == 0)
            {
                break;   // [GUARD]
            }
            SetBehaviourInline(lpPlayerCar, E_AI_BEHAVIOUR_STOP);                           // 0x82792464..0x82792474
            mBuzzBy.AddCarAwaitingCollection();                                             // 0x82792478..0x82792480
            if (WitnessGameAction(liType))
            {
                *CgsDev::Log::gpDebugPrint << "[ai-evt] action 120 SHUTDOWN -> player car STOP\n";
            }
            break;
        }

        // ---- 122  E_ACTION_AWARD_SEQUENCE_START  (case 115, asm 0x82792488) ---------------
        case E_ACTION_AWARD_SEQUENCE_START:
        {
            AICar* const lpPlayerCar = GetAICar(static_cast<u32>(mePlayerGlobalRaceCarIndex));
            if (lpPlayerCar == 0)
            {
                break;   // [GUARD]
            }
            SetBehaviourInline(lpPlayerCar, E_AI_BEHAVIOUR_STOP);                           // 0x82792494..0x827924A0
            if (WitnessGameAction(liType))
            {
                *CgsDev::Log::gpDebugPrint << "[ai-evt] action 122 AWARD_SEQUENCE_START -> STOP\n";
            }
            break;
        }

        // ---- 123  E_ACTION_AWARD_SEQUENCE_END  (case 116, asm 0x827924A8) -----------------
        case E_ACTION_AWARD_SEQUENCE_END:
        {
            AICar* const lpPlayerCar = GetAICar(static_cast<u32>(mePlayerGlobalRaceCarIndex));
            if (lpPlayerCar == 0)
            {
                break;   // [GUARD]
            }
            SetBehaviourInline(lpPlayerCar, E_AI_BEHAVIOUR_CRUISING);                       // `stw r14(3), 0x14B4`
            if (WitnessGameAction(liType))
            {
                *CgsDev::Log::gpDebugPrint << "[ai-evt] action 123 AWARD_SEQUENCE_END -> CRUISING\n";
            }
            break;
        }

        // ---- 131  E_ACTION_UPDATE_ROAD_RAGE_MADNESS  (case 124, asm 0x8279237C) -----------
        case E_ACTION_UPDATE_ROAD_RAGE_MADNESS:
        {
            const UpdateRoadRageMadnessAction* const lpMadness =
                reinterpret_cast<const UpdateRoadRageMadnessAction*>(lpAction);

            AIDriver* const lpDriver = GetAIDriver(lpMadness->meActiveRaceCarIndex);
            if (lpDriver == 0 || !lpDriver->IsActive())                                     // 0x82792388
            {
                break;
            }
            AICar* const lpCar = lpDriver->GetCar();                                        // 0x82792398
            if (lpCar == 0)
            {
                break;   // [GUARD] the console derefs mpCar unconditionally once mbIsActive is set.
            }
            lpCar->SetRoadRageMadness(lpMadness->mfMadness);                                // 0x8279239C

            if (WitnessGameAction(liType))
            {
                *CgsDev::Log::gpDebugPrint
                    << "[ai-evt] action 131 UPDATE_ROAD_RAGE_MADNESS slot "
                    << static_cast<s32>(lpMadness->meActiveRaceCarIndex) << "\n";
            }
            break;
        }

        default:
            break;   // the console's default arm is the loop tail; 109 of the 125 slots land here.
        }

        liType = lpGameActionQueue->GetNextEvent(lpAction, &lpAction, &liSize);              // 0x82792604
    }
}

// =================================================================================================
// OnRollingStart @0x8276E5C8   (DWARF BrnAIModule.cpp:993)
//
// Every car that is in the mode starts rolling. The loop is the console's `do { } while` over all
// 35 global slots with the BurnoutConstants.h:84 post-increment guard (reproduced by
// `operator++(EGlobalRaceCarIndex&, int)`), and the body is the inlined SetBehaviour.
// =================================================================================================
void AIModule::OnRollingStart()
{
    for (EGlobalRaceCarIndex leIndex = E_GLOBAL_RACE_CAR_INDEX_0;
         leIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT;
         leIndex++)
    {
        AICar* const lpCar = GetAICar(static_cast<u32>(leIndex));
        if (lpCar == 0)
        {
            continue;   // [GUARD]
        }
        if (lpCar->mbIsInGameMode)                                                          // car+0x154B
        {
            SetBehaviourInline(lpCar, E_AI_BEHAVIOUR_ROLLING_START);
        }
    }
}

// =================================================================================================
// OnModeStartRacing @0x8276E4B0   (DWARF BrnAIModule.cpp:953)
//
// `lbSkipPlayerCar` (r4) is set by the FULL-ROLLING-START path (HandleGameActions action 30) and
// clear by the normal start (action 34). Every in-mode, ACTIVE car that is not being skipped gets
// AICar::OnModeStartRacing; the master route is then rebuilt from the player's car unless the mode
// is online; and both start-mechanism flags are consumed.
//
// asm 0x8276E4C4 `*(this + 270921) = 0` -- a byte inside the AIDebugComponent block that neither
// this file nor BrnAIModule.h has a named member for; see the park below.
// =================================================================================================
void AIModule::OnModeStartRacing(bool lbSkipPlayerCar)
{
    // [FLAG PC bring-up] AIModule::OnModeStartRacing @0x8276E4C4 clears the byte at this+270921.
    // 270921 lands in the AIDebugComponent block (DWARF BrnAIModule.h:74 `AIDebugComponent
    // mAIDebugComponent`) that AIModule::Update's own debug arm reads at +270920/+270921 and this
    // host class does not declare -- the whole component is absent, so there is nothing to clear.
    // Drop-safe: its only reader is the debug time accumulator, which is also absent.
    // DELETE-WHEN AIModule grows a named mAIDebugComponent member.

    for (EGlobalRaceCarIndex leIndex = E_GLOBAL_RACE_CAR_INDEX_0;
         leIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT;
         leIndex++)
    {
        AICar* const lpCar = GetAICar(static_cast<u32>(leIndex));
        if (lpCar == 0)
        {
            continue;   // [GUARD]
        }
        // asm 0x8276E4E8: skip the player's own car when lbSkipPlayerCar is set.
        if (lbSkipPlayerCar && lpCar->mbIsPlayer)
        {
            continue;
        }
        // asm 0x8276E4F8..0x8276E510: IsActive() && mbIsInGameMode.
        if (lpCar->IsActive() && lpCar->mbIsInGameMode)
        {
            lpCar->OnModeStartRacing();                                                     // 0x82765C50
        }
    }

    // asm 0x8276E55C..0x8276E578: offline only -- rebuild the master route from the player's car.
    // `Route::Construct(this + 289632, GetAICar(mePlayerGlobalRaceCarIndex))` -- the console hands
    // the AICar POINTER straight in as the source Route because mRoute sits at AICar+0.
    if (!mbIsInOnlineGameMode)
    {
        AICar* const lpPlayerCar = GetAICar(static_cast<u32>(mePlayerGlobalRaceCarIndex));
        if (lpPlayerCar != 0)
        {
            mMasterRoute.Construct(*lpPlayerCar->GetRoute());
        }
    }

    mbFullRollingStart = false;                                                             // 0x8276E584
    mbDonutStart       = false;                                                             // 0x8276E58C
}

// =================================================================================================
// OnModeFinished @0x8277B970   (DWARF BrnAIModule.cpp:859)
//
// The mode is over. Every ACTIVE car has its route invalidated; the player-flagged cars also get
// the post-race behaviour the notify record's single byte selects (won -> POST_RACE_WIN(9),
// lost -> POST_RACE_LOSE(10)) plus a route-style/method reset and a fresh route timestamp.
//
// NOTE the console's gate at 0x8277B9AC is `mbIsPlayer` (car+0x1549), NOT mbIsInGameMode --
// the outer arm runs for every ACTIVE car, the inner one only for the player's.
// =================================================================================================
void AIModule::OnModeFinished(const BrnGameState::GameStateModuleIO::FinishedModeNotifyAction* lpAction)
{
    for (EGlobalRaceCarIndex leIndex = E_GLOBAL_RACE_CAR_INDEX_0;
         leIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT;
         leIndex++)
    {
        AICar* const lpCar = GetAICar(static_cast<u32>(leIndex));
        if (lpCar == 0)
        {
            continue;   // [GUARD]
        }
        if (!lpCar->IsActive())                                                             // 0x8277B994..0x8277B9A8
        {
            continue;
        }

        lpCar->mfWrongWayTime = 0.0f;                                                       // 0x8277B9C8
        InvalidateRouteInline(lpCar);                                                       // 0x8277B9CC/D0

        if (!lpCar->mbIsPlayer)                                                             // 0x8277B9AC
        {
            continue;
        }

        SetBehaviourInline(lpCar,
                           (lpAction != 0 && lpAction->mbPlayerWon) ? E_AI_BEHAVIOUR_POST_RACE_WIN
                                                                   : E_AI_BEHAVIOUR_POST_RACE_LOSE);
        lpCar->mfWrongWayTime = 0.0f;                                                       // 0x8277BA00
        InvalidateRouteInline(lpCar);                                                       // 0x8277BA04/08
        lpCar->meRouteFindingStyle    = E_ROUTE_FINDING_FREE_ROAM;                           // 0x8277BA0C (stw 0, 0x14C0)
        lpCar->meSpeedSelectionMethod = E_AI_SPEED_SELECTION_METHOD_FREE_ROAM;               // 0x8277BA10 (stw 0, 0x14BC)
        ++lpCar->miRouteTimeStamp;                                                           // 0x8277BA18
    }
}

// =================================================================================================
// OnRaceCarReachedFinish @0x8277B8D0   (DWARF BrnAIModule.cpp:821)
//
// One car crossed the line. Its route is invalidated; a NON-player finisher that was racing is
// switched onto an "away from the action" style -- AVOID_PLAYER(4) when the player is still
// racing, ALWAYS_STRAIGHT(5) when the player is not -- and its speed-selection method goes back to
// free-roam. A player finisher only gets the route invalidation.
// =================================================================================================
void AIModule::OnRaceCarReachedFinish(
        const BrnGameState::GameStateModuleIO::RaceCarReachedFinishAction* lpAction)
{
    if (lpAction == 0)
    {
        return;   // [GUARD]
    }

    AICar* const lpPlayerCar = GetAICar(static_cast<u32>(mePlayerGlobalRaceCarIndex));       // 0x8277B8E4
    AICar* const lpCar       = GetAICar(static_cast<u32>(lpAction->meGlobalRaceCarIndex));   // 0x8277B8F4
    if (lpCar == 0)
    {
        return;   // [GUARD]
    }

    const bool lbIsPlayer = lpCar->mbIsPlayer;                                               // 0x8277B8FC
    lpCar->mfWrongWayTime = 0.0f;                                                            // 0x8277B908
    InvalidateRouteInline(lpCar);                                                            // 0x8277B90C/10

    if (lbIsPlayer)
    {
        return;
    }

    if (lpCar->meRouteFindingStyle == E_ROUTE_FINDING_RACE)                                  // 0x8277B920
    {
        const bool lbPlayerStillRacing =
            (lpPlayerCar != 0 && lpPlayerCar->meRouteFindingStyle == E_ROUTE_FINDING_RACE);  // 0x8277B92C
        lpCar->mfWrongWayTime = 0.0f;                                                        // 0x8277B93C
        InvalidateRouteInline(lpCar);                                                        // 0x8277B940/4C
        ++lpCar->miRouteTimeStamp;                                                           // 0x8277B950
        lpCar->meRouteFindingStyle =
            lbPlayerStillRacing ? E_ROUTE_FINDING_AVOID_PLAYER      // `li r8, 4`
                                : E_ROUTE_FINDING_ALWAYS_STRAIGHT;  // `li r8, 5`
    }
    lpCar->meSpeedSelectionMethod = E_AI_SPEED_SELECTION_METHOD_FREE_ROAM;                    // 0x8277B95C
}

// =================================================================================================
// OnModeStart @0x82791DB8   (DWARF BrnAIModule.cpp:903)
//
// Latch the mode's AI configuration out of its GameModeParams. Reproduced here except for two
// blocks that need members this host class does not have -- both are NAMED PARKS below, and
// neither is on the activation path.
// =================================================================================================
void AIModule::OnModeStart(const BrnGameState::GameModeParams* lpGameModeParams)
{
    CGS_ASSERT(lpGameModeParams != 0, "lpGameModeParams != NULL");                            // :911
    if (lpGameModeParams == 0)
    {
        return;   // [GUARD] the console reads params+0x94 two instructions later.
    }

    // [FLAG PC bring-up] AIModule::OnModeStart @0x82791DF4..0x82791E38 also derives
    //   mbEnableDrivingInput  = (*(u8*)(lpGameModeParams + 0x94) == 0)
    //   mbIsInOnlineGameMode  =  *(u8*)(lpGameModeParams + 0x94)
    // and +0x94 has NO pinned member name: BrnGameModeParams.h states outright that
    // GameModeParams' real layout is NOT its DWARF source order (see the meGameModeType banner
    // there, pinned to +0x148 by the asm), and walking the declared order from the one other
    // anchor this tree has (maePlayerTeam[8] @ +0x118, BrnOnlineStuntRunMode.cpp:297) puts no
    // 1-byte member at +0x94. Semantically it can only be `mbIsOnline`, but "can only be" is not
    // a pin, so the two flags are LEFT AT THEIR CONSTRUCT VALUES (mbEnableDrivingInput = 1,
    // mbIsInOnlineGameMode = 0) instead of being written from a guessed offset. That IS the
    // console's outcome for every OFFLINE mode -- the byte is 0 there, so the console writes
    // exactly those two values. An ONLINE mode diverges (its AI would keep driving input enabled).
    // DELETE-WHEN GameModeParams' offset -> member map pins +0x94.

    mbIsInGameMode           = true;                                                          // 0x82791E34 (stb 1, 0x4EB7E)
    mfProgressionRankAsRatio = lpGameModeParams->mfProgressionRankAsRatio;                    // 0x82791E3C/40 (lfs 4)

    SetupRaceBalancingManager(lpGameModeParams);                                              // 0x82791E44

    // [FLAG PC bring-up] asm 0x82791E64 stores the mode's meAStarDistanceFunction into
    // this+271528 == mRouteRequestManager.meDefaultAStarDistanceFunction (the manager is at
    // console +270952 and its default heuristic at +0x240), and the loop at 0x82791E90..0x82791F14
    // clears + refills mRouteRequestManager.mauBlockSectionIds[checkpoint] from each
    // CheckpointData's own block-section array. AIModule has NO named mRouteRequestManager member
    // on this host (DWARF BrnAIModule.h:71; lane A1 flagged the same gap for AIModule::Update row
    // #22) and RouteRequestManager exposes no public "clear/append this checkpoint's block
    // sections" API, so BOTH are parked. Consequence: standard race routes are built with the
    // manager's own default heuristic and with NO blocked sections, i.e. U-turn blocking through
    // checkpoints is off. Not on the activation path.
    // DELETE-WHEN BrnAIModule.h grows `RouteRequestManager mRouteRequestManager;` and
    // BrnRouteRequestManager.h grows the per-checkpoint block-section setter.

    muNumAggressiveCars              = static_cast<u8>(lpGameModeParams->GetAIAggressiveCarCount());  // 0x82791F38 (lbz 0x84C)
    meDefaultPlayerRouteFindingStyle = static_cast<ERouteFindingStyle>(
                                           lpGameModeParams->GetDefaultPlayerRouteFindingStyle());   // 0x82791F44 (0x840)
    meDefaultAIRouteFindingStyle     = static_cast<ERouteFindingStyle>(
                                           lpGameModeParams->GetDefaultAIRouteFindingStyle());       // 0x82791F4C (0x844)
    meSpeedSelectionMethod           = static_cast<EAISpeedSelectionMethod>(
                                           lpGameModeParams->GetAISpeedSelectionMethod());           // 0x82791F54 (0x848)

    // asm 0x82791F58..0x82791FB0: two bits out of the mode's flag word.
    mbFullRollingStart = lpGameModeParams->GetFlag(0x04000000u);                              // rlwinm 0,5,5
    mbDonutStart       = lpGameModeParams->GetFlag(0x08000000u);                              // rlwinm 0,4,4

    // asm 0x82791FB4..0x82791FBC: the master route is dropped (all three head words).
    mMasterRoute.miNodeCount       = 0;                                                       // +0x1400
    mMasterRoute.miDefaultStartNode = 0;                                                      // +0x1404
    mMasterRoute.meStatus          = Route::E_STATUS_UNINITIALISED;                           // +0x1408

    mBuzzBy.SetInGameMode(true);                                                              // 0x82791FC0 (stb 1, 0x4EBE0)
}

// =================================================================================================
// OnModeEnd @0x8277BA80   (DWARF BrnAIModule.cpp:1016)
//
// The mirror of OnModeStart. `lbRestoreDrivingInput` is HandleGameActions action 39's computed
// argument (see there); it is clear only for a non-final online round.
// =================================================================================================
void AIModule::OnModeEnd(bool lbRestoreDrivingInput)
{
    // [FLAG PC bring-up] the console's first store (asm 0x8277BA90, this+271528 = 0) resets
    // mRouteRequestManager.meDefaultAStarDistanceFunction, the loop at 0x8277BBF0..0x8277BC04
    // clears all 16 mRouteRequestManager per-checkpoint block-section counts, and 0x8277BBD8..
    // 0x8277BBE8 zero this+270908 / +270916 / +270920 (the AIDebugComponent block) and
    // this+252816 (a word inside mRaceBalancingManager with no named member). None of those four
    // members exists on this host class -- same gap as OnModeStart's park above.
    // DELETE-WHEN BrnAIModule.h grows mRouteRequestManager / mAIDebugComponent and
    // RaceBalancingManager exposes the +448 word by name.

    if (lbRestoreDrivingInput)                                                                // 0x8277BA9C
    {
        mbEnableDrivingInput = true;                                                          // 0x8277BAA8
        mbIsInOnlineGameMode = false;                                                         // 0x8277BAB0
    }

    muNumAggressiveCars = 3;                                                                  // 0x8277BAB8 (li 3)

    for (EGlobalRaceCarIndex leIndex = E_GLOBAL_RACE_CAR_INDEX_0;
         leIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT;
         leIndex++)
    {
        AICar* const lpCar = GetAICar(static_cast<u32>(leIndex));
        if (lpCar == 0)
        {
            continue;   // [GUARD]
        }
        if (!lpCar->mbIsInGameMode)                                                           // car+0x154B
        {
            continue;
        }

        lpCar->mfWrongWayTime            = 0.0f;                                              // car+0x14F4
        lpCar->muDestinationSectionIndex = 0x7FFF;                                            // car+0x1536
        InvalidateRouteInline(lpCar);                                                         // car+0x1408 / +0x1400
        lpCar->meRouteFindingStyle       = E_ROUTE_FINDING_FREE_ROAM;                          // car+0x14C0
        lpCar->meSpeedSelectionMethod    = E_AI_SPEED_SELECTION_METHOD_FREE_ROAM;              // car+0x14BC
        ++lpCar->miRouteTimeStamp;                                                             // car+0x1528
        SetBehaviourInline(lpCar, E_AI_BEHAVIOUR_CRUISING);                                     // car+0x14E0/B4/B8
        lpCar->mbIsInGameMode            = false;                                               // car+0x154B
        lpCar->mbUseAIShortcuts          = 0;                                                   // car+0x153E
        lpCar->mbWantsAlternativeRoute   = 0;                                                   // car+0x153F
        lpCar->mAggressiveness.SetAggression(0.0f);                                             // car+0x140C/0x1410
    }

    meSpeedSelectionMethod = E_AI_SPEED_SELECTION_METHOD_FREE_ROAM;                            // 0x8277BBC8 (stw 0, 0x4EB6C)

    mMasterRoute.miNodeCount = 0;                                                              // 0x8277BC10 (+0x1400)
    mMasterRoute.meStatus    = Route::E_STATUS_UNINITIALISED;                                  // 0x8277BC0C (+0x1408)

    mBuzzBy.SetInGameMode(false);                                                              // 0x8277BC1C (stb 0, 0x4EBE0)
    mbIsInGameMode = false;                                                                    // 0x8277BC24 (stb 0, 0x4EB7E)
}

// =================================================================================================
// SetupRaceBalancingManager @0x8278A460   (DWARF BrnAIModule.cpp:202)   -- NAMED PARK
// =================================================================================================
void AIModule::SetupRaceBalancingManager(const BrnGameState::GameModeParams* lpGameModeParams)
{
    // [FLAG PC bring-up] BrnAI::AIModule::SetupRaceBalancingManager @0x8278A460 (125 insns) is
    // parked. The console body, recovered in full so this is cheap to land later:
    //     if (!lpGameModeParams->GetFlag(0x8000))        return;      // `lbz 0x219 ; andi 0x8000`
    //     Array<RaceBalancingGraph,7> laGraphs;          // 448 bytes on the caller's stack
    //     for (i = 0; i < lpGameModeParams->GetOpponentCount(); ++i)
    //     {
    //         const OpponentData* lpOpponent = lpGameModeParams->GetOpponentData(i);
    //         RaceBalancingGraph lGraph;                 // 32 bytes; zeroed as 8x2 words
    //         for (p = 0; p < 8; ++p)                    // KI_GRAPH_POINTS, BrnRaceBalance.h:144/:152
    //         {
    //             lGraph.SetX(p, lpOpponent-><+0x1C + 4*p>);
    //             lGraph.SetY(p, lpOpponent-><+0x3C + 4*p>);
    //         }
    //         laGraphs.Append(lGraph);                   // BrnAI::RaceBalancingGraph,7>::Append @0x8276A238
    //     }
    //     mRaceBalancingManager.OnRaceStart(laGraphs, lpGameModeParams->GetCheckpointCount(),
    //                                       *(u8*)(this + 322433));   // @0x82789AF8
    // Three blockers, none of them this lane's file: RaceBalancingGraph's two per-point setters are
    // not published, OpponentData's two 8-float graph arrays (+0x1C / +0x3C) are not named, and the
    // module's +322433 byte is DELIBERATELY UNDECLARED in BrnAIModule.h (see the comment there --
    // it is an X360-only bool the PS3 DWARF does not have, so it has no name to give it), which is
    // OnRaceStart's third argument.
    // Consequence: the race-balancing rubber-band never gets its per-opponent difficulty curve, so
    // rivals run at their unbalanced pace. NOT on the activation path.
    // DELETE-WHEN RaceBalancingGraph publishes its point setters, OpponentData names its graph
    // arrays, and BrnAIModule.h names +322433.
    (void)lpGameModeParams;

    static bool sbWitnessed = false;
    if (!sbWitnessed && CgsDev::Log::gpDebugPrint != 0)
    {
        sbWitnessed = true;
        *CgsDev::Log::gpDebugPrint
            << "[ai-evt] SetupRaceBalancingManager is PARKED -- rivals get no per-opponent"
               " race-balancing curve\n";
    }
}

// =================================================================================================
// OnPlayerTakedown @0x8278A720   (DWARF BrnAIModule.cpp:884)   -- NAMED PARK
// =================================================================================================
void AIModule::OnPlayerTakedown(
        const BrnGameState::GameStateModuleIO::OnPlayerTakedownAction* lpAction)
{
    // [FLAG PC bring-up] BrnAI::AIModule::OnPlayerTakedown @0x8278A720 -- the RACE-BALANCING half
    // is unreachable from this host class. The console body is:
    //     lpCar = GetAICar(lpAction->meVictimGlobalRaceCarIndex);
    //     liOpponent = lpCar->miOpponentIndex;                       (car+0x153A)
    //     if (liOpponent != 0xFF && !lpCar->mbIsPlayer                (car+0x1549)
    //         && *(u8*)(this + 270920))                               (the AIDebugComponent block)
    //         ++Array<RaceBalancingRoute,7>::GetItem(this + 252820, liOpponent)->+0xA04;
    // Two of the three seats have no named member here: this+270920 (DWARF BrnAIModule.h:74
    // `AIDebugComponent mAIDebugComponent`, which this class does not declare) and
    // this+252820 == mRaceBalancingManager + 452, whose RaceBalancingRoute array
    // (BrnAI::RaceBalancingRoute,7>::GetItem @0x8276A7F8) is private with no public accessor,
    // as is the per-route +0xA04 takedown counter it bumps.
    // The park is SAFE for this wave: the counter feeds the race-balancing rubber-band only, and
    // nothing on the activation path reads it. Doing it by offset would be exactly the
    // `*(T*)(p+N)` hack the faithfulness gate exists to stop.
    // DELETE-WHEN BrnAIModule.h grows a named mAIDebugComponent and RaceBalancingManager exposes
    // its RaceBalancingRoute array + that route's takedown counter by name.
    (void)lpAction;

    static bool sbWitnessed = false;
    if (!sbWitnessed && CgsDev::Log::gpDebugPrint != 0)
    {
        sbWitnessed = true;
        *CgsDev::Log::gpDebugPrint
            << "[ai-evt] OnPlayerTakedown is PARKED (race-balancing takedown counter has no named"
               " home) -- rubber-banding will not react to player takedowns\n";
    }
}

// =================================================================================================
// OnRaceCarReachedCheckpoint @0x8278A658   (DWARF BrnAIModule.cpp:835)   -- NAMED PARK
// =================================================================================================
void AIModule::OnRaceCarReachedCheckpoint(
        const BrnGameState::GameStateModuleIO::RaceCarReachedCheckpointAction* lpAction)
{
    // [FLAG PC bring-up] BrnAI::AIModule::OnRaceCarReachedCheckpoint @0x8278A658 -- there is NO
    // .ida-exports/BURNOUT_X360_ARTIST.XEX/0x8278A658.json and the symbol is absent from the
    // 30,094-row name index; the address is known only through HandleGameActions' xrefs_from.
    // With neither asm nor pseudocode there is nothing to reconstruct, and the DWARF gives only
    // the signature. The sibling that IS exported -- RaceBalancingManager::
    // OnOpponentReachedCheckpoint @0x82789D88 -- is what this almost certainly forwards to, but
    // "almost certainly" is not evidence.
    // Consequence: per-checkpoint race-balancing progress is not recorded, so the rubber-band
    // works off distance only. Not on the activation path.
    // DELETE-WHEN 0x8278A658 is exported (idat re-run) or recovered from image.bin.
    (void)lpAction;

    static bool sbWitnessed = false;
    if (!sbWitnessed && CgsDev::Log::gpDebugPrint != 0)
    {
        sbWitnessed = true;
        *CgsDev::Log::gpDebugPrint
            << "[ai-evt] OnRaceCarReachedCheckpoint is PARKED (@0x8278A658 is an ARTIST export"
               " hole) -- race-balancing checkpoint progress is not recorded\n";
    }
}

}
