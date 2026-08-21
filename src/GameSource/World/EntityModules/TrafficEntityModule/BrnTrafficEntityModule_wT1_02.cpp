// ============================================================================
// BrnTrafficEntityModule_wT1_02.cpp
//
//   TrafficEntityModule::PreSceneUpdate @0x8274A968  PARTIAL, ARTIST EXPORT HOLE
//
// This function owns E_STARTINGUPSTATE_WAITING_FOR_PLAYER -> _POPULATING, the only door into
// the arm that creates parked cars (PostPhysicsUpdate's POPULATING arm, _wT1_01.cpp).
//
// // FLAG rung-3: Feb-2007 is the primary source for this function's control flow.
// There is no .ida-exports/BURNOUT_X360_ARTIST.XEX/0x8274A968.json, so rung 1 has no body.
// Control flow comes from references/Feb-2007/.../BrnTrafficEntityModule.cpp:1110..1266;
// every member, constant and callee below is attested elsewhere, and which legs exist comes
// from the ship's 36-entry `xrefs_to` inventory for 0x8274A968. Leg ORDER is not recoverable
// from an xref list, so unbodied legs are named gates rather than guesses at placement.
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

#include <cstdlib>   // getenv

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

    // DELETE-WHEN-STABLE bring-up probe plumbing, gated on BRN_TRAFFIC_DIAG.
    // [DIAG] NOT IN THE X360 BINARY.
    bool TrafficDiagEnabled()
    {
        static const bool sbEnabled = (getenv("BRN_TRAFFIC_DIAG") != 0);
        return sbEnabled;
    }

    CgsDev::Log::DebugPrint* TrafficDiagStream()
    {
        if (!TrafficDiagEnabled() || CgsDev::Log::gpDebugPrint == 0)
        {
            return 0;
        }
        return CgsDev::Log::gpDebugPrint;
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::PreSceneUpdate  @ 0x8274A968   PARTIAL, EXPORT HOLE
//
// Feb-2007 shape at BrnTrafficEntityModule.cpp:1110; the WAITING_FOR_PLAYER arm is :1188..
// :1201. The signature is the header's, not the leak's: the leak passes
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
    (void)lUpdateSet;

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

    // GATE: the update-set decode (lbSimPaused, mbInReplay) plus the ship's EnterReplay
    // @0x827081D8 / LeaveReplay @0x82708248 latch. BrnUpdateSet is a bare `typedef u16` in
    // SharedClasses/BrnSharedConstants.h with no named bits, and the ARTIST body is an export
    // hole, so the masks cannot be read off anything.
    // DELETE WHEN: PreSceneUpdate's body is recovered, or BrnUpdateSet gets named bits.
    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "PreSceneUpdate update-set decode (lbSimPaused from E_HLA_UPDATE_PAUSED, mbInReplay "
            "from E_HLA_UPDATE_PLAYING_REPLAY) and the ship's EnterReplay @0x827081D8 / "
            "LeaveReplay @0x82708248 latch -- BrnUpdateSet is a bare typedef with NO named bits "
            "in this tree and the ARTIST body is an EXPORT HOLE, so the masks cannot be read");
    }

    lpOutput->LockForWrite();
    lpInput->LockForRead();

    if (mbHullSyncDivergence)
    {
        // GATE: the hull-sync 2D banner. Its four text/position/size/colour constants come
        // from the leak's BrnTrafficTweakConstants block with no X360 attestation. Online-only
        // (mbHullSyncDivergence is set only by UpdateRaceCarHulls' online arm), so offline
        // cannot reach it. DELETE WHEN the four constants are attested on this build.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "PreSceneUpdate mbHullSyncDivergence 2D banner (DebugInterface::Get2dRender()."
            "Draw2DText) -- its four text/position/size/colour constants have no X360 "
            "attestation on this build. ONLINE-only, unreachable offline");
    }

    {
        // GATE: the ship's five per-frame output producers, none bodied in this tree, and
        // their order relative to the state switch is not recoverable from an xref list, so
        // they are named as a block rather than placed. They write into OutputBuffer_PreScene,
        // which nothing in this build reads yet. DELETE WHEN the bodies land.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "PreSceneUpdate output producers -- GenerateNearbyParkedTrafficOutput @0x8271FA18, "
            "GenerateSympatheticCrasherOutput @0x82715C30, GenerateNearMissOutput @0x82715CC0, "
            "GeneratePotentialLeapedAndStompedCarsOutput @0x8271F298 and the leak's "
            "GenerateRivalInActiveHullOutput. No bodies in this tree; the ARTIST hole means "
            "their order in this function is unknown, so they are not placed");
    }

    switch (meState)
    {
    // Leg order follows Feb-2007 BrnTrafficEntityModule.cpp:1145..:1174.
    //
    // BEHAVIOUR DELTA: the leak's guard is `!IsPaused() && !lbSimPaused`; only IsPaused() is
    // written. PostPhysicsUpdate's asm attests update-set bit 0 as sim-paused for ITSELF
    // (0x8274E710 `clrlwi r27, r30, 31`), but nothing attests that PreSceneUpdate decodes the
    // same bit, and this function is an export hole. IsPaused() alone runs the legs on a frame
    // the console might have skipped, which at worst registers a scene entity one frame early.
    // DELETE WHEN: PreSceneUpdate's body is recovered, or BrnUpdateSet gets named bits.
    case E_STATE_RUNNING:
    {
        {
            // GATE: HandleIncomingNetworkData @0x82741AF8, no body; online-only.
            static bool sbLogged = false;
            LogMissingLeg(sbLogged,
                "PreSceneUpdate E_STATE_RUNNING leg HandleIncomingNetworkData @0x82741AF8 -- "
                "no body in this tree; ONLINE-only (it drains the network hull-sync ring)");
        }

        if (!IsPaused())
        {
            // UpdateTimers @0x82715858 is the only writer of mbDecisionFrame in the image, so
            // everything downstream of IsDecisionFrame() depends on this call. Body in
            // _wT1_06.cpp.
            UpdateTimers(lpInput);

            {
                // GATE: KillDyingVehicleEntities @0x82741E40 (and its callee
                // KillDyingVehicleEntity @0x8272EB40), the REMOVE half of the scene
                // registration below. Neither is bodied, so a dying traffic vehicle keeps its
                // scene entity. Parked cars only die in TEARING_DOWN today, so nothing leaks
                // on a normal drive. DELETE WHEN the bodies land, which KillOutOfAreaTraffic
                // will force.
                static bool sbLogged = false;
                LogMissingLeg(sbLogged,
                    "PreSceneUpdate E_STATE_RUNNING leg KillDyingVehicleEntities @0x82741E40 "
                    "-- no body (nor its callee KillDyingVehicleEntity @0x8272EB40). It is the "
                    "REMOVE half of CreateNewVehicleEntities; a dying vehicle would keep its "
                    "scene entity. Unreachable on the parked path today because parked cars "
                    "only die in TEARING_DOWN");
            }

            // The scene registration; body in BrnTrafficEntityModule_wT1_05.cpp, which must
            // also be mounted in tools/build/build_game_exe.bat or this call is an LNK2019 at
            // exe link. The per-TU `cl /c` gate cannot see that.
            CreateNewVehicleEntities(lpOutput);

            {
                // GATE: UpdateCollidableVehicles @0x827302C8, no body. It posts
                // AddVolumeInstance for vehicles near the player, so parked cars render but
                // are not solid until it lands. DELETE WHEN the body lands.
                static bool sbLogged = false;
                LogMissingLeg(sbLogged,
                    "PreSceneUpdate E_STATE_RUNNING leg UpdateCollidableVehicles @0x827302C8 "
                    "-- no body; the COLLISION-volume half (wave 3). Parked cars register a "
                    "scene entity here but no collision volume, so they render and are not "
                    "solid");
            }
        }

        {
            static bool sbLogged = false;
            LogMissingLeg(sbLogged,
                "PreSceneUpdate E_STATE_RUNNING remaining legs -- GenerateCrashedVehicleEvents "
                "@0x82720030 / ManageTriggers @0x82747518 / UpdateSerialiser @0x8272DA80 / "
                "UpdateCrashSlider @0x82715A18. None bodied; all are crash/trigger/replay "
                "surface (waves 2 and 3). The leak's KillTrafficTooCloseToRaceCars is NOT in "
                "the ship's callee list and is therefore not written");
        }
    }
    break;

    case E_STATE_STARTING_UP:
    {
        // Leak :1176..:1179. mbDEBUGTurnTrafficOff's ship default is false (Construct
        // @0x82740220), so this early-out does not fire on a normal boot.
        if (mbDEBUGTurnTrafficOff)
        {
            break;
        }

        switch (meStartingUpState)
        {
        case E_STARTINGUPSTATE_WAITING_FOR_PLAYER:
        {
            // The transition, leak :1188..:1201. Ship-attested: the callee inventory for
            // 0x8274A968 has both IsPlayerCarActive @0x82277B90 and sub_82710BD8
            // (== InputBuffer_PreScene::GetActiveRaceCarOutputInterface, DWARF :153).
            //
            // Guard polarity is easy to get backwards: the player test applies ONLY at
            // non-zero density, so density 0 advances immediately. Dropping the test advances
            // to POPULATING before a player car exists, RecalculateActiveHulls then builds its
            // sim box around nothing, and the module reaches RUNNING with an empty world it
            // never leaves, because POPULATING runs once.
            //
            // mbDEBUGTurnTrafficOff is re-tested here as the leak does, though the arm above
            // already returned on it. Redundancy in the original, kept.
            if (mfTrafficAmountScale > 0.0f && !mbDEBUGTurnTrafficOff)
            {
                if (!lpInput->GetActiveRaceCarOutputInterface()->IsPlayerCarActive())
                {
                    break;
                }
            }

            meStartingUpState = E_STARTINGUPSTATE_POPULATING;

            if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
            {
                // [T1-populate] one-shot by construction: POPULATING is entered once per
                // starting-up cycle. DELETE-WHEN-STABLE.
                *lpDiag << "[T1-populate] meStartingUpState -> E_STARTINGUPSTATE_POPULATING"
                        << " (density=" << mfTrafficAmountScale << ")\n";
            }
        }
        break;

        case E_STARTINGUPSTATE_POPULATING:
            // Leak :1203..:1206, empty. PostPhysicsUpdate does the populating and advances to
            // WAITING_FOR_STREAMING.
            break;

        case E_STARTINGUPSTATE_WAITING_FOR_STREAMING:
            // Leak :1208..:1211, empty. PostPhysicsUpdate owns the AreAllAssetsLoaded() latch
            // and EnterRunningState.
            break;

        default:
            CGS_ASSERT(false, "Invalid starting up state");   // leak baked .cpp line 1224
            break;
        }
    }
    break;

    case E_STATE_TEARING_DOWN:
    {
        // GATE: leak :1218..:1250, a three-way switch on meTearingDownState whose only
        // non-empty arm (FLUSHING) calls the bodiless KillDyingVehicleEntities @0x82741E40.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "PreSceneUpdate E_STATE_TEARING_DOWN arm -- the meTearingDownState switch whose "
            "FLUSHING arm calls KillDyingVehicleEntities @0x82741E40 (no body in this tree)");
    }
    break;

    default:
        CGS_ASSERT(false, "Invalid state in traffic system");   // leak baked .cpp line 1259
        break;
    }

    lpOutput->UnlockForWrite();
    lpInput->UnlockForRead();
}

}
