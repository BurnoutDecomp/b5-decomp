// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModule_Streaming.cpp
//
// BrnWorld::PropEntityModule -- the PROP STREAMING half of the module: the per-frame
// state machine that decides which prop zones should be resident, requests the missing
// zones' instance data from the GameData module, and drives PropZoneManager's
// LoadZone/UnloadZone (the spawner landed in BrnPropZoneManager.cpp).
//
// Landed 2026-08-12 (prop-spawn wave, phase 2, agent B2). Sibling TU of
// BrnPropEntityModule.cpp (lifecycle, agent B1) and BrnPropEntityModule_Render.cpp
// (dispatch, agent B3); all three define members of the ONE class declared in
// BrnPropEntityModule.h.
//
// Functions in this TU (X360 BURNOUT_X360_ARTIST.XEX):
//   PropEntityModule::UpdateInstanceStreaming  @0x82308330  (1007 insns)  the state machine
//   PropEntityModule::GenerateTargetList       @0x822DA730  (  63 insns)
//   PropEntityModule::LoadZone                 @0x823035D0  ( 112 insns)
//   PropEntityModule::UnloadZone               @0x82306FD0  (  99 insns)
//   PropEntityModule::ResetProps               (header inline; see its banner)
//
// PARKED (declared in BrnPropEntityModule.h, deliberately NOT bodied here) -- see the
// PARK LIST at the bottom of this banner.
//
// ---- THE STATE MACHINE ------------------------------------------------------------
// One call per frame from PreSceneUpdate @0x82309A40:
//     UpdateInstanceStreaming( lpInput->GetPropInstancesNeededForZoneQueue(),
//                              lpInput->miPlayerZoneNumber,     // the +0x788 word
//                              lpOutput );
//
//   [A] meStreamingMode == E_RESET_UNLOADING -> HARD RESET, in place:
//         log "RESETTING ALL PROPS\n", drop every serialiser zone record (unless the
//         replay is playing back), PropZoneManager::RemoveAllPropsAndParts, then
//         meStreamingMode = E_STREAM, mLoadedZones.Clear(), muNumberOfLoadedZones = 0,
//         maRecentlyBrokenProps.Clear().  Falls straight through into [B].
//
//   [B] GenerateTargetList -> lTargetZones (see that function; in E_STREAM it is the
//       set of zone ids the world module said it NEEDS this frame).
//
//   [C] tripwire: every zone in mLoadedZones must really be loaded in the zone manager.
//
//   [D] lZonesToUnload = mLoadedZones \ lTargetZones.  If NON-EMPTY: unload exactly ONE
//       zone (element [0]) inside the miUnloadingPM perf monitor and RETURN. Nothing
//       loads on a frame that unloads -- at most one zone transition per frame.
//
//   [E] lZonesToLoad = lTargetZones \ mLoadedZones.
//       mbStreamingSettled = ( lZonesToLoad is empty )   <-- the X360-only byte at
//       +0xD3204 that PrepareForReplay / RestoreFromReplay gate on. This is the ONLY
//       writer, which is what attests B1's descriptive name.
//
//   [F] if liZoneIndex is a real zone AND it is not loaded yet -> try to request THAT
//       zone's instances (the player's own zone jumps the queue) and stop.
//       Otherwise -> walk lZonesToLoad in order and request the FIRST zone that is
//       ready, then stop. At most ONE GetPropInstances request is issued per frame.
//
//   A request is "ready" only when   !mabWaitingForInstances.IsBitSet( zone )
//                              and    mabLoadedWorldGraphics.IsBitSet( zone )
//   -- i.e. we are not already waiting on that zone, and the world graphics for it are
//   resident (PreSceneUpdate sets mabLoadedWorldGraphics from the world module's
//   PropGraphicsLoaded events). Issuing the request SETS mabWaitingForInstances; the
//   reply (GameData event type 62, "PRP_INST_<zone>") is drained by PreSceneUpdate,
//   which calls LoadZone and clears the bit.
//
// ---- LAYOUT DISCIPLINE -------------------------------------------------------------
// Not one console byte offset appears in the code below; every access is by named
// member. The offset -> member decode used to READ the asm is recorded per function.
//
// ---- PARK LIST (no body here; reason each) -----------------------------------------
//  * PropEntityModule::PreSceneUpdate @0x82309A40 (2289 insns) -- PARKED. It is a TU of
//    its own, and it cannot be bodied from this lane: it needs ~10 accessors/members that
//    do not exist on PropEntityIO::InputBuffer_PreScene yet (the three input queues
//    @0x822B90A8 / 0x822B9150 / 0x822B91F8, the +0x6F0 player position, the +0x700..0x77F
//    8-entry race-car velocity array, the +0x788 player zone number, the +0x78C player
//    index, the +0x793/+0x794 crashing/wrecked bytes, and the +0x780 hit-props word
//    retyped to a `const HitPropsBitArray*`), and its remaining legs pull in
//    PropCellManager contact generation, Nicotine::DMixIO and BrnGui::OverheadSignScore.
//    The full decode is in the agent report -- inventing it here would be fabrication.
//  * PropEntityModule::BreakPropIntoParts @0x822FB000 (168 insns) -- PARKED. Physical /
//    breakable props are explicitly out of scope for this wave, and the body needs
//    PropEntityInstance::GetState/SetState, PropVolumeInstanceID::SetPropEntityId,
//    PropEntityID::SetPartIndex and PropCellManager::{Remove,Add}Prop*ContactGeneration,
//    none of which are declared in the committed headers this lane may touch.
//  * PropEntityModule::UpdateProps @0x822FB2A0 (148 insns) -- PARKED. Same wave scope,
//    plus it calls PropZoneManager::UpdateAnimation, which the committed
//    BrnPropZoneManager.h does not declare (and this lane must not edit it), and its
//    `lpUpdatePropEventQueue` parameter is still a `const void*` placeholder.
// ============================================================================

#include "BrnPropEntityModule.h"

#include "BrnPropEntityModuleIO.h"   // PropEntityIO::OutputBuffer_PreScene
#include "BrnPropZoneManager.h"      // PropZoneManager, KU_MAX_ZONES
#include "SharedIO/BrnPropGraphicsAndZoneEvents.h"   // PropInstancesNeededForZoneEvent

#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsSet.h"                    // ::Set<T,N>
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                 // CgsModule::EventQueue
#include "GameShared/GameClasses/Development/Log/CgsLog.h"               // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // PerfMonCpu::Start/StopMonitor
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"        // RequestInterface<1024>::GetPropInstances
#include "GameSource/Replays/Serialisers/BrnReplayPropEntitySerialiser.h" // PropEntitySerialiser
#include "SharedClasses/Physics/Props/BrnPhysicsPropZoneData.h"          // PropZoneData::GetZoneId
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"        // PropPhysicsDataHeader (ResourcePtr<T>)

namespace BrnWorld
{
    namespace
    {
        // ⭐ 2026-08-12 (agent B5): the file-local `AsNeededQueue()` re-typing helper that
        // stood here is GONE. It existed only because BrnPropEntityModule.h modelled the
        // "prop instances needed for zone" queue as an incomplete forward-declared class;
        // that declaration now names the real instantiation
        // (PropEntityModule::PropInstancesNeededForZoneQueue ==
        //  CgsModule::EventQueue<PropEntityIO::PropInstancesNeededForZoneEvent,30>), so the
        // parameter arrives already typed and the reinterpret_cast is unnecessary. The two
        // call sites below use lpNeeded directly.

        // The pre-scene output buffer models its interface members as opaque sized spans
        // (BrnPropEntityModuleIO.h owns that decision, and it is another group's file).
        // Re-type the handle exactly the way BrnPropEntityModule.cpp and
        // BrnPropCellManager.cpp already do -- same idiom, same reason.
        BrnResource::GameDataIO::RequestInterface<1024>*
        GetGameDataRequestInterface(PropEntityIO::OutputBuffer_PreScene* lpOutput)
        {
            return reinterpret_cast<BrnResource::GameDataIO::RequestInterface<1024>*>(
                lpOutput->GetResourceRequestInterface());
        }

        // GetPropInstances' two literal arguments at both X360 call sites
        // (`li r5,0 ; mr r6,zone ; li r7,3`): event id 0, resource pool 3.
        const s32 KI_PROP_INSTANCES_EVENT_ID = 0;
        const s32 KI_PROP_INSTANCES_POOL_ID  = 3;

        // BrnPhysics::Props::KU_MAX_LOADED_ZONES -- the number of prop-pool zone slots.
        // Pinned by LoadZone's own tripwire (`cmplwi r10, 9 ; ble` guarding
        // "muZonesLoaded <= BrnPhysics::Props::KU_MAX_LOADED_ZONES") and equal to
        // PropZoneManager::KU_NUM_ZONE_SLOTS / PropSerialiserFrame::KU_MAX_LOADED_ZONES.
        const u32 KU_MAX_LOADED_ZONES = 9;

        // UpdateInstanceStreaming's "no priority zone this frame" sentinel
        // (`cmpwi cr6, r23, -1 ; beq` at 0x82308938).
        const s32 KI_NO_PRIORITY_ZONE = -1;

        // ---- value tripwires for the facts this TU relies on -------------------------
        // (Nothing here keys on a byte offset; these pin the SHAPES the bodies assume.)
        static_assert(sizeof(PropEntityIO::PropInstancesNeededForZoneEvent) == 2,
                      "PropInstancesNeededForZoneEvent is the bare u16 zone id");
        static_assert(PropEntityModule::PropInstancesNeededForZoneQueue::KI_LENGTH == 30,
                      "EventQueue<PropInstancesNeededForZoneEvent,30> (producer's typedef)");
        static_assert(PropEntityModule::KI_ZONE_SET_SIZE == 15,
                      "Set<u16,15> -- the X360 SetDifference<15,15> instantiation");
        static_assert(KU_MAX_ZONES == 500,
                      "the 0x1F4 bound every zone-index tripwire in this TU quotes");
        static_assert(KU_NUM_ZONE_SLOTS == KU_MAX_LOADED_ZONES,
                      "one prop-pool slot per simultaneously-loaded zone");

        // ------------------------------------------------------------------
        // The prop-instance request the X360 emits at BOTH ends of
        // UpdateInstanceStreaming's load leg -- out of line at 0x82308BCC (the priority
        // zone) and INLINED at 0x82309144 (the to-load scan). Identical instruction for
        // instruction, so it is de-inlined here per the project convention.
        //
        // Returns true when the request was issued (the scan stops on the first one).
        //
        // FAITHFUL QUIRK: mabWaitingForInstances.SetBit( zone ) is emitted TWICE, once
        // before the request and once after -- two complete inlined SetBit expansions,
        // each with its own CgsBitArray.h:222 bounds tripwire
        // (0x82308B20..0x82308BC8 + 0x82308BE4..0x82308C7C, and again
        // 0x82309024..0x82309160 + 0x823091C4..0x823092E0). The operation is idempotent,
        // so it is behaviour-neutral, but it is what the binary does and it is kept.
        // ------------------------------------------------------------------
        bool RequestPropInstancesForZone(PropEntityModule& lrModule,
                                         u32 luZoneId,
                                         PropEntityIO::OutputBuffer_PreScene* lpOutput)
        {
            if (lrModule.mabWaitingForInstances.IsBitSet(luZoneId))
            {
                return false;   // already asked for this zone; nothing to do
            }

            if (!lrModule.mabLoadedWorldGraphics.IsBitSet(luZoneId))
            {
                return false;   // the zone's world graphics are not resident yet
            }

            lrModule.mabWaitingForInstances.SetBit(luZoneId);

            // DIAGNOSTIC (prop-spawn wave 2026-08-12) -- NOT in the X360 binary. One-shot
            // proof that the streaming state machine got as far as asking for real prop
            // data. Boot-log grep: "PROPS-BOOT first instance request".
            {
                static bool sbLoggedFirstRequest = false;
                if (!sbLoggedFirstRequest && CgsDev::Log::gpDebugPrint != 0)
                {
                    sbLoggedFirstRequest = true;
                    *CgsDev::Log::gpDebugPrint
                        << "PROPS-BOOT first instance request: zone " << luZoneId << "\n";
                }
            }

            GetGameDataRequestInterface(lpOutput)->GetPropInstances(
                &lrModule.mReceiverQueue,
                KI_PROP_INSTANCES_EVENT_ID,
                static_cast<s32>(luZoneId),
                KI_PROP_INSTANCES_POOL_ID);

            lrModule.mabWaitingForInstances.SetBit(luZoneId);   // see FAITHFUL QUIRK above

            return true;
        }
    }

    // ========================================================================
    // PropEntityModule::GenerateTargetList   @ 0x822DA730   (63 insns)
    // ------------------------------------------------------------------------
    // Build the set of prop zones that SHOULD be resident this frame, and advance the
    // two "reset" states once everything has finished unloading.
    //
    // Offset decode (console, provenance only): r3+0xD3200 == meStreamingMode (the
    // switch selector, a real 6-entry jump table at jpt_822DA774), r3+0xD320C ==
    // muNumberOfLoadedZones, `stw 0, 0x20(r5)` == lrTargetZones.Clear() (Set<u16,15>'s
    // muLength sits past the 15 u16 elements + 2 bytes of pad), `lwz 8(r4)` ==
    // BaseEventQueue::miLength.
    //
    // NOTE the clear happens BEFORE the switch, so E_DONT_STREAM /
    // E_REQUESTING_PROFILE_DATA / E_WAITING_FOR_PROFILE_DATA all yield an EMPTY target
    // set -- which is what makes UpdateInstanceStreaming drain every loaded zone one per
    // frame in those states. That is deliberate, not a missing case.
    // ========================================================================
    void PropEntityModule::GenerateTargetList(
            const PropInstancesNeededForZoneQueue* lpNeeded,
            PropZonesSet& lrTargetZones )
    {
        lrTargetZones.Clear();

        switch ( meStreamingMode )
        {
        case E_STREAM:
            {
                const s32 liNumZonesNeeded = lpNeeded->GetLength();

                for ( s32 liZone = 0; liZone < liNumZonesNeeded; ++liZone )
                {
                    lrTargetZones.Insert( lpNeeded->GetEvent( liZone ).muZoneId );
                }
            }
            break;

        // Nothing is wanted in these three -- the target set stays empty.
        case E_DONT_STREAM:
        case E_REQUESTING_PROFILE_DATA:
        case E_WAITING_FOR_PROFILE_DATA:
            break;

        // "Unload everything, then resume streaming."
        case E_RESET_UNLOADING:
            if ( muNumberOfLoadedZones == 0 )
            {
                meStreamingMode = E_STREAM;
            }
            break;

        // "Unload everything, then go and fetch the profile's prop progression."
        case E_RESET_UNLOADING_FOR_PROFILE:
            if ( muNumberOfLoadedZones == 0 )
            {
                meStreamingMode = E_REQUESTING_PROFILE_DATA;
            }
            break;

        default:
            CGS_ASSERT( false, "Bad streaming state\n" );   // BrnPropEntityModule.cpp:967
            break;
        }
    }

    // ========================================================================
    // PropEntityModule::LoadZone   @ 0x823035D0   (112 insns)
    // ------------------------------------------------------------------------
    // The module-level wrapper around PropZoneManager::LoadZone (the real spawner):
    // bookkeeping + the replay record, then hand the zone to the manager with the
    // player's position so it can apply the "don't pop a prop on top of the player"
    // gate, and finally remember the zone as loaded.
    //
    // Offset decode (console, provenance only): r29+0x280 == mZoneManager,
    // r29+0xD3180 == mPropEntitySerialiser (`lwz 0(...)` == BaseSerialiser::meMode; the
    // 4/5/6 test is IsPlaying @0x821F3440 verbatim, inlined twice), r29+0xD320C ==
    // muNumberOfLoadedZones, r29+0xD3220 == mPlayerPosition (`lvx128 v127` -> LoadZone's
    // v1), r29+0xCDD88 == mpPropPhysicsDataHeader (`sub_822CA1E8` ==
    // ResourcePtr<T>::GetMemoryResource), r29+0xD337C == mLoadedZones, and
    // `lhz 0x18(r28)` == PropZoneData::GetZoneId().
    // ========================================================================
    void PropEntityModule::LoadZone( PropZoneData* lpZoneData,
                                     PropEntityIO::OutputBuffer_PreScene* lpOutput )
    {
        CGS_ASSERT( !mZoneManager.IsZoneLoaded( lpZoneData->GetZoneId() ),
                    "!mZoneManager.IsZoneLoaded( lpZoneData->GetZoneId() )" );  // cpp:2182

        // Only the RECORDING side keeps the loaded-zone table; on playback the table
        // comes out of the replay stream instead.
        if ( !mPropEntitySerialiser.IsPlaying() )
        {
            mPropEntitySerialiser.AddLoadedZone( lpZoneData->GetZoneId() );
        }

        ++muNumberOfLoadedZones;
        CGS_ASSERT( muNumberOfLoadedZones <= KU_MAX_LOADED_ZONES,
                    "muZonesLoaded <= BrnPhysics::Props::KU_MAX_LOADED_ZONES" );  // cpp:2192

        if ( CgsDev::Message::gxMessageFilterFlags & 1 )
        {
            *CgsDev::Log::gpDebugPrint
                << "PROPS Loading zone: " << static_cast<u32>( lpZoneData->GetZoneId() ) << "\n";
        }

        // DIAGNOSTIC (prop-spawn wave 2026-08-12) -- NOT in the X360 binary. The line
        // above is gated on the debug message filter, which is off in a default PC boot;
        // this one-shot is not, so a boot log always shows whether ANY zone ever loaded.
        // Boot-log grep: "PROPS-BOOT first zone load".
        {
            static bool sbLoggedFirstZoneLoad = false;
            if ( !sbLoggedFirstZoneLoad && CgsDev::Log::gpDebugPrint != 0 )
            {
                sbLoggedFirstZoneLoad = true;
                *CgsDev::Log::gpDebugPrint
                    << "PROPS-BOOT first zone load: zone " << static_cast<u32>( lpZoneData->GetZoneId() )
                    << " props " << lpZoneData->GetNumberOfProps()
                    << " instances " << lpZoneData->GetNumberOfInstances() << "\n";
            }
        }

        mZoneManager.LoadZone( lpZoneData,
                               mpPropPhysicsDataHeader.GetMemoryResource(),
                               mPlayerPosition,
                               lpOutput,
                               mPropEntitySerialiser.IsPlaying(),
                               &mPropEntitySerialiser );

        mLoadedZones.Insert( lpZoneData->GetZoneId() );
    }

    // ========================================================================
    // PropEntityModule::UnloadZone   @ 0x82306FD0   (99 insns)
    // ------------------------------------------------------------------------
    // The mirror of LoadZone.
    //
    // Offset decode (console, provenance only): r31+0x280 == mZoneManager,
    // r31+0xD320C == muNumberOfLoadedZones, r31+0xCDDE4 == maRecentlyBrokenProps,
    // r31+0xD3334 == mbInReplay, r31+0xD3180 == mPropEntitySerialiser,
    // r31+0xD337C == mLoadedZones, r31+0xCDD9C == mpPropPhysicsDataHeader's handle.
    //
    // TWO DELIBERATE DIVERGENCES from the console, both forced by the committed
    // BrnPropZoneManager.h declaration (another lane's file -- reported, not edited):
    //  1. The console builds a LOCAL ResourcePtr<PropPhysicsDataHeader> from
    //     mpPropPhysicsDataHeader's handle (`CreateFromHandle(&local, this+0xCDD9C)`)
    //     and passes &local as r5, i.e. the manager's parameter is the smart pointer,
    //     not the raw pointer. The committed signature takes
    //     `const PropPhysicsDataHeader*`, so the resolved pointer is passed instead.
    //     Same pointee; what is lost is the temporary's ref-count hold for the duration
    //     of the call (nothing in the unload path releases the header, so the hold is
    //     not load-bearing here).
    //  2. The console passes SEVEN arguments -- r8 is mbInReplay, between lpOutput (r7)
    //     and the serialiser (r9). The committed signature has no slot for it, so that
    //     argument is DROPPED. PropZoneManager::UnloadZone's own reconstruction must
    //     grow the parameter before this is exact.
    // ========================================================================
    void PropEntityModule::UnloadZone( u16 lu16ZoneId,
                                       PropEntityIO::OutputBuffer_PreScene* lpOutput )
    {
        CGS_ASSERT( mZoneManager.IsZoneLoaded( lu16ZoneId ),
                    "mZoneManager.IsZoneLoaded( luZone )" );   // cpp:2213

        --muNumberOfLoadedZones;

        if ( CgsDev::Message::gxMessageFilterFlags & 1 )
        {
            *CgsDev::Log::gpDebugPrint
                << "PROPS Unloading zone: " << static_cast<u32>( lu16ZoneId ) << "\n";
        }

        mZoneManager.UnloadZone( lu16ZoneId,
                                 mpPropPhysicsDataHeader.GetMemoryResource(),
                                 &maRecentlyBrokenProps,
                                 lpOutput,
                                 &mPropEntitySerialiser );

        mLoadedZones.Erase( lu16ZoneId );

        if ( !mPropEntitySerialiser.IsPlaying() )
        {
            mPropEntitySerialiser.RemoveLoadedZone( lu16ZoneId );
        }
    }

    // ========================================================================
    // PropEntityModule::UpdateInstanceStreaming   @ 0x82308330   (1007 insns)
    // ------------------------------------------------------------------------
    // THE state machine. See the file banner for the [A]..[F] walkthrough; the step
    // markers below match it.
    //
    // Offset decode (console, provenance only):
    //   +0xD3200 meStreamingMode         +0xD3204 mbStreamingSettled
    //   +0xD320C muNumberOfLoadedZones   +0xD337C mLoadedZones (count word +0xD339C)
    //   +0xD3180 mPropEntitySerialiser   +0xCDE64 maRecentlyBrokenProps' count word
    //   +0xD32F0 mabWaitingForInstances  (8*(idx>>6 + 0x1A65E), i.e. base 0xD32F0)
    //   +0xD33A0 mabLoadedWorldGraphics  (8*(idx>>6 + 0x1A674))
    //   +0xD3368 miUnloadingPM           +0xCD970 mReceiverQueue
    //   +0x280   mZoneManager   (its mauStartIndexOfZone[] at 2*(zone + 0x662A1))
    // ⚠️ The two bit arrays are 64 bytes apart and 0xD32B0 (mabWaitingForGraphics) is
    // NOT touched by this function -- an easy mis-read, because Hex-Rays prints both
    // bases as `8 * (idx + <constant>)`.
    //
    // ONE OMITTED DEAD STORE: the asm zeroes stack var_E0 at 0x82308360 and again at
    // 0x8230842C under `if ( mbResetPropPosition )`. var_E0 is never read anywhere in
    // the function (verified by grepping the whole listing), so it is a local the
    // optimiser could not eliminate; it is not reproduced, and no behaviour depends on
    // it. The mbResetPropPosition READ is therefore the only surviving trace.
    // ========================================================================
    void PropEntityModule::UpdateInstanceStreaming(
            const PropInstancesNeededForZoneQueue* lpNeeded,
            s32 liZoneIndex,
            PropEntityIO::OutputBuffer_PreScene* lpOutput )
    {
        // ---- [A] the hard reset ------------------------------------------------
        if ( meStreamingMode == E_RESET_UNLOADING )
        {
            if ( CgsDev::Message::gxMessageFilterFlags & 1 )
            {
                *CgsDev::Log::gpDebugPrint << "RESETTING ALL PROPS\n";
            }

            if ( !mPropEntitySerialiser.IsPlaying() )
            {
                mPropEntitySerialiser.RemoveAllLoadedZones();
            }

            mZoneManager.RemoveAllPropsAndParts( lpOutput );

            meStreamingMode       = E_STREAM;
            mLoadedZones.Clear();
            muNumberOfLoadedZones = 0;
            maRecentlyBrokenProps.Clear();
        }

        // ---- [B] what SHOULD be resident this frame ----------------------------
        PropZonesSet lTargetZones;
        GenerateTargetList( lpNeeded, lTargetZones );

        // See the banner's OMITTED DEAD STORE note -- this read is all that is left of
        // the console's dead `if ( mbResetPropPosition ) <local> = 0;`.
        (void)mbResetPropPosition;

        // ---- [C] tripwire: the loaded set and the zone manager must agree -------
        for ( u32 luLoaded = 0; luLoaded < mLoadedZones.GetLength(); ++luLoaded )
        {
            CGS_ASSERT( mZoneManager.IsZoneLoaded( mLoadedZones[luLoaded] ),
                        "mZoneManager.IsZoneLoaded( mLoadedZones[ i ] )" );   // cpp:807
        }

        // ---- [D] unload leg: at most ONE zone, then stop for the frame ----------
        PropZonesSet lZonesToUnload;
        lZonesToUnload.Clear();
        lZonesToUnload.SetDifference( mLoadedZones, lTargetZones );

        if ( lZonesToUnload.GetLength() != 0 )
        {
            const u16 lu16UnloadZoneId = lZonesToUnload[0];

            CGS_ASSERT( mZoneManager.IsZoneLoaded( lu16UnloadZoneId ),
                        "mZoneManager.IsZoneLoaded( liUnloadZoneId )" );              // cpp:822
            CGS_ASSERT( !mabWaitingForInstances.IsBitSet( lu16UnloadZoneId ),
                        "!mabWaitingForInstances.IsBitSet( liUnloadZoneId )" );        // cpp:823

            CgsDev::PerfMonCpu::StartMonitor( miUnloadingPM );
            UnloadZone( lu16UnloadZoneId, lpOutput );
            CgsDev::PerfMonCpu::StopMonitor( miUnloadingPM );
            return;
        }

        // ---- [E] what is still missing -----------------------------------------
        PropZonesSet lZonesToLoad;
        lZonesToLoad.Clear();
        lZonesToLoad.SetDifference( lTargetZones, mLoadedZones );

        // The ONLY writer of mbStreamingSettled: nothing left to unload (we returned
        // above if there were) and nothing left to load.
        mbStreamingSettled = ( lZonesToLoad.GetLength() == 0 );

        // ---- [F] load leg: at most ONE request per frame ------------------------
        if ( liZoneIndex != KI_NO_PRIORITY_ZONE
             && !mZoneManager.IsZoneLoaded( static_cast<u16>( liZoneIndex ) ) )
        {
            // The caller named a zone that has to come in NOW (PreSceneUpdate passes the
            // player's own zone number). It jumps ahead of the to-load scan, and the scan
            // does not run at all this frame.
            RequestPropInstancesForZone( *this, static_cast<u32>( liZoneIndex ), lpOutput );
            return;
        }

        for ( u32 luToLoad = 0; luToLoad < lZonesToLoad.GetLength(); ++luToLoad )
        {
            const u16 lu16LoadZoneId = lZonesToLoad[luToLoad];

            CGS_ASSERT( !mZoneManager.IsZoneLoaded( lu16LoadZoneId ),
                        "!mZoneManager.IsZoneLoaded( liLoadZoneId )" );   // cpp:871

            if ( RequestPropInstancesForZone( *this, lu16LoadZoneId, lpOutput ) )
            {
                break;
            }
        }
    }

    // ========================================================================
    // PropEntityModule::ResetProps   (BrnPropEntityModule.h:386 -- a header inline the
    // X360 never emitted out of line)
    // ------------------------------------------------------------------------
    // Ask the state machine to drop and re-stream every prop zone. Attested by its TWO
    // inlined expansions, which agree exactly:
    //   * PropEntityDebugComponent::ResetProps @0x822A9758 -- the debug menu's "Reset
    //     props" action: `lwz r11,0x34(r3) ; li r9,2 ; stwx r9,r11,0xD3200`.
    //   * PreSceneUpdate @0x82309A40 -- the game-action latch:
    //     `if ( lpInput->mbResetProps ) <module>+0xD3200 = 2`.
    // Both store 2 == E_RESET_UNLOADING and nothing else, so the body is exact even
    // though no out-of-line symbol exists.
    // ========================================================================
    void PropEntityModule::ResetProps()
    {
        meStreamingMode = E_RESET_UNLOADING;
    }
}
