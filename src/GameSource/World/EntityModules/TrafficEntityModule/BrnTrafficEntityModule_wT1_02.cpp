// ============================================================================
// BrnTrafficEntityModule_wT1_02.cpp
//
//   TrafficEntityModule::PreSceneUpdate @0x8274A968  PARTIAL, ARTIST EXPORT HOLE
//
// This function owns E_STARTINGUPSTATE_WAITING_FOR_PLAYER -> _POPULATING, the only door into
// the arm that creates parked cars (PostPhysicsUpdate's POPULATING arm, _wT1_01.cpp).
//
// FLAG rung-3: an earlier source revision is the primary source for this function's control flow.
// There is no per-function export for it, so rung 1 has no body.
// Control flow comes from the earlier revision of this function;
// every member, constant and callee below is attested elsewhere, and which legs exist comes
// from the ship's 36-entry `xrefs_to` inventory for 0x8274A968. An xref list carries no order,
// but the image does: where a leg's slot has been read out of the instruction stream it is
// placed (see the output-producer run below); the rest stay named gates.
//
// MOUNT REQUIRED (conductor-owned; agents may not edit the build script). Add
//   echo "%SRC%\GameSource\World\EntityModules\TrafficEntityModule\BrnTrafficEntityModule_wT1_02.cpp"
// to tools/build/build_game_exe.bat after the _wT1_01.cpp mount. The inert gate that defined
// this symbol in GameSource/World/WorldLinkStubs.cpp is deleted (it could not be retired
// separately: BrnUpdateSet is a bare `typedef u16`, so the gate's spelling mangled identically
// to this body's), so until the mount lands the exe link fails with LNK2019 on PreSceneUpdate.
// The per-TU `cl /c` gate sees neither the mount nor a duplicate definition; only a link does.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"

// RCEntityActiveRaceCarOutputInterface::IsPlayerCarActive -- the transition's own test.
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"              // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"      // gpDebugPrint / gxMessageFilterFlags


namespace BrnTraffic
{
namespace
{
    // NAMED LEG GATE -- same shape as the sibling partfiles', file-local by convention.
    // [DIAG] NOT IN THE X360 BINARY.
    inline void LogMissingLeg(bool& lrbAlreadyLogged, const char* lpcLegNameAndReason)
    {
        if (lrbAlreadyLogged)
        {
            return;
        }
        lrbAlreadyLogged = true;

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[T1-traffic-leg] TrafficEntityModule leg NOT RECONSTRUCTED, skipped: "
                << lpcLegNameAndReason << " [FLAG PC partial gate]\n";
        }
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::PreSceneUpdate  @ 0x8274A968   PARTIAL, EXPORT HOLE
//
// Earlier-revision shape; the WAITING_FOR_PLAYER arm is its second half.
// The signature is the header's, not the earlier revision's: that revision passes
// `const InputBuffer_PreScene*` where BrnTrafficEntityModule.h takes a non-const pointer.
// ----------------------------------------------------------------------------
void TrafficEntityModule::PreSceneUpdate(CgsModule::IOBufferStack* lpInputBufferStack,
                                         CgsModule::IOBufferStack* lpOutputBufferStack,
                                         BrnTrafficIO::InputBuffer_PreScene* lpInput,
                                         BrnTrafficIO::OutputBuffer_PreScene* lpOutput,
                                         BrnUpdateSet lUpdateSet)
{
    (void)lpInputBufferStack;
    (void)lpOutputBufferStack;

    // ⭐⭐ THE SIM-PAUSED BIT, RECOVERED FROM THE IMAGE (pauseresume wave, 2026-08-27).
    // 0x8274A994 `mr r30, r8` puts the update set (the 6th argument) in r30, and the prologue
    // splits it into the two bits this function cares about:
    //     0x8274A9A4  rlwinm r11, r30, 24, 24, 31   ; (updateSet >> 8) & 0xFF
    //     0x8274A9AC  rlwinm r29, r11, 0,  31, 31   ; r29 = BIT 8  -- mbInReplay; it drives the
    //                                               ;   EnterReplay @0x827081D8 / LeaveReplay
    //                                               ;   @0x82708248 latch at 0x8274A9C4..0x8274A9D8
    //     0x8274A9B0  rlwinm r27, r30, 0,  31, 31   ; r27 = updateSet & 1 -- THE SIM-PAUSED BIT
    // i.e. `clrlwi r27, r30, 31`, the same decode PostPhysicsUpdate does at 0x8274E710.
    const bool lbSimPaused = ((lUpdateSet & 1u) != 0);

    {
        // GATE: the console's PerfMonCpu Start/StopMonitor(miPerfMon_PreSceneUpdate) bracket.
        // The handle is never issued because Construct's twenty AddMonitor registrations are
        // gated in the sibling partfile. DELETE WHEN those registrations land.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "PreSceneUpdate PerfMonCpu Start/StopMonitor(miPerfMon_PreSceneUpdate) bracket -- "
            "the handle is never issued because Construct's twenty AddMonitor registrations "
            "are gated; same reason the sibling partfile gates PostPhysicsUpdate's bracket");
    }

    // ⛔ THE OLD NOTE HERE IS RETRACTED (2026-08-27). It said: "BrnUpdateSet is a bare
    // `typedef u16` ... with no named bits, and the ARTIST body is an export hole, so THE MASKS
    // CANNOT BE READ OFF ANYTHING." **An ARTIST export hole is not an IMAGE hole.** Both masks
    // were read straight out of the instruction bytes at 0x8274A9A4..0x8274A9B0 (see the decode
    // above); the export set simply carries no per-function JSON for this address. ⭐ "The export
    // does not have it" is a fact about the export, never about the binary.
    //
    // The SIM-PAUSED half is LIVE now (it gates the block below, and it is what makes a paused
    // traffic sim actually stop). The REPLAY half stays gated: bit 8 is decoded above in the
    // console and drives EnterReplay @0x827081D8 / LeaveReplay @0x82708248, neither of which has
    // a body in this tree, and replay is not this wave's surface.
    // DELETE WHEN: EnterReplay/LeaveReplay are bodied.
    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "PreSceneUpdate REPLAY latch -- update-set BIT 8 (E_HLA_UPDATE_PLAYING_REPLAY) is "
            "decoded by the console at 0x8274A9A4/0x8274A9AC and drives EnterReplay @0x827081D8 "
            "/ LeaveReplay @0x82708248 at 0x8274A9C4..0x8274A9D8; neither callee is bodied in "
            "this tree. The SIM-PAUSED half of the decode (bit 0, 0x8274A9B0) is LIVE");
    }

    lpOutput->LockForWrite();
    lpInput->LockForRead();

    if (mbHullSyncDivergence)
    {
        // GATE: the hull-sync 2D banner. Its four text/position/size/colour constants come
        // from the earlier revision's BrnTrafficTweakConstants block with no binary attestation. Online-only
        // (mbHullSyncDivergence is set only by UpdateRaceCarHulls' online arm), so offline
        // cannot reach it. DELETE WHEN the four constants are attested on this build.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "PreSceneUpdate mbHullSyncDivergence 2D banner (DebugInterface::Get2dRender()."
            "Draw2DText) -- its four text/position/size/colour constants have no X360 "
            "attestation on this build. ONLINE-only, unreachable offline");
    }

    // ⭐ THE PRODUCER BLOCK, AND ITS ORDER, ARE NOW MEASURED (2026-09-11). The old gate here
    // said "their order in this function is unknown, so they are not placed" -- that was a
    // claim about the EXPORT, not the image, and it was wrong the same way the update-set note
    // above was. Disassembling this function straight out of the image shows the producers as a
    // straight run right here, between the hull-sync banner and the state switch, each called
    // with (this, lpInput, lpOutput):
    //     GenerateSympatheticCrasherOutput
    //     GenerateNearMissOutput                 <- LIVE below
    //     GeneratePotentialLeapedAndStompedCarsOutput
    //     GenerateNearbyParkedTrafficOutput
    // then ManageTriggers, then the switch. GenerateRivalInActiveHullOutput is NOT among them
    // on this build; nothing in this function calls it.
    GenerateNearMissOutput(lpInput, lpOutput);

    {
        // GATE: the three remaining pre-scene output producers, none bodied in this tree. They
        // are placed in the console's own order around the live call above. They write into
        // OutputBuffer_PreScene. DELETE WHEN the bodies land.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "PreSceneUpdate output producers -- GenerateSympatheticCrasherOutput (before the "
            "live GenerateNearMissOutput), GeneratePotentialLeapedAndStompedCarsOutput and "
            "GenerateNearbyParkedTrafficOutput (after it). No bodies in this tree");
    }

    switch (meState)
    {
    // Leg order follows the earlier revision of this function.
    //
    // ✅ THE BEHAVIOUR DELTA THAT STOOD HERE IS CLOSED (2026-08-27, pauseresume wave).
    // It said: "the earlier revision's guard is `!IsPaused() && !lbSimPaused`; only IsPaused() is written ...
    // nothing attests that PreSceneUpdate decodes the same bit, and this function is an export
    // hole ... IsPaused() alone runs the legs on a frame the console might have skipped, WHICH AT
    // WORST REGISTERS A SCENE ENTITY ONE FRAME EARLY."
    // ⛔⛔ THE RISK ASSESSMENT WAS WRONG, and instructively so. It rated the block by looking at
    // CreateNewVehicleEntities and stopped there -- but `UpdateTimers` is in the SAME guarded
    // block, and UpdateTimers is **the only writer of mbDecisionFrame in the whole image**, i.e.
    // this module's frame clock. With only `!IsPaused()`, the clock FREE-RAN for the entire
    // duration of a sim pause: PostPhysicsUpdate's paused arm (correctly empty) consumed nothing,
    // so the param time-slice cursor stood still while decision frames kept being minted, and the
    // first decision frame after the resume tripped `muLastParamCalculated >= KU_MAX_PARAMS`
    // (UpdateParams, _wT2_02.cpp:96). Not one frame early -- an entire pause of free-running.
    // ⭐ THE CLASS: a claim about ONE BRANCH of a block published as a claim about the block.
    //
    // The console's guard, read out of the image (the export hole notwithstanding):
    //     0x8274ABC4  bl     TrafficEntityModule::IsPaused (0x82707560)
    //     0x8274ABC8  rlwinm r11, r3, 0, 24, 31
    //     0x8274ABD0  bc  -> 0x8274AC28        ; skip the block if IsPaused()
    //     0x8274ABD4  rlwinm r11, r27, 0, 24, 31    ; r27 == lUpdateSet & 1 (prologue, above)
    //     0x8274ABDC  bc  -> 0x8274AC28        ; skip the block if the SIM-PAUSED bit is set
    //     0x8274ABE8  bl     TrafficEntityModule::UpdateTimers (0x82715858)
    // -- exactly the earlier revision's `!IsPaused() && !lbSimPaused`. Restored below; a RESTORATION, not an
    // invented arm. (The console's block also carries UpdateCrashSlider @0x82715A18 and
    // GenerateCrashedVehicleEvents @0x82720030, which stay gated with the tail legs.)
    case E_STATE_RUNNING:
    {
        {
            // GATE: HandleIncomingNetworkData @0x82741AF8, no body; online-only.
            static bool sbLogged = false;
            LogMissingLeg(sbLogged,
                "PreSceneUpdate E_STATE_RUNNING leg HandleIncomingNetworkData @0x82741AF8 -- "
                "no body in this tree; ONLINE-only (it drains the network hull-sync ring)");
        }

        if (!IsPaused() && !lbSimPaused)
        {
            // UpdateTimers @0x82715858 is the only writer of mbDecisionFrame in the image, so
            // everything downstream of IsDecisionFrame() depends on this call -- and that is
            // exactly why it must stand behind the sim-paused bit too. Body in _wT1_06.cpp.
            UpdateTimers(lpInput);

            // 0x8274ABF0 -- UN-GATED. Body in _wT5_01.cpp; it is the ONLY writer of
            // mfCrashSliderFinalValue, so with it gated ShouldBeHollywoodAction() was constant
            // false and the sympathetic-crash arm in UpdateParams was unreachable. Its slot is
            // read straight out of the image (PreSceneUpdate is an ARTIST export hole):
            // `bl 0x82715858` (UpdateTimers) then `bl 0x82715A18` two instructions later,
            // before KillDyingVehicleEntities @0x8274ABF8. Mount _wT5_01.cpp in
            // tools/build/build_game_exe.bat or this call is an LNK2019 at exe link.
            UpdateCrashSlider();

            // The REMOVE half of the scene registration (bodies in
            // BrnTrafficEntityModule_KillDyingVehicleEntities.cpp). Landing it closed the
            // param-pool leak behind the "traffic is anchored to the junkyard" user report
            // (2026-08-24): without it a killed driving car kept its entity/collision/physics
            // registrations, UpdateParams_UpdateDead could never retire its param, and
            // mFreeParams drained 400 -> 0 a few minutes into every session.
            KillDyingVehicleEntities(lpOutput);

            // The scene registration; body in BrnTrafficEntityModule_wT1_05.cpp, which must
            // also be mounted in tools/build/build_game_exe.bat or this call is an LNK2019 at
            // exe link. The per-TU `cl /c` gate cannot see that.
            CreateNewVehicleEntities(lpOutput);

            // UN-GATED: UpdateCollidableVehicles @0x827302C8 is BODIED
            // (_wT4_01.cpp). It is the COLLISION-volume half of the registration above -- the
            // only producer of mVehicleSoaData.mCollidableVehicles and the only caller of
            // AddVolumeInstance / AddForCollision for a traffic vehicle. Without it a parked
            // car has a scene entity and no volume, so nothing is solid and the broad phase
            // never emits a race-car-vs-traffic overlap pair. Mount _wT4_01.cpp in
            // tools/build/build_game_exe.bat or this call is an LNK2019 at exe link.
            UpdateCollidableVehicles(lpInput, lpOutput);

            // 0x8274AC20 -- UN-GATED 2026-09-06. Body in _wT3_01.cpp beside its one producer.
            // ⭐ ITS TAIL IS THE ONLY THING THAT BOUNDS maNewCrashedVehicles: the array holds
            // 160 TrafficCrashInfo, RecordTrafficVehicleIsPhysical appends one per promotion,
            // and this Clear is the only shrink apart from HandleRecycledTraffic's single-entry
            // erase and Reset(). With it gated, append #161 would have written an EntityId over
            // the count word and #162 a store ~half a gigabyte past the record.
            // It is INSIDE the `!IsPaused() && !lbSimPaused` guard, exactly where the console
            // puts it -- the whole block sits between 0x8274ABD0/0x8274ABDC and 0x8274AC24.
            GenerateCrashedVehicleEvents(lpOutput);
        }

        {
            static bool sbLogged = false;
            LogMissingLeg(sbLogged,
                "PreSceneUpdate E_STATE_RUNNING remaining legs -- ManageTriggers @0x82747518 / "
                "UpdateSerialiser @0x8272DA80. Neither bodied; both are trigger/replay surface. "
                "UpdateCrashSlider @0x82715A18 and GenerateCrashedVehicleEvents @0x82720030 "
                "WERE in this list and are now live above. The earlier revision's "
                "KillTrafficTooCloseToRaceCars is NOT in the ship's callee list and is "
                "therefore not written");
        }
    }
    break;

    case E_STATE_STARTING_UP:
    {
        // mbDEBUGTurnTrafficOff's ship default is false (Construct
        // @0x82740220), so this early-out does not fire on a normal boot.
        if (mbDEBUGTurnTrafficOff)
        {
            break;
        }

        switch (meStartingUpState)
        {
        case E_STARTINGUPSTATE_WAITING_FOR_PLAYER:
        {
            // The transition. Ship-attested: the callee inventory for
            // 0x8274A968 has both IsPlayerCarActive @0x82277B90 and sub_82710BD8
            // (== InputBuffer_PreScene::GetActiveRaceCarOutputInterface, DWARF :153).
            //
            // Guard polarity is easy to get backwards: the player test applies ONLY at
            // non-zero density, so density 0 advances immediately. Dropping the test advances
            // to POPULATING before a player car exists, RecalculateActiveHulls then builds its
            // sim box around nothing, and the module reaches RUNNING with an empty world it
            // never leaves, because POPULATING runs once.
            //
            // mbDEBUGTurnTrafficOff is re-tested here as the reference does, though the arm above
            // already returned on it. Redundancy in the original, kept.
            if (mfTrafficAmountScale > 0.0f && !mbDEBUGTurnTrafficOff)
            {
                if (!lpInput->GetActiveRaceCarOutputInterface()->IsPlayerCarActive())
                {
                    break;
                }
            }

            meStartingUpState = E_STARTINGUPSTATE_POPULATING;
        }
        break;

        case E_STARTINGUPSTATE_POPULATING:
            // Empty. PostPhysicsUpdate does the populating and advances to
            // WAITING_FOR_STREAMING.
            break;

        case E_STARTINGUPSTATE_WAITING_FOR_STREAMING:
            // Empty. PostPhysicsUpdate owns the AreAllAssetsLoaded() latch
            // and EnterRunningState.
            break;

        default:
            CGS_ASSERT(false, "Invalid starting up state");
            break;
        }
    }
    break;

    // The three-way switch on meTearingDownState; its only non-empty arm
    // (FLUSHING) runs KillDyingVehicleEntities, which is what publishes mbAllVehiclesDead for
    // PostPhysicsUpdate's flush tail to advance on. LIVE 2026-09-10 -- the state producer
    // (PostPhysicsUpdate's TEARING_DOWN arm, _wT1_01.cpp) landed the same day.
    case E_STATE_TEARING_DOWN:
    {
        switch (meTearingDownState)
        {
        case E_TEARINGDOWNSTATE_WIPING:
            break;
        case E_TEARINGDOWNSTATE_FLUSHING:
            KillDyingVehicleEntities(lpOutput);
            break;
        case E_TEARINGDOWNSTATE_WAITING_TO_RESET:
            break;
        default:
            break;
        }
    }
    break;

    default:
        CGS_ASSERT(false, "Invalid state in traffic system");
        break;
    }

    lpOutput->UnlockForWrite();
    lpInput->UnlockForRead();
}

}
