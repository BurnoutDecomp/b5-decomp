// ============================================================================
// BrnTrafficEntityModule_wQ7_01.cpp  --  wave Q7 / cluster `traffic`, partfile 1 of 2
//
// THE CONSUMER SIDE OF "a smashed traffic light tells the traffic system to change".
//
//   PropEntityModule::ChangePropState / PropZoneManager::SendTrafficLightRestoreEvents
//        -> PropToTrafficInterface's two rings                              [LIVE]
//   -> WorldModule::BridgePropModuleToTrafficModule_PrePhysics @0x827AEA70  [LIVE]
//        (WorldBridgePropModule.cpp:485; last drive logged
//         "[Q6-lights] first traffic-light requests -> traffic module: knockdown 0 restore 12")
//   -> BrnTrafficIO::InputBuffer_PrePhysics::mPropToTrafficInterface @+0x30C60,
//        Constructed by InputBuffer_PrePhysics::Construct @0x827615F8            [LIVE]
//   -> TrafficEntityModule::PrePhysicsUpdate @0x8274C690         <-- THIS FILE (PARTIAL)
//   -> TrafficEntityModule::HandlePropModuleRequests @0x82720A90 <-- THIS FILE (REAL)
//   -> TrafficLightManager::TrafficLightGot{Smashed,Restored}                    [REAL]
//   -> TrafficLightRuntimeState::muFlags bit 0x80
//
// THIS FILE CONTAINS:
//   * TrafficEntityModule::HandlePropModuleRequests  -- REAL, complete, 118/118 insns.
//   * TrafficEntityModule::PrePhysicsUpdate          -- the documented PARTIAL pattern:
//       every leg the console runs is REAL except EIGHT sibling legs that have no body
//       anywhere in the tree; each of those is a NAMED one-shot gate INSIDE the body
//       (never a call to a declared-but-bodyless member -- that would be a link hole the
//       per-TU `cl /c` gate cannot see, AGENTS.md gotcha 12). The eight are listed in
//       the PARTIAL banner over the function.
//
// ⚠️ THIS FILE ALONE IS CODE-COMPLETE BUT RUNTIME-DEAD, BY DESIGN AND ON PURPOSE.
//   `meState` @+0x300 is E_STATE_STARTING_UP (0) on this build -- TrafficEntityModule::
//   Construct and ::Prepare are inert gates in WorldLinkStubs.cpp, the module storage is
//   zero-initialised, and the only console writer of E_STATE_RUNNING is EnterRunningState
//   @0x827080E8, reachable only through PostPhysicsUpdate @0x8274E6D0 (581 insns) after
//   Reset @0x8272CDA0 (824 insns) -- neither reconstructed. A FAITHFUL PrePhysicsUpdate
//   therefore takes the starting-up arm and never reaches the HandlePropModuleRequests leg.
//   That is CORRECT behaviour for this file, not a bug to paper over: the bring-up that
//   makes the leg reachable (seat `mpData`, then latch the running state) lives in the
//   SIBLING partfile BrnTrafficEntityModule_wQ7_02.cpp, which the conductor mounts as a
//   SEPARATE decision because it changes the boot Prepare ladder. Mounting THIS file with
//   its gate retired is safe on its own and changes no observable behaviour.
//
// ⚠️ AND THE *VISIBLE* CONSUMER OF THE SMASHED BIT IS SEPARATELY DEAD. muFlags bit 0x80 is
//   read by TrafficLightManager::RenderLightsForHull @0x8275DBF0 (152) and
//   ::RenderAllLightsToBeInStateForHull @0x8275DE50 (90) -- both skip
//   TrafficLightCollection::RenderCoronasForInstance for a smashed light -- driven by
//   TrafficEntityModule::RenderTrafficLightCoronas @0x8271EC80 (389), driven in turn by
//   GenerateDispatchLists (itself an inert gate, WorldLinkStubs.cpp:789). NONE of those
//   three has a body in the tree. So the visible effect of a knock-down is "that light's
//   coronas stop being submitted", and on THIS build the flip is provable in the LOG ONLY.
//   Do not promise a dark traffic light.
//
// SOURCES: X360 ARTIST asm+pseudocode (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82720A90.json
// and 0x8274C690.json) for behaviour; DecFIGS DWARF (references/DecFIGS/dwarfdump/GameSource/
// World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h) for declaration shape,
// member NAMES and local names.
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
    // ------------------------------------------------------------------------
    // Byte-offset field access into the opaque module object. Identical in form to the
    // helper BrnTrafficEntityModule.cpp:38-48 already establishes for this class (the
    // "opaque external field by attested offset" allowance -- the module has no
    // reconstructable member layout, see the header banner). Every offset used below is
    // quoted with the DWARF member NAME it corresponds to and the asm site that attests it.
    // ------------------------------------------------------------------------
    template <typename T>
    inline T& FieldAt(void* lpBase, size_t luByteOffset)
    {
        return *reinterpret_cast<T*>(reinterpret_cast<u8*>(lpBase) + luByteOffset);
    }

    // ---- attested module offsets used by this file -------------------------------------
    // meState                    (DWARF :607)  -- PrePhysicsUpdate `lwz r11,0x300(r31)`;
    //                                             EnterRunningState `stw 1,0x300`; IsPaused.
    const size_t KU_OFFSET_ME_STATE              = 0x300;
    // meStartingUpState          (DWARF :608)  -- PrePhysicsUpdate `lwz r11,0x304(r31)`.
    const size_t KU_OFFSET_ME_STARTING_UP_STATE  = 0x304;
    // meTearingDownState         (DWARF :611)  -- PrePhysicsUpdate `lwz r11,0x310(r31)`.
    const size_t KU_OFFSET_ME_TEARING_DOWN_STATE = 0x310;
    // mTrafficLightManager       (DWARF :661)  -- HandlePropModuleRequests `ori r26,r10,0x3790`.
    const size_t KU_OFFSET_TRAFFIC_LIGHT_MANAGER = 0x53790;
    // mpData                     (DWARF :752)  -- HandlePropModuleRequests `ori r25,r10,0x1840`;
    //                                             also LoadData + RenderTrafficLightCoronas.
    const size_t KU_OFFSET_DATA                  = 0x71840;
    // mbPlayingShowtimeMode      (DWARF :619)  -- PrePhysicsUpdate `lbzx r11,r31,0x717DD`.
    const size_t KU_OFFSET_PLAYING_SHOWTIME_MODE = 0x717DD;
    // miPerfMon_PrePhysicsUpdate (DWARF :976)  -- PrePhysicsUpdate `addis r27,r31,7 ; addi 0x29FC`.
    const size_t KU_OFFSET_PERFMON_PREPHYSICS    = 0x729FC;

    // BrnUpdateSet bit 0 == "the simulation did not step this frame". The NAME comes from
    // the same measurement PropEntityModule_wQ_07.cpp:178 already carries; the BIT is
    // measured here too (`clrlwi r29,r29,31` on the r8 parameter at 0x8274C6C0).
    const BrnUpdateSet KU_UPDATESET_SIM_PAUSED = 0x1;

    // ------------------------------------------------------------------------
    // PARTIAL-pattern leg gate. One NAMED one-shot line per console leg that has no body
    // anywhere in the tree, logged the first time control reaches its position. Same shape
    // and same gating condition (`gxMessageFilterFlags & 1`) as the existing boot gates in
    // WorldLinkStubs.cpp, so a boot log reads uniformly.
    // [DIAG] NOT IN THE X360 BINARY.
    // ------------------------------------------------------------------------
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
//
// DWARF :1443  void HandlePropModuleRequests(const InputBuffer_PrePhysics*,
//                                            OutputBuffer_PrePhysics*)
// DWARF locals: lpTrafficLightKnockDownQueue, liEvent, lpEvent, lpTrafficLightRestoreQueue.
//
// REGISTER -> PARAMETER MAP (prologue 0x82720A9C..0x82720AA8, measured):
//   r3 -> r24 this | r4 -> r23 lpInput | r5 -> r31 lpOutput
// Two constants are hoisted into the prologue and re-formed per iteration:
//   r25 = 0x71840 (mpData), r26 = 0x53790 (mTrafficLightManager);
//   r28 = this + r25, r27 = this + r26 recomputed at the top of EACH ring's loop
//   (0x82720B50/54 and 0x82720BF8/FC).
//
// ⚠️ lpOutput IS ASSERTED AND THEN NEVER READ. Its assert block runs 0x82720AD4..0x82720AF4
//    (`cmplwi cr6,r31,0` .. `bl EndAssert`); r31 is then clobbered by `li r31,0` at
//    0x82720B30 -- 0x3C bytes / fifteen instructions past the end of that block, and r31 is
//    re-used from there on as the loop counter. The parameter is part of the DWARF signature
//    and the assert is a real console side effect, so both stay; NO use is added.
//
// ⚠️ THE TWO "lpEvent" ASSERTS COLLAPSE. The console calls the per-instantiation
//    BaseEventQueue<T>::GetEvent(int) out of line (0x82709FA0 / 0x8270A048), gets a pointer
//    back and null-checks it. In-tree GetEvent returns `const T&` (CgsBaseEventQueue.h:146,
//    the DWARF-attested shape), so the check has no expressible subject. Stated here rather
//    than faked with a pointer round-trip.
//
// ⚠️ THE RESTORE RING IS REACHED BY NAME, NOT BY +0x8C. The console re-calls
//    GetPropToTrafficInterface() and adds the literal 0x8C == 12 + 32*4 ==
//    sizeof(EventQueue<TrafficLightKnockDownEvent,32>) on the CONSOLE. On the x64 host that
//    queue carries an 8-byte mpEvents, so 0x8C is the wrong number (gotcha 1); the member
//    accessor GetTrafficLightRestoreQueue() is the same expression, correctly.
//
// ⚠️ operator-> IS CALLED INSIDE EACH ITERATION, once per event (0x82720B90 / 0x82720C38),
//    and the loop bound is RE-READ from the queue every pass (`lwz r11,8(r29)` at 0x82720BA8
//    / 0x82720C50). Both reproduced; neither hoisted.
//
// PAYLOAD SPELLING: the DWARF names the accessor TrafficLightRestoreEvent::GetInstanceID();
// the committed tree spells the member muPayload (BrnPropToTrafficInterface.h:47/:67, with
// the divergence already documented there). muPayload is used.
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

    // mTrafficLightManager @ +0x53790 and mpData @ +0x71840 (see the offset table above).
    TrafficLightManager& lrTrafficLightManager =
        FieldAt<TrafficLightManager>( this, KU_OFFSET_TRAFFIC_LIGHT_MANAGER );
    CgsResource::ResourcePtr<TrafficData>& lrData =
        FieldAt< CgsResource::ResourcePtr<TrafficData> >( this, KU_OFFSET_DATA );

    // ---- ring 0: the traffic lights that were knocked down this frame ---------------
    // 0x82720AFC `bl BrnTraffic__BrnTrafficIO__InputBuffer_PreP` == the CONST (read-lock)
    // GetPropToTrafficInterface @0x827113B8, returning &mPropToTrafficInterface (this+199776).
    // The console asserts THAT pointer under the QUEUE's name because the knock-down ring is
    // the interface's FIRST member (+0), so the two addresses coincide -- which is also why it
    // never calls a getter here. Reproduced with the console's assert string.
    {
        const PropToTrafficInterface* const lpInterface = lpInput->GetPropToTrafficInterface();
        CGS_ASSERT( lpInterface != 0, "lpTrafficLightKnockDownQueue" );   // line 0x195B == 6491

        const TrafficLightKnockDownQueue* const lpTrafficLightKnockDownQueue =
            lpInterface->GetTrafficLightKnockDownQueue();

        for ( s32 liEvent = 0; liEvent < lpTrafficLightKnockDownQueue->GetLength(); ++liEvent )
        {
            // 0x82720B60 `bl sub_82709FA0` ==
            // BaseEventQueue<TrafficLightKnockDownEvent>::GetEvent(int) const, stride 4.
            // (the console's "lpEvent" assert, line 0x1961 == 6497, collapses -- see banner)
            const TrafficLightKnockDownEvent& lrEvent =
                lpTrafficLightKnockDownQueue->GetEvent( liEvent );

            // 0x82720B90 `bl BrnTraffic__TrafficData___operator__` then `addi r4,r11,0x3C`:
            // ResourcePtr<TrafficData>::operator->() + TrafficData::mTrafficLights. The console's
            // +0x3C is its 4-byte-pointer offset for that member; on the host it is +0x68
            // (BrnTrafficDataResourceType.h static_assert), so the member is reached BY NAME.
            lrTrafficLightManager.TrafficLightGotSmashed( &lrData->mTrafficLights,
                                                          lrEvent.muPayload );
        }
    }

    // ---- ring 1: the traffic lights being restored ----------------------------------
    // 0x82720BBC: the SAME const getter is called a second time (not cached), then
    // `addi r29,r3,0x8C` (0x82720BC0) -- and it is THAT +0x8C-adjusted address, not the
    // interface pointer, that the console null-checks at 0x82720BC4 under the string
    // "lpTrafficLightRestoreQueue". The reconstruction asserts the INTERFACE pointer instead,
    // because 0x8C is a console-only literal (gotcha 1: the host EventQueue widens mpEvents
    // 4->8) and the host has no expression for "the restore queue's address" that is not just
    // the getter below. Both conditions are vacuously true -- `lpInterface + 0x8C` can only be
    // NULL if lpInterface is -- so the subject swap is documented, not behavioural. Contrast
    // the knock-down ring above, where the two addresses genuinely coincide (+0).
    {
        const PropToTrafficInterface* const lpInterface = lpInput->GetPropToTrafficInterface();
        CGS_ASSERT( lpInterface != 0, "lpTrafficLightRestoreQueue" );     // line 0x1969 == 6505

        const TrafficLightRestoreQueue* const lpTrafficLightRestoreQueue =
            lpInterface->GetTrafficLightRestoreQueue();

        for ( s32 liEvent = 0; liEvent < lpTrafficLightRestoreQueue->GetLength(); ++liEvent )
        {
            // 0x82720C08 `bl sub_8270A048` ==
            // BaseEventQueue<TrafficLightRestoreEvent>::GetEvent(int) const, stride 4.
            // (the console's "lpEvent" assert, line 0x196E == 6510, collapses -- see banner)
            const TrafficLightRestoreEvent& lrEvent =
                lpTrafficLightRestoreQueue->GetEvent( liEvent );

            lrTrafficLightManager.TrafficLightGotRestored( &lrData->mTrafficLights,
                                                            lrEvent.muPayload );
        }
    }

    // ---------------------------------------------------------------------------------
    // [DIAG] NOT IN THE X360 BINARY. Opt-in behind BRN_PROP_DIAG, ONE-SHOT, ever.
    // Fires the first frame either ring carried anything, i.e. the first time the traffic
    // module actually CONSUMES a prop-side traffic-light request. This is the only proof
    // available on this build that the chain closed (the corona render leg that would show
    // it on screen has no body -- see the file banner).
    //
    // `I` is the DENSE instance index the manager resolved from the persistent instance ID,
    // read back through the manager TU's diag accessor: the id->index hash lives on the
    // Junctions/ copy of TrafficLightCollection, which this TU cannot include (it holds the
    // SharedClasses/Traffic/ copy by value inside TrafficData -- see the duplicate-type
    // report BL-1 in scratchpad/waveQ7/traffic.owner.md). -1 means the id was not in the
    // baked table. With the current route the RESTORE ring is the one that fires (every
    // traffic-light prop posts a restore on LOAD, PropZoneManager::LoadProp), so `I` is
    // normally the last restored light unless the drive actually struck one.
    // ---------------------------------------------------------------------------------
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
// BrnTraffic::TrafficEntityModule::PrePhysicsUpdate  @ 0x8274C690  (120 insns)
//                                                                        *** PARTIAL ***
// PARTIAL PATTERN (AGENTS.md gotcha 18). Every console leg is REAL except the EIGHT below,
// which have NO body and NO declaration anywhere in the tree. Each is a NAMED one-shot gate
// at its exact console position INSIDE this body -- deliberately NOT a call to a
// declared-but-bodyless member, because `cl /c` cannot see an unresolved external and the
// declaration alone would turn a green gate into a broken link (gotcha 12).
//
//   1 BuildPotentialCollisionList          @0x8274B378
//   2 UpdateJunctionFUP                     (no export dumped)
//   3 GenerateDriverInputs                 @0x82748E78
//   4 SendPhysicalRequests                 @0x8274C510
//   5 SendEmergencyCrashEvents             @0x82747BB8
//   6 CreateBodiesForCrashingNetworkTraffic@0x8274B4B0
//   7 CleanUpCrashedVehiclePhysics         @0x82720960   (THREE call positions: the paused
//                                                         running arm, the tearing-down arm,
//                                                         and the tail of the normal arm)
//   8 StoreAISceneResultsForNextFrame       (no export dumped)
//
// REAL here: the PerfMon bracket, both lock brackets, SetPlayingShowtime, the complete
// three-way state machine with all FOUR console asserts, the BitArray<601> local + UnSetAll,
// and HandlePropModuleRequests.
//
// REGISTER -> PARAMETER MAP (prologue 0x8274C69C..0x8274C6B0, measured):
//   r3 -> r31 this | r4 UNUSED | r5 UNUSED | r6 -> r28 lpInput | r7 -> r30 lpOutput
//   | r8 -> r29 lUpdateSet, immediately `clrlwi r29,r29,31` == lUpdateSet & 1.
// r4/r5 are the two IOBufferStacks; this body never touches them (WorldModule creates and
// destroys the buffers around the call). NOT the float-skips-a-GPR case (gotcha 3).
//
// ⚠️ SIGNATURE: DWARF :1094 spells the third parameter `const InputBuffer_PrePhysics*`. The
//    COMMITTED declaration (BrnTrafficEntityModule.h) is non-const and MUST STAY non-const:
//    adding const changes the mangled name and orphans both the caller
//    (BrnWorldModule.cpp:1714) and the WorldLinkStubs.cpp gate. Recorded as a divergence to
//    note, not to chase. The const is honoured internally -- HandlePropModuleRequests takes
//    the DWARF's const pointer, which is also what picks the const/read-lock
//    GetPropToTrafficInterface @0x827113B8 the console calls.
//
// ⚠️ UNLOCK ORDER: the console releases WRITE first, then READ (0x8274C854 / 0x8274C85C) --
//    the OPPOSITE order to PropEntityModule::PrePhysicsUpdate. Reproduced. Do not "fix" it.
//
// ⚠️ THE PERFMON HANDLE IS UNSEATED ON THIS BUILD (miPerfMon_PrePhysicsUpdate @+0x729FC is 0
//    because the module Construct is an inert gate) AND THAT IS SAFE: PerfMonCpu::StartMonitor
//    is `if (!IsValidHandle(h)) return;` with no assert (CgsPerfMonCpu.cpp:119). Calling it is
//    faithful and costs nothing; suppressing it would be the deviation.
// ============================================================================
void TrafficEntityModule::PrePhysicsUpdate( CgsModule::IOBufferStack* /*lpInputBufferStack*/,
                                            CgsModule::IOBufferStack* /*lpOutputBufferStack*/,
                                            BrnTrafficIO::InputBuffer_PrePhysics*  lpInput,
                                            BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput,
                                            BrnUpdateSet lUpdateSet )
{
    // 0x8274C6B4 `lwz r3,0(r27)` with r27 == this + 0x729FC.
    CgsDev::PerfMonCpu::StartMonitor( FieldAt<s32>( this, KU_OFFSET_PERFMON_PREPHYSICS ) );

    // 0x8274C6C4 / 0x8274C6CC -- write lock the OUTPUT, then read lock the INPUT.
    lpOutput->LockForWrite();
    lpInput->LockForRead();

    // 0x8274C6C0 `clrlwi r29,r29,31` -- the local the DWARF names lbSimPaused (:2641).
    const bool lbSimPaused = ( ( lUpdateSet & KU_UPDATESET_SIM_PAUSED ) != 0 );

    // 0x8274C6E0/E4 `lbzx r11,r31,0x717DD ; stbx r11,r30,0x24720` -- mbPlayingShowtimeMode
    // (DWARF :619) is republished into the output buffer's mbPlayingShowtime (console +149280
    // == 0x24720, BrnTrafficEntityModuleIO.h:795). Both ends reached by name.
    lpOutput->SetPlayingShowtime( FieldAt<u8>( this, KU_OFFSET_PLAYING_SHOWTIME_MODE ) != 0 );

    // ---- 0x8274C6E8: switch (meState) ----------------------------------------------
    // The console emits the unsigned three-way ladder `cmplwi 1 / blt / beq / cmplwi 3 / blt`,
    // i.e. a switch with cases 0/1/2 and an asserting default. E_STATE_INVALID (-1) lands in
    // the default arm on both sides (it compares as >= 3 unsigned).
    switch ( FieldAt<s32>( this, KU_OFFSET_ME_STATE ) )
    {
    case E_STATE_STARTING_UP:
        // 0x8274C818: nothing runs while starting up; the arm exists only to validate
        // meStartingUpState (DWARF :608). Cases 0/1/2 are empty; the default asserts.
        switch ( FieldAt<s32>( this, KU_OFFSET_ME_STARTING_UP_STATE ) )
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
            static bool sbLoggedCleanUpPaused = false;
            LogMissingLeg( sbLoggedCleanUpPaused,
                           "CleanUpCrashedVehiclePhysics @0x82720960 (paused-running arm)" );
        }
        else
        {
            // ---- 0x8274C778: THE LEG THIS WAVE EXISTS FOR ---------------------------
            HandlePropModuleRequests( lpInput, lpOutput );

            // 0x8274C77C..0x8274C794: ten 64-bit zero stores over the 80-byte stack local.
            // 601 bits -> kuNumberOfBitFields == 10 -> exactly 80 bytes (CgsBitArray.h:23).
            // DWARF names the local lCreatedBodies (:2661); it is an OUT parameter of legs
            // 1/4/5/6 below, all of which are gated, so nothing reads it back yet -- it is
            // kept REAL because it is cheap and it keeps the local's identity honest.
            CgsContainers::BitArray<601u> lCreatedBodies;
            lCreatedBodies.UnSetAll();

            {
                static bool sbLogged = false;
                LogMissingLeg( sbLogged,
                    "BuildPotentialCollisionList @0x8274B378 (in, out, &lCreatedBodies)" );
            }
            {
                static bool sbLogged = false;
                LogMissingLeg( sbLogged, "UpdateJunctionFUP (no export dumped)" );
            }
            {
                static bool sbLogged = false;
                LogMissingLeg( sbLogged, "GenerateDriverInputs @0x82748E78 (out)" );
            }
            {
                static bool sbLogged = false;
                LogMissingLeg( sbLogged,
                    "SendPhysicalRequests @0x8274C510 (out, &lCreatedBodies)" );
            }
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
            {
                static bool sbLogged = false;
                LogMissingLeg( sbLogged,
                    "CleanUpCrashedVehiclePhysics @0x82720960 (normal-running arm)" );
            }
            {
                static bool sbLogged = false;
                LogMissingLeg( sbLogged, "StoreAISceneResultsForNextFrame (in) (no export dumped)" );
            }
        }
        break;

    case E_STATE_TEARING_DOWN:
        // 0x8274C71C: validate meTearingDownState (DWARF :611) and, in the FLUSHING phase
        // only, run the crashed-vehicle clean-up.
        switch ( FieldAt<s32>( this, KU_OFFSET_ME_TEARING_DOWN_STATE ) )
        {
        case E_TEARINGDOWNSTATE_WIPING:
            break;
        case E_TEARINGDOWNSTATE_FLUSHING:
        {
            // 0x8274C750.
            static bool sbLoggedCleanUpTearDown = false;
            LogMissingLeg( sbLoggedCleanUpTearDown,
                           "CleanUpCrashedVehiclePhysics @0x82720960 (tearing-down arm)" );
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
    CgsDev::PerfMonCpu::StopMonitor( FieldAt<s32>( this, KU_OFFSET_PERFMON_PREPHYSICS ) );
}

}
