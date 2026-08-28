// ============================================================================
// BrnTrafficEntityModule_wQ7_01.cpp -- the traffic side of "a smashed traffic light changes
// the traffic system".
//
//   * TrafficEntityModule::HandlePropModuleRequests @0x82720A90 (118 insns)  REAL
//   * TrafficEntityModule::PrePhysicsUpdate         @0x8274C690 (120 insns)  PARTIAL
//
// The chain: the prop module's ChangePropState / SendTrafficLightRestoreEvents fill the two
// PropToTrafficInterface rings, WorldModule::BridgePropModuleToTrafficModule_PrePhysics
// @0x827AEA70 carries them into InputBuffer_PrePhysics, PrePhysicsUpdate calls
// HandlePropModuleRequests, and TrafficLightManager::TrafficLightGot{Smashed,Restored} set
// TrafficLightRuntimeState::muFlags bit 0x80.
//
// OPEN PARK: the VISIBLE consumer of bit 0x80 has no body anywhere in the tree.
// TrafficLightManager::RenderLightsForHull @0x8275DBF0 and
// ::RenderAllLightsToBeInStateForHull @0x8275DE50 skip RenderCoronasForInstance for a smashed
// light; both are driven by RenderTrafficLightCoronas @0x8271EC80. None of the three is
// reconstructed, so on this build a knock-down is provable in the log only. Do not promise a
// dark traffic light.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightManager.h"

#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"          // BrnTraffic::TrafficData (+ mTrafficLights)
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"     // CgsResource::ResourcePtr<T>
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"             // CgsDev::Log::gpDebugPrint / Message::gxMessageFilterFlags
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // PerfMonCpu::Start/StopMonitor
#include "GameShared/GameClasses/Containers/CgsBitArray.h"             // CgsContainers::BitArray<N>

#include <cstddef>   // size_t
#include <cstdlib>   // getenv

namespace BrnTraffic
{
namespace
{
    // Console displacements for the members read below, kept as attestation only. They are
    // 32-bit-pointer values and never valid on this LP64 host; every read is by name.
    //   0x300 meState (:607) . 0x304 meStartingUpState (:608) . 0x310 meTearingDownState (:611)
    //   0x53790 mTrafficLightManager (:661) . 0x71840 mpData (:752)
    //   0x717DD mbPlayingShowtimeMode (:716) . 0x729FC miPerfMon_PrePhysicsUpdate (:898)

    // BrnUpdateSet bit 0 == "the simulation did not step this frame", measured here as
    // `clrlwi r29,r29,31` on the r8 parameter at 0x8274C6C0.
    const BrnUpdateSet KU_UPDATESET_SIM_PAUSED = 0x1;

    // One-shot leg gate, one named line per console leg with no body in the tree.
    // [DIAG] NOT IN THE X360 BINARY.
    inline void LogMissingLeg(bool& lrbAlreadyLogged, const char* lpcLegNameAndAddress)
    {
        if (lrbAlreadyLogged)
        {
            return;
        }
        lrbAlreadyLogged = true;

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[Q7-traffic-leg] TrafficEntityModule::PrePhysicsUpdate leg NOT RECONSTRUCTED, skipped: "
                << lpcLegNameAndAddress << " [FLAG PC partial gate]\n";
        }
    }
}

// ============================================================================
// BrnTraffic::TrafficEntityModule::HandlePropModuleRequests  @ 0x82720A90  (118 insns)  REAL
// DWARF :1443  void HandlePropModuleRequests(const InputBuffer_PrePhysics*,
//                                            OutputBuffer_PrePhysics*)
//
// lpOutput is asserted and then never read: r31 holds it across the assert block
// (0x82720AD4..0x82720AF4) and is reused as the loop counter from `li r31,0` @0x82720B30 on.
// The parameter is in the DWARF signature and the assert is a real side effect, so both stay
// and no use is invented.
//
// The two "lpEvent" asserts collapse: the console calls GetEvent(int) out of line and
// null-checks the returned pointer, but in-tree GetEvent returns `const T&`
// (CgsBaseEventQueue.h:146), so the check has no expressible subject.
//
// The restore ring is reached by name, not by the console's +0x8C == 12 + 32*4 ==
// sizeof(EventQueue<TrafficLightKnockDownEvent,32>). The host queue's mpEvents is 8 bytes, so
// that literal is wrong here; GetTrafficLightRestoreQueue() is the same expression correctly.
//
// operator-> runs once per event inside each loop (0x82720B90 / 0x82720C38) and the loop bound
// is re-read from the queue every pass (0x82720BA8 / 0x82720C50). Neither is hoisted.
//
// The DWARF names the accessor TrafficLightRestoreEvent::GetInstanceID(); the tree spells the
// member muPayload (BrnPropToTrafficInterface.h:47/:67), which is what is used.
// ============================================================================
void TrafficEntityModule::HandlePropModuleRequests(
        const BrnTrafficIO::InputBuffer_PrePhysics* lpInput,
        BrnTrafficIO::OutputBuffer_PrePhysics*      lpOutput )
{
    typedef BrnWorld::PropEntityIO::PropToTrafficInterface        PropToTrafficInterface;
    typedef BrnWorld::PropEntityIO::TrafficLightKnockDownEvent    TrafficLightKnockDownEvent;
    typedef BrnWorld::PropEntityIO::TrafficLightRestoreEvent      TrafficLightRestoreEvent;
    typedef CgsModule::EventQueue<TrafficLightKnockDownEvent, 32> TrafficLightKnockDownQueue;
    typedef CgsModule::EventQueue<TrafficLightRestoreEvent, 80>   TrafficLightRestoreQueue;

    CGS_ASSERT( lpInput  != 0, "lpInput != NULL" );    // baked source line 0x1955 == 6485
    CGS_ASSERT( lpOutput != 0, "lpOutput != NULL" );   // baked source line 0x1956 == 6486

    // mTrafficLightManager (console +0x53790, DWARF :661) and mpData (+0x71840, :752).
    TrafficLightManager& lrTrafficLightManager = mTrafficLightManager;
    CgsResource::ResourcePtr<TrafficData>& lrData = mpData;

    // Ring 0: the traffic lights knocked down this frame. 0x82720AFC calls the const
    // (read-lock) GetPropToTrafficInterface @0x827113B8. The console asserts that pointer under
    // the QUEUE's name because the knock-down ring is the interface's first member (+0), so the
    // two addresses coincide; the console's assert string is kept.
    {
        const PropToTrafficInterface* const lpInterface = lpInput->GetPropToTrafficInterface();
        CGS_ASSERT( lpInterface != 0, "lpTrafficLightKnockDownQueue" );   // line 0x195B == 6491

        const TrafficLightKnockDownQueue* const lpTrafficLightKnockDownQueue =
            lpInterface->GetTrafficLightKnockDownQueue();

        for ( s32 liEvent = 0; liEvent < lpTrafficLightKnockDownQueue->GetLength(); ++liEvent )
        {
            // 0x82720B60 == BaseEventQueue<TrafficLightKnockDownEvent>::GetEvent(int) const,
            // stride 4. The console's "lpEvent" assert (line 6497) collapses; see the banner.
            const TrafficLightKnockDownEvent& lrEvent =
                lpTrafficLightKnockDownQueue->GetEvent( liEvent );

            // 0x82720B90 is ResourcePtr<TrafficData>::operator->() then `addi r4,r11,0x3C` for
            // TrafficData::mTrafficLights. That +0x3C is the console's 4-byte-pointer offset;
            // the host's is +0x68 (BrnTrafficDataResourceType.h static_assert), so the member
            // is reached by name.
            lrTrafficLightManager.TrafficLightGotSmashed( &lrData->mTrafficLights,
                                                          lrEvent.muPayload );
        }
    }

    // Ring 1: the traffic lights being restored. 0x82720BBC calls the same const getter again
    // (not cached), then null-checks the +0x8C-adjusted address, not the interface pointer,
    // under the string "lpTrafficLightRestoreQueue". This asserts the interface pointer
    // instead, because 0x8C is a console-only literal (the host EventQueue widens mpEvents
    // 4 -> 8). Both conditions are vacuously equivalent: `lpInterface + 0x8C` is null only if
    // lpInterface is.
    {
        const PropToTrafficInterface* const lpInterface = lpInput->GetPropToTrafficInterface();
        CGS_ASSERT( lpInterface != 0, "lpTrafficLightRestoreQueue" );     // line 0x1969 == 6505

        const TrafficLightRestoreQueue* const lpTrafficLightRestoreQueue =
            lpInterface->GetTrafficLightRestoreQueue();

        for ( s32 liEvent = 0; liEvent < lpTrafficLightRestoreQueue->GetLength(); ++liEvent )
        {
            // 0x82720C08 == BaseEventQueue<TrafficLightRestoreEvent>::GetEvent(int) const,
            // stride 4. The console's "lpEvent" assert (line 6510) collapses; see the banner.
            const TrafficLightRestoreEvent& lrEvent =
                lpTrafficLightRestoreQueue->GetEvent( liEvent );

            lrTrafficLightManager.TrafficLightGotRestored( &lrData->mTrafficLights,
                                                            lrEvent.muPayload );
        }
    }

    // [DIAG] NOT IN THE X360 BINARY. One-shot, opt-in behind BRN_PROP_DIAG. Fires the first
    // frame either ring carries anything, which is the only proof on this build that the chain
    // closed, since the corona render leg has no body. The logged instance index is the dense
    // index the manager resolved from the persistent id, read through the manager TU's diag
    // accessor; -1 means the id was not in the baked table. Every traffic-light prop posts a
    // restore on load, so the restore ring is normally the one that fires.
    {
        static const bool sbPropDiag        = ( getenv( "BRN_PROP_DIAG" ) != 0 );
        static bool       sbLoggedFirstFlip = false;

        if ( sbPropDiag && !sbLoggedFirstFlip && CgsDev::Log::gpDebugPrint != 0 )
        {
            const PropToTrafficInterface* const lpInterface = lpInput->GetPropToTrafficInterface();
            const s32 liKnockDowns = lpInterface->GetTrafficLightKnockDownQueue()->GetLength();
            const s32 liRestores   = lpInterface->GetTrafficLightRestoreQueue()->GetLength();

            if ( liKnockDowns > 0 || liRestores > 0 )
            {
                sbLoggedFirstFlip = true;
                *CgsDev::Log::gpDebugPrint
                    << "[Q7-tlight] traffic module consumed knockdown " << liKnockDowns
                    << " restore " << liRestores
                    << " -> last resolved light instance " << Q7Diag_GetLastResolvedLightIndex()
                    << ( liKnockDowns > 0 ? " smashed" : " restored" )
                    << " (instance id " << Q7Diag_GetLastResolvedLightInstanceID()
                    << ")\n";
            }
        }
    }
}

// ============================================================================
// BrnTraffic::TrafficEntityModule::PrePhysicsUpdate  @ 0x8274C690  (120 insns)  PARTIAL
//
// Every console leg is real except eight, which have no body and no declaration in the tree.
// Each is a named one-shot gate at its console position inside this body, never a call to a
// declared-but-bodyless member: `cl /c` cannot see an unresolved external, so the declaration
// alone would turn a green gate into a broken link.
//
// Parameters, from the prologue 0x8274C69C..0x8274C6B0: r3 this, r4/r5 the two IOBufferStacks
// (never touched), r6 lpInput, r7 lpOutput, r8 lUpdateSet with `clrlwi r29,r29,31` applied
// immediately.
//
// SIGNATURE DIVERGENCE: DWARF :1094 spells the third parameter const. The committed
// declaration is non-const and must stay so, since const changes the mangled name and orphans
// the caller (BrnWorldModule.cpp:1714) and the WorldLinkStubs.cpp gate. Const is honoured
// internally: HandlePropModuleRequests takes the const pointer, which is also what picks the
// const/read-lock GetPropToTrafficInterface @0x827113B8 the console calls.
//
// UNLOCK ORDER: the console releases WRITE first, then READ (0x8274C854 / 0x8274C85C), the
// opposite of PropEntityModule::PrePhysicsUpdate. Do not "fix" it.
//
// The perfmon handle is unseated on this build and that is safe: PerfMonCpu::StartMonitor is
// `if (!IsValidHandle(h)) return;` with no assert (CgsPerfMonCpu.cpp:119).
// ============================================================================
void TrafficEntityModule::PrePhysicsUpdate( CgsModule::IOBufferStack* /*lpInputBufferStack*/,
                                            CgsModule::IOBufferStack* /*lpOutputBufferStack*/,
                                            BrnTrafficIO::InputBuffer_PrePhysics*  lpInput,
                                            BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput,
                                            BrnUpdateSet lUpdateSet )
{
    // 0x8274C6B4 `lwz r3,0(r27)` with r27 == this + 0x729FC.
    CgsDev::PerfMonCpu::StartMonitor( miPerfMon_PrePhysicsUpdate );

    // 0x8274C6C4 / 0x8274C6CC -- write lock the OUTPUT, then read lock the INPUT.
    lpOutput->LockForWrite();
    lpInput->LockForRead();

    // 0x8274C6C0 `clrlwi r29,r29,31` -- the local the DWARF names lbSimPaused (:2641).
    const bool lbSimPaused = ( ( lUpdateSet & KU_UPDATESET_SIM_PAUSED ) != 0 );

    // 0x8274C6E0 republishes mbPlayingShowtimeMode (DWARF :716) into the output buffer's
    // mbPlayingShowtime (console +0x24720, BrnTrafficEntityModuleIO.h:795). Both ends by name.
    lpOutput->SetPlayingShowtime( mbPlayingShowtimeMode );

    // 0x8274C6E8. The console emits an unsigned three-way ladder, i.e. cases 0/1/2 with an
    // asserting default. E_STATE_INVALID (-1) lands in the default arm, comparing >= 3
    // unsigned.
    switch ( meState )
    {
    case E_STATE_STARTING_UP:
        // 0x8274C818: nothing runs while starting up; the arm exists only to validate
        // meStartingUpState (DWARF :608). Cases 0/1/2 are empty; the default asserts.
        switch ( meStartingUpState )
        {
        case E_STARTINGUPSTATE_WAITING_FOR_PLAYER:
        case E_STARTINGUPSTATE_POPULATING:
        case E_STARTINGUPSTATE_WAITING_FOR_STREAMING:
            break;
        default:
            CGS_ASSERT( false, "Invalid starting up state" );   // baked line 0xA6D == 2669
            break;
        }
        break;

    case E_STATE_RUNNING:
        // 0x8274C760.
        if ( lbSimPaused )
        {
            // 0x8274C80C: a paused frame runs the crashed-vehicle clean-up and nothing else.
            // LANDED (was a gate) -- body in _wT3_02.cpp.
            CleanUpCrashedVehiclePhysics( lpOutput );
        }
        else
        {
            HandlePropModuleRequests( lpInput, lpOutput );   // 0x8274C778

            // 0x8274C77C..0x8274C794: ten 64-bit zero stores over the 80-byte stack local
            // (601 bits -> 10 bit fields -> 80 bytes, CgsBitArray.h:23). DWARF names it
            // lCreatedBodies (:2661).
            // RETYPED: spelled through TrafficEntityModule::TotalTrafficBitArray
            // (BitArray<KU_MAX_TOTAL_TRAFFIC>, the ship's 600) so it matches the parameter type
            // of SendPhysicalRequests below. Same 10 bit fields, same 80 bytes; the DWARF's 601
            // is the off-by-one it also carries on the index map.
            TotalTrafficBitArray lCreatedBodies;
            lCreatedBodies.UnSetAll();

            // UN-GATED: BuildPotentialCollisionList @0x8274B378 is BODIED
            // (_wT4_02.cpp). 0x8274C7A4, the FIRST leg of this arm -- it walks the scene's raw
            // overlap-pair list and promotes every non-physical traffic half to a physics body
            // BEFORE the driver inputs are generated, so a car the player is about to hit
            // already owns a slot when contact generation runs. Mount _wT4_02.cpp in
            // tools/build/build_game_exe.bat or this call is an LNK2019 at exe link.
            BuildPotentialCollisionList( lpInput, lpOutput, &lCreatedBodies );

            // 0x8274C7B0 -- UN-GATED. The note that stood here said "no export dumped"; that
            // was FALSE, the per-function export exists with 1365 asm lines
            // (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82745218.json) and the body is now in
            // _wT5_01.cpp. It is the ONLY writer of mfJunctionFUP, so with it gated
            // NeedToTakeActionAgainstJunctionFUP() was constant false and BOTH the traffic
            // avoid arm (UpdateParams_TryAvoidCrashing) and SpawnNewTraffic's jam brake were
            // unreachable. Mount _wT5_01.cpp in tools/build/build_game_exe.bat or this call is
            // an LNK2019 at exe link.
            UpdateJunctionFUP();

            // 0x8274C7BC -- LIVE (cluster C3 owns the body).
            GenerateDriverInputs( lpOutput );

            // 0x8274C7CC -- LIVE (cluster C1, _wT3_01.cpp). lCreatedBodies stops
            // being write-only here: it is this leg's OUT parameter.
            SendPhysicalRequests( lpOutput, &lCreatedBodies );
            {
                static bool sbLogged = false;
                LogMissingLeg( sbLogged,
                    "SendEmergencyCrashEvents @0x82747BB8 (out, &lCreatedBodies)" );
            }
            {
                static bool sbLogged = false;
                LogMissingLeg( sbLogged,
                    "CreateBodiesForCrashingNetworkTraffic @0x8274B4B0 (out, &lCreatedBodies)" );
            }
            // LANDED (was a gate) -- body in _wT3_02.cpp. THIS is the leg
            // that turns the module's maNewRemovedVehicles into physics RemoveTrafficEvents,
            // i.e. the only thing that ever frees a slot in the 20-car physical pool.
            CleanUpCrashedVehiclePhysics( lpOutput );
            {
                static bool sbLogged = false;
                LogMissingLeg( sbLogged, "StoreAISceneResultsForNextFrame (in) (no export dumped)" );
            }
        }
        break;

    case E_STATE_TEARING_DOWN:
        // 0x8274C71C: validate meTearingDownState (DWARF :611) and, in the FLUSHING phase
        // only, run the crashed-vehicle clean-up.
        switch ( meTearingDownState )
        {
        case E_TEARINGDOWNSTATE_WIPING:
            break;
        case E_TEARINGDOWNSTATE_FLUSHING:
        {
            // 0x8274C750. LANDED (was a gate) -- body in _wT3_02.cpp.
            CleanUpCrashedVehiclePhysics( lpOutput );
            break;
        }
        case E_TEARINGDOWNSTATE_WAITING_TO_RESET:
            break;
        default:
            CGS_ASSERT( false, "Invalid tearing down state" );  // baked line 0xA8A == 2698
            break;
        }
        break;

    default:
        // 0x8274C700.
        CGS_ASSERT( false, "Invalid state in traffic system" ); // baked line 0xA93 == 2707
        break;
    }

    // 0x8274C854 then 0x8274C85C -- WRITE released first (see the banner).
    lpOutput->UnlockForWrite();
    lpInput->UnlockForRead();

    // 0x8274C860 `lwz r3,0(r27)`.
    CgsDev::PerfMonCpu::StopMonitor( miPerfMon_PrePhysicsUpdate );
}

}
