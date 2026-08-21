// ============================================================================
// BrnTrafficEntityModule_wT1_02.cpp  --  wave T1 (parked traffic cars) round 2,
// cluster R2A, partfile 2 of 2.
//
// ONE FUNCTION LIVES HERE: TrafficEntityModule::PreSceneUpdate @0x8274A968 -- round 1's
// BLOCKER B2, and the owner of the state transition
//
//     E_STARTINGUPSTATE_WAITING_FOR_PLAYER  ->  E_STARTINGUPSTATE_POPULATING
//
// which is the only door into the arm that creates parked cars. With PreSceneUpdate an inert
// gate (WorldLinkStubs.cpp), meStartingUpState never left WAITING_FOR_PLAYER, so
// PostPhysicsUpdate's POPULATING arm in the sibling partfile
// BrnTrafficEntityModule_wT1_01.cpp -- RecalculateActiveHulls -> SpawnNewTraffic ->
// FillNewHull -> StaticVehicles_CreateNewVehicles, all real -- was never entered even once.
//
// ---------------------------------------------------------------------------
// ⛔⛔ LINK BLOCKER -- THIS FILE MUST BE MOUNTED, AND ITS GATE IS ALREADY RETIRED.
//
// (1) GATE RETIRED, in this same change: the inert boot gate that defined this same symbol in
//     GameSource/World/WorldLinkStubs.cpp is DELETED (tombstone left in its place). It could
//     not be retired separately -- BrnUpdateSet is a bare `typedef u16`, so the gate's
//     `unsigned short` spelling mangled IDENTICALLY to this body's and the two collided.
//     MEASURED, not predicted:
//         link /DLL /NOENTRY t102.obj wls.obj
//         wls.obj : error LNK2005: "public: void __cdecl BrnTraffic::TrafficEntityModule::
//           PreSceneUpdate(...)" already defined in t102.obj
//     The per-TU `cl /c` compile gate CANNOT see this: a green selfcheck on this file proves
//     nothing about the duplicate. Only a link does.
//
// (2) MOUNT STILL REQUIRED (conductor-owned -- agents may not edit the build script). Add
//         echo "%SRC%\GameSource\World\EntityModules\TrafficEntityModule\BrnTrafficEntityModule_wT1_02.cpp"
//     to tools/build/build_game_exe.bat immediately after the _wT1_01.cpp mount (currently
//     ~:2092, inside the "2026-08-21 wave T1 parked traffic cars" block). Until then this
//     file is not compiled into the exe. With the gate now retired that is a LOUD failure
//     (LNK2019 on PreSceneUpdate at exe link), which is deliberate: the alternative --
//     retiring nothing and mounting nothing -- is the SILENT failure where the module never
//     leaves WAITING_FOR_PLAYER while the tree's banners claim blocker B2 is closed.
//     Sibling in the same fix window: R2B's BrnTrafficEntityModule_Render.cpp is likewise
//     unmounted with its two gates already retired.
//
// ---------------------------------------------------------------------------
// ⚠️⚠️ SOURCE-LADDER FLAG: THIS FUNCTION IS AN ARTIST EXPORT HOLE.
//
// There is NO .ida-exports/BURNOUT_X360_ARTIST.XEX/0x8274A968.json -- no pseudocode and no
// assembly. Rung 1 is therefore unavailable for the BODY, and the primary rung used below is
// the leaked Feb-2007 source (references/Feb-2007/BrnEntityModuleUnity/GameSource/World/
// EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp:1110..1266), which AGENTS.md
// ranks THIRD -- style/idiom only. That is a deliberate, flagged exception, and it is
// confined to CONTROL FLOW; every member, constant and callee named below is attested
// elsewhere. Read this file as: leak-shaped skeleton, X360-attested contents, everything
// unattested explicitly gated. // FLAG rung-3: Feb-2007 is the primary source for this
// function's control flow.
//
// ⭐ WHAT RUNG 1 STILL GIVES, EVEN WITH THE BODY MISSING: a COMPLETE CALLEE INVENTORY.
// Every exported function records its `xrefs_to`, so scanning the whole export set for
// 0x8274A968 yields exactly what the SHIP's PreSceneUpdate calls -- 36 entries. That is
// evidence about the ship, not about Feb-2007, and it does three things:
//
//   (1) IT CONFIRMS THE ONE LEG THIS FILE LANDS. The list contains
//       BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface::
//       IsPlayerCarActive @0x82277B90 and sub_82710BD8 (==
//       InputBuffer_PreScene::GetActiveRaceCarOutputInterface, DWARF :153). Those two calls
//       exist in the ship, in this function, and the leak uses exactly that pair in exactly
//       one place: the WAITING_FOR_PLAYER guard. The transition below is therefore SHIP-
//       attested in substance even though its surrounding `switch` is leak-shaped.
//   (2) IT CONFIRMS THE SKELETON. PerfMonCpu Start/StopMonitor, IOBuffer LockForWrite /
//       LockForRead / UnlockForWrite / UnlockForRead, DebugInterface + Get2dRender +
//       Draw2DText (the mbHullSyncDivergence banner), IsPaused, UpdateTimers,
//       KillDyingVehicleEntities, CreateNewVehicleEntities, UpdateCollidableVehicles and
//       GenerateCrashedVehicleEvents are all present -- the leak's bracket and its
//       E_STATE_RUNNING arm, function for function.
//   (3) IT NAMES THE SHIP'S ADDITIONS the leak does not have, so each one gets a NAMED gate
//       instead of vanishing without a trace: GenerateNearbyParkedTrafficOutput @0x8271FA18,
//       GenerateSympatheticCrasherOutput @0x82715C30, GenerateNearMissOutput @0x82715CC0,
//       GeneratePotentialLeapedAndStompedCarsOutput @0x8271F298, ManageTriggers @0x82747518,
//       UpdateSerialiser @0x8272DA80, UpdateCrashSlider @0x82715A18, EnterReplay
//       @0x827081D8 / LeaveReplay @0x82708248, TrafficEntitySerialiser::Read @0x8265F2D8 +
//       CheckStaticLayoutCleared @0x82653418, and GetPlayerRaceCarState (sub_82310240).
//       Conversely the leak's KillTrafficTooCloseToRaceCars is ABSENT from the ship's call
//       list, so it is not written here either.
//   The ORDER of those legs is not recoverable from an xref list, which is exactly why every
//   one of them is a named gate instead of a guess at placement.
//
// ---------------------------------------------------------------------------
// WHAT IS REAL HERE
//   * the IO lock bracket (both buffers, in the leak's order);
//   * the whole `switch (meState)` skeleton with both baked asserts;
//   * E_STATE_STARTING_UP's `if (mbDEBUGTurnTrafficOff) break;` early-out;
//   * E_STARTINGUPSTATE_WAITING_FOR_PLAYER -> _POPULATING, INCLUDING its
//     `lpInput->GetActiveRaceCarOutputInterface()->IsPlayerCarActive()` test and its
//     `mfTrafficAmountScale > 0.0f` guard;
//   * the empty POPULATING / WAITING_FOR_STREAMING arms (they really are empty here -- their
//     work is PostPhysicsUpdate's).
//
// ⭐⭐ AND, SINCE 2026-08-21 (wave T1 ROUND 4), TWO LEGS OF THE E_STATE_RUNNING ARM:
//   * UpdateTimers @0x82715858 -- the ONLY writer of mbDecisionFrame in the image, i.e. the
//     thing that makes IsDecisionFrame() ever true once the module leaves STARTING_UP, i.e.
//     the thing that makes PostPhysicsUpdate's steady-state loop run at all (item 1);
//   * CreateNewVehicleEntities @0x8272FA30 -- the per-vehicle scene registration, whose ONLY
//     caller in the whole image is this function (item 2).
//   Both bodies landed the same day, in _wT1_06.cpp and _wT1_05.cpp respectively.
//
// WHAT IS GATED, AND WHY IT IS SAFE TO GATE FOR WAVE 1
//   Everything in the list at (3), the remaining E_STATE_RUNNING legs (each now named
//   individually rather than as one block), and the E_STATE_TEARING_DOWN arm. None of those
//   functions has a body in this tree, and none of them is on the parked-car path: a parked
//   car is created inside PostPhysicsUpdate's STARTING_UP arm (and, in steady state, inside
//   UpdateDecisionFrame / UpdateNonDecisionFrame), and it becomes VISIBLE through the
//   CreateNewVehicleEntities call this arm now makes.
//
// ⚠️ WHY THE IsPlayerCarActive TEST MUST NOT BE DROPPED. Emitting the transition
// unconditionally would advance to POPULATING before a player car exists. RecalculateActiveHulls
// then builds its sim box around nothing, mActiveHulls comes back empty, SpawnNewTraffic fills
// nothing, and the module walks straight on to WAITING_FOR_STREAMING and RUNNING with an empty
// world -- a state it never leaves, because POPULATING runs once. That is strictly worse than
// not advancing at all, and it looks identical from the outside. The guard is the function.
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
    // ------------------------------------------------------------------------------------
    // NAMED LEG GATE -- the same shape (and the same "[FLAG PC partial gate]" tail) as the
    // sibling partfiles BrnTrafficEntityModule_wT1_01.cpp and _wQ7_02.cpp, so one boot log
    // reads as a single stream. File-local by design: each partfile carries its own copy
    // rather than exporting one, which is the convention this directory already uses.
    // [DIAG] NOT IN THE X360 BINARY.
    // ------------------------------------------------------------------------------------
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

    // ------------------------------------------------------------------------------------
    // DELETE-WHEN-STABLE bring-up probe plumbing, gated on this wave's own BRN_TRAFFIC_DIAG
    // knob. Identical to the sibling partfile's.
    // [DIAG] NOT IN THE X360 BINARY.
    // ------------------------------------------------------------------------------------
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
// TrafficEntityModule::PreSceneUpdate  @ 0x8274A968   *** PARTIAL, EXPORT HOLE ***
//
// Feb-2007 shape at BrnTrafficEntityModule.cpp:1110; the WAITING_FOR_PLAYER arm this file
// exists for is at :1188..:1201. Signature is the committed one from
// BrnTrafficEntityModule.h (WorldModule::EntityModulePreSceneUpdate @0x827BD1F0 names the
// two buffers), NOT the leak's -- the leak passes `const InputBuffer_PreScene*` where the
// header takes a non-const pointer, and the header wins.
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
        // The console brackets the whole body in PerfMonCpu::StartMonitor /
        // StopMonitor(miPerfMon_PreSceneUpdate) -- both are in the ship's callee list. The
        // HANDLE does not exist on this build: Construct's twenty PerfMonCpu::AddMonitor
        // registrations are themselves a named gate in the sibling partfile, so
        // miPerfMon_PreSceneUpdate never leaves its constructed value and passing it to
        // StartMonitor would index the monitor registry with a handle nothing issued.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "PreSceneUpdate PerfMonCpu Start/StopMonitor(miPerfMon_PreSceneUpdate) bracket -- "
            "the handle is never issued because Construct's twenty AddMonitor registrations "
            "are gated; same reason the sibling partfile gates PostPhysicsUpdate's bracket");
    }

    // The leak also derives `lbSimPaused` from the update set here (E_HLA_UPDATE_PAUSED) and
    // latches mbInReplay from E_HLA_UPDATE_PLAYING_REPLAY. NEITHER BIT IS NAMED IN THIS TREE:
    // SharedClasses/BrnSharedConstants.h defines BrnUpdateSet as a bare `typedef u16` with no
    // enumerators at all, and with the ARTIST body missing there is no instruction to read the
    // masks off. The ship additionally calls EnterReplay @0x827081D8 / LeaveReplay @0x82708248
    // from here (both in its callee list) where the leak only assigns the flag, so its replay
    // handling is a shape the leak does not even show. All of it is gated together.
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
        // CgsDev::DebugInterface + Get2dRender + Draw2DText are all in the ship's callee
        // list, so this banner really is emitted here; its text/position/size/colour constants
        // (KAC_HULL_SYNC_ERROR_TEXT, K_HULL_SYNC_ERROR_TEXT_POS, KF_..._SIZE, KU_..._COLOUR)
        // live in the leak's BrnTrafficTweakConstants block and have no X360 attestation on
        // this build. Only reachable after a network hull-sync divergence, which offline
        // cannot happen (mbHullSyncDivergence is set only by UpdateRaceCarHulls' online arm).
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "PreSceneUpdate mbHullSyncDivergence 2D banner (DebugInterface::Get2dRender()."
            "Draw2DText) -- its four text/position/size/colour constants have no X360 "
            "attestation on this build. ONLINE-only, unreachable offline");
    }

    {
        // The ship's five per-frame output producers. Every one is in the callee list and
        // none has a body in this tree; their ORDER relative to the state switch is not
        // recoverable from an xref list, so they are named as one block rather than placed.
        // GenerateNearbyParkedTrafficOutput @0x8271FA18 is the interesting one for later
        // waves -- it is the "power park" scan over the 199 static params, and it is one of
        // the four consumers of InputBuffer_PreScene::GetActiveRaceCarOutputInterface() in the
        // image. (CORRECTED 2026-08-21 fix round: this line previously claimed it was the ONLY
        // other consumer. `xrefs_to` on sub_82710BD8 lists FOUR callers --
        // GeneratePotentialLeapedAndStompedCarsOutput @0x8271F298,
        // GenerateNearbyParkedTrafficOutput @0x8271FA18, UpdateCollidableVehicles @0x827302C8
        // and this function @0x8274A968 -- i.e. three others, exactly as the getter's own
        // banner in BrnTrafficEntityModuleIO.h already listed them. The claim did not change
        // any code; it is corrected so the next reader is not misled about the call graph.)
        // It writes into OutputBuffer_PreScene, which nothing in this build reads yet.
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
    // ========================================================================
    // ⭐⭐ TWO LEGS UN-GATED 2026-08-21 (wave T1 ROUND 4) -- and they are the two the whole
    //     round turns on.
    //
    // (1) UpdateTimers @0x82715858 (item 1). It is the ONLY writer of mbDecisionFrame in the
    //     image, and mbDecisionFrame is what IsDecisionFrame() returns once meState leaves
    //     E_STATE_STARTING_UP. Until this call existed, PostPhysicsUpdate's RUNNING arm could
    //     only ever take the NON-decision branch, so RecalculateActiveHulls / SpawnNewTraffic
    //     / the StaticVehicles_* updates were unreachable in steady state -- i.e. the
    //     "one-shot miss self-heals on the console" premise the round-4 boot evidence relies
    //     on was NOT true of this build until now. Full derivation in _wT1_06.cpp's banner.
    //
    // (2) CreateNewVehicleEntities @0x8272FA30 (item 2) -- the per-vehicle
    //     InSceneUpdateInterface::AddEntity registration. THIS FUNCTION IS ITS ONLY CALLER IN
    //     THE WHOLE IMAGE (its `xrefs_to` names exactly one entry), which is why the body and
    //     this leg had to land in the same change: either alone is dead.
    //
    // LEAK ORDER, reproduced (Feb-2007 BrnTrafficEntityModule.cpp:1145..:1174):
    //     HandleIncomingNetworkData(lpInput);
    //     if (!IsPaused() && !lbSimPaused)
    //     {
    //         UpdateTimers(lpInput);                                   <-- REAL
    //         KillDyingVehicleEntities(lpOutput);
    //         CreateNewVehicleEntities(lpOutput);                      <-- REAL
    //         UpdateCollidableVehicles(lpInput, lpOutput);
    //     }
    //     GenerateCrashedVehicleEvents(lpOutput);
    //
    // ⚠️ THE `!lbSimPaused` HALF OF THE GUARD IS NOT AVAILABLE HERE and is deliberately NOT
    // faked. PostPhysicsUpdate's asm attests bit 0 of the update set as the sim-paused bit for
    // ITSELF (0x8274E710 `clrlwi r27, r30, 31`), but PreSceneUpdate is an ARTIST EXPORT HOLE,
    // so nothing attests that THIS function decodes the same bit -- the leak says it does, and
    // the leak is rung 3. Using IsPaused() alone is the conservative direction: it runs the two
    // legs on a frame the console might have skipped, which at worst registers a scene entity
    // one frame early. Inventing the mask to skip work would be the other, silent, direction.
    // Un-gate the second half when PreSceneUpdate's body is recovered or BrnUpdateSet gets
    // named bits.
    // ========================================================================
    case E_STATE_RUNNING:
    {
        {
            // HandleIncomingNetworkData @0x82741AF8 -- no body; online-only by name and by
            // the leak's placement (it drains the network hull-sync ring).
            static bool sbLogged = false;
            LogMissingLeg(sbLogged,
                "PreSceneUpdate E_STATE_RUNNING leg HandleIncomingNetworkData @0x82741AF8 -- "
                "no body in this tree; ONLINE-only (it drains the network hull-sync ring)");
        }

        if (!IsPaused())
        {
            // ⭐ ITEM 1: the frame clock. Everything downstream of IsDecisionFrame() depends
            // on this one call.
            UpdateTimers(lpInput);

            {
                // KillDyingVehicleEntities @0x82741E40 (DWARF :1317's neighbour, declared in
                // BrnTrafficEntityModule.h as `void KillDyingVehicleEntities()` with a FLAG)
                // -- no body. It is the REMOVE half of the scene registration below and its
                // own callee KillDyingVehicleEntity @0x8272EB40 is likewise bodiless.
                // CONSEQUENCE, stated because it is the mirror of the gate in _wT1_06.cpp: a
                // traffic vehicle that dies keeps its scene entity. On the parked path that
                // cannot happen yet -- a parked car only dies through
                // StaticVehicles_KillParam, which this build reaches only in TEARING_DOWN --
                // so nothing leaks on a normal drive. It becomes load-bearing the moment
                // KillOutOfAreaTraffic lands.
                static bool sbLogged = false;
                LogMissingLeg(sbLogged,
                    "PreSceneUpdate E_STATE_RUNNING leg KillDyingVehicleEntities @0x82741E40 "
                    "-- no body (nor its callee KillDyingVehicleEntity @0x8272EB40). It is the "
                    "REMOVE half of CreateNewVehicleEntities; a dying vehicle would keep its "
                    "scene entity. Unreachable on the parked path today because parked cars "
                    "only die in TEARING_DOWN");
            }

            // ⭐⭐ ITEM 2: THE SCENE REGISTRATION. Body in
            // BrnTrafficEntityModule_wT1_05.cpp. This is the call that makes activeHulls'
            // downstream numbers ([T1-rinfo], [T1-dispatch]) able to be non-zero.
            //
            // ⚠️ MOUNT DEPENDENCE, stated so nobody reads a green selfcheck as proof: the
            // per-TU `cl /c` gate cannot see whether _wT1_05.cpp is on
            // tools/build/build_game_exe.bat. If it is not mounted this call is an LNK2019 at
            // exe link -- loud, which is the point. The bat line is conductor-owned and is in
            // this round's report.
            CreateNewVehicleEntities(lpOutput);

            {
                // UpdateCollidableVehicles @0x827302C8 -- no body; it is the collision-volume
                // half (it posts AddVolumeInstance for vehicles near the player), which is
                // wave 3 surface. A parked car with a scene entity but no collision volume is
                // visible and non-solid, which is the correct bring-up order.
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
        // Leak :1176..:1179. The whole starting-up ladder is skipped when traffic is turned
        // off. mbDEBUGTurnTrafficOff's SHIP default is FALSE (measured from Construct
        // @0x82740220 -- see the sibling partfile's banner), so this early-out does not fire
        // on a normal boot.
        if (mbDEBUGTurnTrafficOff)
        {
            break;
        }

        switch (meStartingUpState)
        {
        case E_STARTINGUPSTATE_WAITING_FOR_PLAYER:
        {
            // ================================================================
            // ⭐⭐ THE TRANSITION. Leak :1188..:1201, verbatim in structure.
            //
            //     if ((mfTrafficAmountScale > 0.0f) && !mbDEBUGTurnTrafficOff)
            //         if (!lpInput->GetActiveRaceCarOutputInterface()->IsPlayerCarActive())
            //             break;
            //     meStartingUpState = E_STARTINGUPSTATE_POPULATING;
            //
            // Note the guard's polarity, which is easy to get backwards: the player test is
            // applied ONLY when traffic density is non-zero. At density 0 the module stops
            // waiting for the player and advances immediately -- consistent with the scout
            // report's warning that a zero density changes the STATE MACHINE, not just the
            // spawn count. It is also why the sibling partfile gates FillNewHull's driving
            // half explicitly instead of zeroing mfTrafficAmountScale.
            //
            // mbDEBUGTurnTrafficOff is re-tested inside the guard exactly as the leak does,
            // even though the arm above already returned on it -- a redundancy in the
            // original, reproduced rather than "cleaned up".
            //
            // ⭐ SHIP ATTESTATION FOR THE TEST ITSELF (not the leak): the ARTIST callee
            // inventory for 0x8274A968 contains BOTH
            // RCEntityActiveRaceCarOutputInterface::IsPlayerCarActive @0x82277B90 and
            // sub_82710BD8 (== InputBuffer_PreScene::GetActiveRaceCarOutputInterface, DWARF
            // :153, returns this+0x40 under a read-lock assert at
            // BrnTrafficEntityModuleIO.h:157). This is the only place in the leak where that
            // pair appears, and IsPlayerCarActive has no other caller inside this function's
            // scope, so the ship does make this test here.
            // ================================================================
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
                // starting-up cycle and PostPhysicsUpdate leaves it on the same visit.
                // DELETE-WHEN-STABLE.
                *lpDiag << "[T1-populate] meStartingUpState -> E_STARTINGUPSTATE_POPULATING"
                        << " (density=" << mfTrafficAmountScale << ")\n";
            }
        }
        break;

        case E_STARTINGUPSTATE_POPULATING:
            // Leak :1203..:1206 -- genuinely empty. The populating work is
            // PostPhysicsUpdate's (sibling partfile), which is also what advances the state
            // on to WAITING_FOR_STREAMING.
            break;

        case E_STARTINGUPSTATE_WAITING_FOR_STREAMING:
            // Leak :1208..:1211 -- genuinely empty here too; PostPhysicsUpdate owns the
            // AreAllAssetsLoaded() latch and EnterRunningState.
            break;

        default:
            CGS_ASSERT(false, "Invalid starting up state");   // leak baked .cpp line 1224
            break;
        }
    }
    break;

    case E_STATE_TEARING_DOWN:
    {
        // Leak :1218..:1250: a three-way switch on meTearingDownState whose only non-empty
        // arm (FLUSHING) calls KillDyingVehicleEntities @0x82741E40 -- bodiless here.
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
