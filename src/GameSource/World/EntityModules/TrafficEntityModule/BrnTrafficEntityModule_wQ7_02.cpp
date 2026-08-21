// ============================================================================
// BrnTrafficEntityModule_wQ7_02.cpp  --  wave Q7 / cluster `traffic`, partfile 2 of 2
//
// THE BRING-UP THAT MAKES THE wQ7_01 CONSUMER REACHABLE.
//
// Landing BrnTrafficEntityModule_wQ7_01.cpp alone gives a code-complete but RUNTIME-DEAD
// chain, for two independent reasons that both live in this file:
//
//   B3  mpData @+0x71840 (the ResourcePtr<TrafficData> whose mTrafficLights collection
//       HandlePropModuleRequests hands to the light manager) is NULL, because nothing on
//       this build has ever run TrafficEntityModule::LoadData. That is not merely a gap: on
//       the first frame the leg runs it is an ACCESS VIOLATION -- ResourcePtr::operator->
//       asserts "Can not instance resource pointer - it is NULL" and then returns the null
//       anyway, so TrafficLightGotSmashed receives (TrafficLightCollection*)+0x68, whose own
//       non-null assert PASSES, and GetInstanceIndexForInstanceID dereferences it.
//   B2  meState @+0x300 is E_STATE_STARTING_UP (0) forever: the module storage is
//       zero-initialised, Construct/Prepare are inert gates in WorldLinkStubs.cpp, and the
//       only console writer of E_STATE_RUNNING is EnterRunningState @0x827080E8, reachable
//       only through PostPhysicsUpdate @0x8274E6D0 (581 insns) after Reset @0x8272CDA0
//       (824 insns) -- neither reconstructed.
//       ⭐ B2 IS CLOSED as of 2026-08-21 (wave T1, cluster C4): Construct, Reset,
//       EnterStartingUpState, PostPhysicsUpdate's STARTING_UP arm and EnterRunningState all
//       have bodies in BrnTrafficEntityModule_wT1_01.cpp, and the meState latch that used to
//       sit at the tail of Prepare below IS DELETED. The module now walks its own state
//       machine. What replaces the latch as the open item is PreSceneUpdate @0x8274A968,
//       which owns the WAITING_FOR_PLAYER -> POPULATING transition -- see the note at the
//       old latch site.
//
// THIS FILE CONTAINS (contents list re-stated 2026-08-21, wave T1 round 2, cluster R2C --
// the round retired most of the gates the old list described; the per-body banners below are
// the detail, this is the summary they must agree with):
//   * TrafficEntityModule::Prepare  @0x8274A578 (252 insns) -- PARTIAL, one gate left.
//     Stages 0, 1, 2, 3 and 5 are REAL, together with the console's fall-through chaining and
//     the default assert. Stage 1 is `CgsModule::ModuleSingleBuffered::Prepare()` (flipped this
//     round). Stage 3 is the per-VEHICLE-TYPE BoxVolume::Initialize + AddDynamicVolume
//     registration loop (landed this round). Only stage 4 -- the debug-UI leg (DebugComponent::
//     Register + the 2560-byte DEBUG_VehicleFuzzyLogic allocation) -- is still a NAMED one-shot
//     gate.
//   * TrafficEntityModule::LoadData @0x82746A88 (465 insns) -- the whole resource ladder is
//     REAL. Stages 0/1 (LoadTrafficLanes, bind the reply into mpData + mStreamer.SetAssetList)
//     and the full 2..11 chain (GetVehicleList -> N x LoadVehicle ATTRIBS -> N x LoadVehicle
//     PHYSICS -> bind maTrafficVehiclePhysicsSpecs -> DONE) are landed; the console's own
//     terminal state (E_RESOURCE_ACQUIRE_COUNT) is entered at the end of the chain.
//     ⭐ 2026-08-21 (wave T1 ROUND 3): LoadData is now GATE-FREE. The last gate in it -- the
//     stage-7 pair FindVehicleTypeAttribKey_EXPENSIVE + VehicleTypeRuntime::Prepare -- is
//     retired: the finder is bodied in THIS FILE (above LoadData) and Prepare is bodied in
//     BrnTrafficVehicleTypeRuntime_Prepare.cpp. That leaves EXACTLY ONE LogMissingLeg CALL
//     SITE in this whole file (Prepare stage 4's debug-UI leg) and one grep proves it.
//
// ⚠️⚠️ CONDUCTOR: BOOT-TEST THIS FILE. (Its ORIGINAL mount decision is already taken -- the
//   file has been in tools/build/build_game_exe.bat beside wQ7_01 since wave Q7, and the
//   WorldLinkStubs.cpp Prepare gate was retired in the same change, WorldLinkStubs.cpp:741-743.
//   What is NEW and unproven at runtime is what round 2 turned on inside the already-mounted
//   file: Prepare stage 1, Prepare stage 3, and the whole LoadData 2..11 ladder.)
//   The world Prepare ladder no longer walks straight past this module: Prepare is a real
//   multi-frame resumable ladder that returns FALSE until the LoadTrafficLanes reply lands, and
//   now also until the GetVehicleList / N x ATTRIBS / N x PHYSICS round trips land. The
//   request/reply round trip IS proven live on this build in the same Prepare context --
//   BrnTriggerQueryManager_Prepare.cpp:201-249 and BrnDirectorWorldMap.cpp:200-226 both do
//   LoadTrafficLanes(queue, 1, 5) and both bind their handle (BrnGame.log "[TriggerQueryManager]
//   LOADED -- trigger=1 traffic=1", "[WorldMap] LOADED -- traffic=1"), and WorldModule::Prepare's
//   own not-ready path already drains this module's request pipe through the REAL
//   BridgeTrafficResourceRequestsToOutput @0x827AD9D8 (BrnWorldModule.cpp:992). But if a reply
//   never arrives on THIS route the world Prepare ladder stalls at the traffic stage and the
//   game never finishes loading. Neither cluster owner was the run-time owner; neither could
//   drive it. The `[T1-prepare]` / `[T1-stream]` probes (BRN_TRAFFIC_DIAG=1) name the stalled
//   stage in one line. The revert is per-leg, not per-file: restore the LogMissingLeg call in
//   the stage that stalled. wQ7_01 is unaffected either way.
//
// SOURCES: X360 ARTIST asm+pseudocode (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8274A578.json,
// 0x82746A88.json, 0x827080E8.json, 0x82740220.json) for behaviour; DecFIGS DWARF for shape.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"

#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"          // BrnTraffic::TrafficData
#include "SharedClasses/Traffic/BrnTrafficVehicleAsset.h"              // BrnTraffic::VehicleAsset
#include "SharedClasses/Traffic/BrnTrafficVehicleType.h"               // BrnTraffic::VehicleTypeData
#include "SharedClasses/DataLists/VehicleList.h"                       // BrnResource::VehicleList
#include "SharedClasses/DataLists/VehicleListEntry.h"                  // BrnResource::VehicleListEntry
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h" // the spec Prepare reads
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"     // CgsResource::ResourcePtr<T>
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"   // EventReceiverQueue<4096,16>
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"            // GetGameDataEvent
#include "GameSource/Resource/SharedIO/BrnAssetIds.h"                  // BrnResource::EAssetSet
#include "GameShared/GameClasses/Core/CgsID.h"                         // CgsIDUnCompress, KI_CGSID_STRING_LEN
#include "GameShared/GameClasses/SceneManager/CgsVolumeId.h"           // CgsSceneManager::VolumeId
#include "vendor/renderware/collision/CollisionVolume.hpp"             // rw::collision::BoxVolume
#include "rw/rwcore_structs.h"                                         // rw::Resource
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"             // gpDebugPrint / gxMessageFilterFlags

#include <cstddef>   // size_t
#include <cstdlib>   // getenv (the BRN_TRAFFIC_DIAG probes)
#include <cstring>   // strstr (the CgsIDUnCompress space truncation)

namespace BrnTraffic
{
namespace
{
    // 2026-08-21 (wave T1, cluster C1): the FieldAt<T>(this, X360_BYTE_OFFSET) helper that used
    // to live here is GONE, along with the KU_OFFSET_* table below it. The module class now
    // carries its real member list (BrnTrafficEntityModule.h), so every access is a named
    // member. The console offsets survive ONLY as attestation comments at each use site -- they
    // are 32-bit-pointer displacements and were never valid on this LP64 host.
    //
    // TrafficReceiverQueue is now a nested typedef on the class (the DWARF :613 instantiation),
    // so the local typedef here is retired.

    // Is this queue's backing buffer bound yet? `mpBuffer` is PROTECTED on
    // CgsModule::BaseEventReceiverQueue (CgsBaseEventReceiverQueue.h) and that committed
    // shared type exposes no accessor for it -- so it is read through a derived-class
    // pointer-to-member, the one form of protected access the language grants without
    // growing a type this cluster does not own. Zero-initialised module storage has
    // mpBuffer == 0; BaseEventReceiverQueue::Construct binds it to the embedded maBuffer,
    // and Clear() never touches it. This is therefore a PER-OBJECT "already Constructed?"
    // test, not a process-wide one.
    struct ReceiverQueueBinding : public CgsModule::BaseEventReceiverQueue
    {
        static bool IsBound( const CgsModule::BaseEventReceiverQueue& lrQueue )
        {
            u8* CgsModule::BaseEventReceiverQueue::* const lpBufferMember =
                &ReceiverQueueBinding::mpBuffer;
            return (lrQueue.*lpBufferMember) != 0;
        }
    };

    // ---- console offsets, now resolved to NAMED members (attestation only) -------------
    //   0x2F0  mePrepareStage (:602)    0x2F4  meReleaseStage (:603)   0x2F8 meResourceStage (:604)
    //   0x300  meState (:607)           0x304  meStartingUpState (:608)
    //   0x308  meRunningState (:609)    0x30C  meRunningStateToUseAfterStartup (:610)
    //   0x314  mReceiverQueue (:613)    0x71840 mpData (:752)
    //   0x713A0 maTrafficPhysicsInfoListBits (:686)   0x71B32 mbInReplay (:757)
    // (the last two are the stage-0 zero stores -- see the attestation in Prepare stage 0).

    // The resource-request ids the console bakes into LoadData case 0 / case 1
    // (`LoadTrafficLanes(&mReceiverQueue, 1, 5)`, `cmpwi r10,0x37`, `cmpwi ...,1`). Same
    // three values, same names, as BrnDirectorWorldMap.cpp:51-52 and
    // BrnTriggerQueryManager_Prepare.cpp:71 -- which is the corroboration that 0x37 is the
    // traffic-lanes response and not some other data reply.
    const s32 KI_DATA_ACQUIRE_REQUEST       = 1;
    const s32 KI_LANE_DATA_POOL_ID          = 5;
    const s32 KI_RESPONSE_GET_TRAFFIC_LANES = 55;   // 0x37

    // ---- ADDED 2026-08-21 (wave T1 round 2, cluster R2C) -------------------------------
    // The vehicle-LIST request's own event id. `GetVehicleList(iface, &mReceiverQueue, 0)`
    // at 0x82746xxx (LABEL_10) passes literal 0, and the reply stage asserts the payload's
    // miEventId is 0 -- `if ( *v33 ) { ... "Invalid event id received" ... }` (.cpp:1108).
    const s32 KI_VEHICLE_LIST_REQUEST = 0;

    // The receiver-record KIND word the two ASSET reply stages guard against. Both drains
    // compare the leading word of every drained record against the literal 50 and fire
    // "Invalid event id received" otherwise -- BrnTrafficEntityModule.cpp:1214 (the ATTRIBS
    // drain, LABEL_27) and :1291 (the PHYSICS drain, LABEL_41). Same word position as
    // BrnResource::GameDataIO::EVENT_GET_TRAFFIC_LANES (55) and EVENT_GET_SURFACE_LIST (66);
    // 50 is the LoadGameData "asset is resident" reply. It is spelled here rather than in
    // BrnGameDataEvents.h because that header is not this cluster's file -- PROMOTE IT there
    // (beside its two siblings, which already carry this exact cross-reference in their
    // banner) when a cluster that owns it lands.
    const s32 KI_RESPONSE_LOAD_GAMEDATA_ASSET = 50;

    // The pool every per-vehicle-type asset request is posted to. Both inlined LoadVehicle
    // expansions store the literal 1 into the record's miPoolId
    // (`v149 = 1`, record +0x08) == BrnResource::E_POOL_PHYSICS.
    const s32 KI_VEHICLE_ASSET_POOL_ID = 1;

    // ---- Prepare stage 3 (the per-vehicle-type scene volume) ---------------------------
    // The console's own two shrink constants, both plain rodata read into VMX splats at
    // 0x8274A66C..0x8274A688:
    //   `*v55 = 0.49000001` -- an immediate literal in the frame  -> KF_VEHICLE_BBOX_FATNESS_SCALE
    //   `r23 = &unk_820BA4C4`                                     -> KF_VEHICLE_BBOX_FATNESS_MAX
    // unk_820BA4C4 == 0.44f, DUMPED (idat, this wave's rodata_dump.txt: the four floats at
    // 0x820BA4BC are 50000 / 1.0 / 0.44 / 20.0). NOT a placeholder zero -- recurring bug
    // class (c) checked and closed.
    //
    // SHIP-vs-LEAK DIVERGENCE, and the ASM WINS: Feb-2007 subtracts a FIXED
    // `KF_VEHICLE_BBOX_FATNESS` (BrnTrafficEntityModule.cpp:395-411). The ship computes a
    // CLAMPED one per vehicle type,
    //     fatness = min( 0.44f, 0.49f * min( halfX, halfY, halfZ ) )
    // (vminfp x,y -> vminfp with z -> vmulfp by 0.49 -> vminfp against 0.44), which is what
    // keeps the three `lBBoxHalfSize.<axis>() >= 0.0f` asserts satisfiable for a small
    // vehicle type. Reproduced exactly.
    const f32 KF_VEHICLE_BBOX_FATNESS_SCALE = 0.49000001f;
    const f32 KF_VEHICLE_BBOX_FATNESS_MAX   = 0.44f;

    // `addi r11, r28, 0x24` @0x8274A834 -- the scene VOLUME key for vehicle type N is
    // N + 36, zero-extended to the 64-bit VolumeId (`clrldi r11, r11, 32`), i.e. owner byte
    // 0. Feb-2007 spells the base `KU_HACK_BASE_VOLUME_ID` (:395) and the name is kept.
    const u32 KU_HACK_BASE_VOLUME_ID = 36u;

    // `li r29, 8` @0x8274A684 -- the VolumeTypeFlags AddDynamicVolume tags each traffic
    // vehicle-type box with. Feb-2007 names it BrnWorld::E_ENTITYTYPEFLAG_TRAFFIC_VEHICLE
    // and BrnEntityTypes.h:64 gives it the value 0x00000008 -- leak and ship agree exactly.
    // That enum has no home in this tree (the race-car and prop volume producers name their
    // own immediates the same way -- BrnActiveRaceCar_wQ5_01.cpp's
    // KU8_RACECAR_VOLUME_TYPE_FLAG == 1), so the immediate is named, not decomposed.
    const u8 KU8_TRAFFIC_VEHICLE_VOLUME_TYPE_FLAG = 8u;

    // The console's stack scratch for the box image. Feb-2007 declares
    // `rw::math::Vector3 laResourceBuffer[256]` (4,096 bytes) and asserts
    // `sizeof(laResourceBuffer) >= lResDesc.GetSize()`; the ship's frame is 0x1150 bytes,
    // which is that buffer plus the VMX temporaries. AddDynamicVolume block-copies 128 bytes
    // FROM the volume pointer (`li r5, 0x80` @0x822B1534 -- the same over-read
    // BrnActiveRaceCar_wQ5_01.cpp documents), so the block must stay >= 128 addressable
    // bytes; 4,096 is the console's own size and is kept.
    const size_t KU_VOLUME_SCRATCH_BYTES = 4096;

    // ---- [T1-*] bring-up diagnostics knob (NOT IN THE X360 BINARY) ---------------------
    // Same knob C4/C5 introduced this wave (BRN_TRAFFIC_DIAG); the wQ7 files used to borrow
    // BRN_PROP_DIAG. All probes below are one-shot and marked DELETE-WHEN-STABLE.
    bool IsTrafficDiagOn()
    {
        static const bool sbOn = ( getenv( "BRN_TRAFFIC_DIAG" ) != 0 );
        return sbOn;
    }

    // PARTIAL-pattern leg gate -- one NAMED one-shot line per console leg with no body in the
    // tree. Same gating condition as the WorldLinkStubs.cpp boot gates.
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
                << "[Q7-traffic-leg] TrafficEntityModule stage NOT RECONSTRUCTED, skipped: "
                << lpcLegNameAndReason << " [FLAG PC partial gate]\n";
        }
    }

    // ---- ADDED 2026-08-21 (wave T1 round 2, cluster R2C) -------------------------------
    // INLINING REVERSAL. LoadData's two per-vehicle-type request passes (LABEL_20, the
    // ATTRIBS pass; LABEL_33, the PHYSICS pass) are instruction-for-instruction identical
    // apart from the asset set they ask for, so the console's duplicated block is re-rolled
    // into this one helper. It is the "give me the bare asset name for vehicle type N" step:
    //
    //   0x8274xxxx  v52 = *(*(pData + 44) + v51 + 5)        mpaVehicleTypes[type].muAssetId
    //                                                       (v51 steps by 8 == the attested
    //                                                        VehicleTypeData stride)
    //               CgsIDUnCompress( *(*(pData + 52) + __ROL4__(v52,3) + 4), buf )
    //                                                       mpaVehicleAssets[assetId]
    //                                                       .mVehicleId -- the ROL4 by 3 is
    //                                                       the *8 element stride, and the
    //                                                       "+4" is Hex-Rays splitting the
    //                                                       8-byte CgsID load on a
    //                                                       big-endian host, not a member
    //                                                       at +4 (recurring bug class (b)).
    //               v61 = strstr( buf, " " ); if (v61) *v61 = 0;
    //
    // Identical to the step TrafficCarStreamer::SetAssetList @0x82753A38 performs over the
    // same asset array (BrnTrafficCarStreamer.cpp), which is the corroboration that the
    // 8-byte read is the whole id and the truncation is at the first space.
    void UnCompressVehicleAssetName( const TrafficData* lpData, u32 luVehicleType,
                                     char* lpacName )
    {
        const u32 luAssetId = lpData->mpaVehicleTypes[luVehicleType].muAssetId;

        CgsIDUnCompress( lpData->mpaVehicleAssets[luAssetId].GetVehicleId(), lpacName );

        // CgsIDUnCompress right-pads with spaces; the asset name is the head.
        char* lpcFirstSpace = strstr( lpacName, " " );
        if ( lpcFirstSpace != 0 )
        {
            *lpcFirstSpace = '\0';
        }
    }

    // ---- ADDED 2026-08-21 (wave T1 round 2, cluster R2C) -------------------------------
    // The console emits LoadData's per-vehicle-type REQUEST pass twice -- LABEL_20 (ATTRIBS)
    // and LABEL_33 (PHYSICS) -- and the two blocks are identical apart from the asset set,
    // so they are re-rolled into this one helper (AGENTS.md "inlining reversal"). It is a
    // free function rather than a member because a member would need a declaration in
    // BrnTrafficEntityModule.h, which is not this cluster's file; every value it needs is
    // passed in.
    //
    //   a1[124128] = 0                                 miResourceRequestCount = 0
    //   LockForWrite(a2)
    //   if ( mpData->muNumVehicleTypes ) do {
    //       <UnCompressVehicleAssetName>
    //       v62 = (*v35)++                             the request's event id
    //       <inlined LoadVehicle: mpReceiverQueue = &mReceiverQueue, miPoolId = 1,
    //        mId = MakeVehicleId(name), meType = <the pass>, mbFailFlag = 0, AddEvent 26>
    //   } while ( ++i < mpData->muNumVehicleTypes )
    //   mReceiverQueue.Clear();  UnlockForWrite(a2)
    //
    // The console re-reads mpData->muNumVehicleTypes through ResourcePtr::operator-> on EVERY
    // iteration of the bound test (four separate `operator->` calls appear in the loop); that
    // is a compiler artefact of the resource-pointer accessor, not a semantic, so the bound
    // is read once here.
    void RequestVehicleAssetsForEveryType( BrnTrafficIO::OutputBuffer_Prepare*  lpOutputBuffer,
                                           CgsModule::BaseEventReceiverQueue*   lpReceiverQueue,
                                           const TrafficData*                   lpData,
                                           s32*                                 lpiRequestCount,
                                           BrnResource::EAssetSet               leAssetSet )
    {
        *lpiRequestCount = 0;

        lpOutputBuffer->LockForWrite();

        for ( u32 luVehicleType = 0;
              luVehicleType < lpData->muNumVehicleTypes;
              luVehicleType++ )
        {
            char lacAssetName[KI_CGSID_STRING_LEN];
            UnCompressVehicleAssetName( lpData, luVehicleType, lacAssetName );

            const s32 liEventId = ( *lpiRequestCount )++;

            lpOutputBuffer->GetResourceRequestInterface()->LoadVehicle(
                lpReceiverQueue, liEventId, KI_VEHICLE_ASSET_POOL_ID,
                lacAssetName, leAssetSet );

            // ---- [T1-stream] bring-up probe (NOT IN THE X360 BINARY) --------------------
            // One-shot on the FIRST per-vehicle-type asset request the module ever posts.
            // If this never prints, LoadData never reached its request stages and no
            // VEH_T* asset is asked for at any point in the boot. DELETE-WHEN-STABLE.
            {
                static bool sbLogged = false;
                if ( IsTrafficDiagOn() && !sbLogged && CgsDev::Log::gpDebugPrint != 0 )
                {
                    sbLogged = true;
                    *CgsDev::Log::gpDebugPrint
                        << "[T1-stream] LoadData posted its FIRST traffic vehicle asset "
                           "request: name='" << lacAssetName
                        << "' assetSet=" << static_cast<s32>( leAssetSet )
                        << " pool=" << KI_VEHICLE_ASSET_POOL_ID
                        << " of " << static_cast<s32>( lpData->muNumVehicleTypes )
                        << " vehicle types [DELETE-WHEN-STABLE]\n";
                }
            }
        }

        lpReceiverQueue->Clear();
        lpOutputBuffer->UnlockForWrite();
    }

    // The ATTRIBS drain (LABEL_27). Walks every queued record asserting its KIND word is 50
    // and BINDS NOTHING -- see the LoadData banner. The console's `if ( v78 != -8 )` guard is
    // Hex-Rays rendering "the queue has no backing buffer" (mpBuffer + miStartOffset), which
    // on the host is exactly "GetFirstEvent handed back a null record".
    void DrainAssetReplies( CgsModule::BaseEventReceiverQueue* lpReceiverQueue )
    {
        if ( lpReceiverQueue->GetCount() <= 0 )
        {
            return;
        }

        const CgsModule::Event* lpEvent = 0;
        s32                     liSize  = 0;
        s32 liKind = lpReceiverQueue->GetFirstEvent( &lpEvent, &liSize );

        while ( lpEvent != 0 )
        {
            CGS_ASSERT( liKind == KI_RESPONSE_LOAD_GAMEDATA_ASSET,
                        "Invalid event id received" );                   // baked line 1214

            const CgsModule::Event* lpNext = 0;
            liKind  = lpReceiverQueue->GetNextEvent( lpEvent, &lpNext, &liSize );
            lpEvent = lpNext;
        }
    }

    // PROVENANCE ONLY -- there is no host-layout pin here any more (retired 2026-08-21, wave T1
    // round 2, cluster R2C).
    //
    // What stood here was a "HOST-LAYOUT DIVERGENCE, PINNED (gotcha 1)" block arguing in console
    // bytes: that mReceiverQueue "is placed at the console's +0x314 inside the opaque blob", that
    // the host object therefore "runs 0x314..0x1334 where the console's runs 0x314..0x131C", and
    // that a future grow of the queue could "silently walk into mTrafficLightManager". That
    // reasoning describes a layout this tree no longer has. Since wave T1 cluster C1 deleted
    // `u8 mOpaque[0x73000]`, mReceiverQueue (BrnTrafficEntityModule.h:751) and
    // mTrafficLightManager (:816) are ordinary named members of a host-native class: the
    // compiler places them, nothing sits at a console displacement, and one member cannot
    // overrun another. The console offsets quoted throughout this file are 32-bit-pointer
    // ATTESTATION -- they say which X360 field a line corresponds to, and nothing about the host.
    //
    // What replaced the pin: the queue's position relative to the rest of the state block is
    // pinned by ORDER in TrafficEntityModule::_AssertLayout (BrnTrafficEntityModule.cpp -- the
    // ten-member run ending `meTearingDownState before mReceiverQueue`), and the queue-before-
    // light-manager fact is simply declaration order in the header, which C++ guarantees for
    // same-access members. See also the note at the head of TrafficEntityModule::Prepare, where
    // the X360-byte `static_assert(KU_OFFSET_RECEIVER_QUEUE + sizeof(queue) < 0x53790)` was
    // retired for the same reason.
}

// ============================================================================
// BrnTraffic::TrafficEntityModule::FindVehicleTypeAttribKey_EXPENSIVE @ 0x8273F0B8
//                                                                    *** COMPLETE ***
//
// *** LANDED 2026-08-21 (wave T1 round 3, closure item 3). Round 2 parked this as one half of
// "recurring bug class (d), two instances in one leg", with the blocker "not declared at all
// -- its ledger row's primary_file is the CgsStrStream.h catch-all". That is a LEDGER
// misattribution, not a fact about the function: its own body reaches `this + 464960`
// (mpData) and `this + 496516` (mpVehicleList), and all three of its baked assert strings
// name BrnTrafficEntityModule.cpp (:17180 / :17181 / :17185). It is a TrafficEntityModule
// member and it belongs HERE, beside its one and only caller, LoadData stage 7.
//
// WHAT IT DOES. Map a traffic vehicle TYPE to the attribsys collection key of the car record
// that type is skinned from:
//     type   -> mpData->mpaVehicleTypes[type].muAssetId          (byte at element +5)
//            -> mpData->mpaVehicleAssets[assetId].GetVehicleId() (a CgsID)
//            -> mpVehicleList->GetVehicleIndex(id)               (LINEAR scan -- _EXPENSIVE)
//            -> mpVehicleList->GetVehicleData(index)             (240-byte entry)
//            -> entry->GetAttribCollectionKeyHash()              (AttribSysCollectionKey +0xA0)
//
// EVERY CONSOLE OFFSET RESOLVES TO A NAMED MEMBER (no raw offsets survive):
//     mpData + 44 == +0x2C -> TrafficData::mpaVehicleTypes    (BrnTrafficDataResourceType.h)
//     mpData + 52 == +0x34 -> TrafficData::mpaVehicleAssets   (same)
//     element + 5          -> VehicleTypeData::muAssetId      (BrnTrafficVehicleType.h:143,
//                             already documented there as "index into mpaVehicleAssets")
//     8 * assetId          -> the attested VehicleAsset stride
//                             (static_assert(sizeof(VehicleAsset) == 8))
// The pseudocode's `__ROL4__(v7, 3)` is just `assetId * 8`, and the pair of loads at
// `v8 + v9` / `v8 + v9 + 4` is the two-word CgsID read -- the same "__ROL4__ + 4 CgsID split"
// round 2 flagged as recurring bug class (b). It is ONE CgsID here, not two values.
//
// THE THREE ASSERTS, with their console line numbers:
//   :17180  "Unable to find vehicle <id>"        -- index >= 0 but GetVehicleData returned null
//   :17181  "Traffic vehicle <id> has no AttribCollectionKey"  -- ⚠️ DEAD ON THE CONSOLE, see
//           the note at the call site: its test is `addic. r22, 0xA0`, an ADDRESS, so it can
//           never fire. Not reproduced (reproducing it as a null-entry test doubled :17180).
//   :17185  "The vehicle list is empty!!"        -- the fallback arm's own precondition
// The console builds the first two through a CgsDev::StrStream so the id is printed; the
// house CGS_ASSERT takes a literal, so the id is not interpolated here. Same accommodation
// every sibling assert in this directory makes.
//
// *** THE FALLBACK ARM IS THE INTERESTING ONE, AND IT IS NOT AN ERROR PATH. When the asset id
// is NOT in the vehicle list (`GetVehicleIndex(...) < 0`), the console does not fail -- it
// asserts the list is non-empty and takes ENTRY 0's key. Traffic cars are frequently not
// selectable player cars, so this is the ordinary path for them, not a rescue. Reproduced
// exactly, including the fact that the fallback SKIPS the "no AttribCollectionKey" check
// (the console jumps straight to LABEL_12).
// ============================================================================
VehicleTypeRuntime::AttribKey
TrafficEntityModule::FindVehicleTypeAttribKey_EXPENSIVE( u32 luVehicleType ) const
{
    const VehicleTypeData* lpVehicleType = &mpData->mpaVehicleTypes[luVehicleType];
    const CgsID lVehicleId = mpData->mpaVehicleAssets[lpVehicleType->muAssetId].GetVehicleId();

    const BrnResource::VehicleListEntry* lpEntry = 0;

    const s32 liVehicleIndex = mpVehicleList->GetVehicleIndex( lVehicleId );
    if ( liVehicleIndex < 0 )
    {
        // 0x8273F148ff -- the not-in-the-list arm. Entry 0 is the key every traffic car that
        // is not also a drivable car ends up with.
        CGS_ASSERT( mpVehicleList->GetVehicleCount() > 0,
                    "The vehicle list is empty!!" );          // baked line 17185
        return mpVehicleList->GetVehicleData( 0 )->GetAttribCollectionKeyHash();
    }

    lpEntry = mpVehicleList->GetVehicleData( liVehicleIndex );
    CGS_ASSERT( lpEntry != 0, "Unable to find vehicle" );     // baked line 17180

    // ⭐ CORRECTED 2026-08-21 (round 3 FIX pass). What stood here was a SECOND
    // `CGS_ASSERT(lpEntry != 0, "Traffic vehicle has no AttribCollectionKey")` (baked line
    // 17181), justified as "the console's null-ish guard on the key member ... which is a null
    // check on the ENTRY". It is not, and re-reading the asm settles it:
    //     0x8273F1C8  addic. r11, r22, 0xA0
    //     0x8273F1CC  bne    loc_8273F2D8          -> straight to GetHashKey
    // The test is on `r22 + 0xA0` -- the ADDRESS of the entry's mAttribCollectionKey, not its
    // value -- so it is non-zero for EVERY input including r22 == 0 (0 + 0xA0 == 0xA0), and the
    // console's :17181 assert can never fire. Reproducing it as a null-entry test made this
    // build fire TWO released asserts on the null path where the console fires ONE (:17180) --
    // and this wave has already died once in an assert storm, so the duplicate is dropped
    // rather than kept. Behaviour on every non-null path is identical; the console's own
    // never-taken branch is documented here instead of being spelled as a tautology.
    //
    // (Also worth stating because the pseudocode invites the opposite reading: the console
    // calls GetVehicleIndex TWICE with the SAME register -- `mr r4, r30` at 0x8273F114 and
    // again at 0x8273F12C -- so Hex-Rays renders two calls whose arguments LOOK different.
    // They are the same call on the same id; one call is correct here, nothing was dropped.)

    return lpEntry->GetAttribCollectionKeyHash();
}

// ============================================================================
// BrnTraffic::TrafficEntityModule::LoadData  @ 0x82746A88  (465 insns)   *** PARTIAL ***
//
// DWARF :1266  bool LoadData(OutputBuffer_Prepare*)
//
// The module's resource-acquire ladder, a resumable switch on meResourceStage @+0x2F8
// (EResourceAcquireStage, header :503).
//
// ⭐⭐ THE 2..11 GATE IS RETIRED -- 2026-08-21, wave T1 round 2, cluster R2C.
//
// What stood here was ONE named gate covering "resource stages 2..11", justified by two
// claims that are BOTH STALE:
//   (a) "they reach module storage past sizeof(mOpaque) == 0x73000". The opaque blob is
//       GONE (wave T1 cluster C1). Every address the gate quoted is now a named member:
//         &a1[8*id + 120534] == this + 482136 -> maTrafficVehiclePhysicsSpecs[id]  (:939)
//         &a1[32*id + 121056] == this + 484224 -> maVehicleTypeRuntime[id]         (:940)
//         a1[124128] == this + 496512 -> miResourceRequestCount                    (:941)
//         a1[124129] == this + 496516 -> mpVehicleList                             (:943)
//   (b) "RequestInterface::LoadVehicle has no declaration". It does now -- added this round
//       to GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h with the DWARF signature
//       (`const char*` name, not a prebuilt CgsID) and its body beside its siblings.
//
// THE CONSOLE'S REAL STAGE ORDER, which is NOT the numeric order of the enum. Every arm
// falls THROUGH to the next on the same tick; the numbered `case` labels exist only so a
// later frame can re-enter mid-ladder:
//     0  BASEDATA_NOT_STARTED   LoadTrafficLanes(&mReceiverQueue, 1, 5)
//     1  BASEDATA_REQUESTED     await reply kind 55 -> mpData; mStreamer.SetAssetList(...)
//     2  VEHICLELISTAQUIRE      GetVehicleList(&mReceiverQueue, 0)
//     3  WFVEHICLELISTAQUIRE    await 1 reply -> mpVehicleList
//     8  LOAD_ATTRIBS           N x LoadVehicle(..., E_ASSETSET_ATTRIBS)   [N = types]
//     9  WFLOAD_ATTRIBS         await N replies, drain, BIND NOTHING
//     6  LOAD_PHYSICS           N x LoadVehicle(..., E_ASSETSET_PHYSICS)
//     7  WFLOAD_PHYSICS         await N replies -> maTrafficVehiclePhysicsSpecs[type]
//                                                + VehicleTypeRuntime::Prepare
//    12  ACQUIRE_COUNT          `return true`
// Stages 4/5 (LOAD_VEHICLES / WFLOAD_VEHICLES) and 10/11 (LOAD_WHEELS / WFLOAD_WHEELS) have
// NO case label in the shipped jump table at all -- they are enum slots the ship does not
// walk (the graphics + wheel bundles are the STREAMER's job, TrafficCarStreamer, not this
// ladder's). Reaching one lands in the default arm's "weird state" assert, faithfully.
//
// ⚠ STAGE 9 BINDS NOTHING, AND THAT IS THE CONSOLE. The ATTRIBS drain (LABEL_27) walks
// every reply, asserts its kind is 50, and discards it. Only the PHYSICS drain (LABEL_41)
// binds. Do not "fix" this into a symmetric pair.
//
// ⚠ ONE ARRAY-BOUND FACT WORTH KNOWING, REPRODUCED NOT FIXED: stage 7 indexes
// maTrafficVehiclePhysicsSpecs -- which the DWARF sizes [KU_MAX_VEHICLE_ASSETS == 64]
// (BrnTrafficEntityModule.h:939) -- by the vehicle TYPE index, whose own assert bound is
// mpData->muNumVehicleTypes (up to KU_MAX_VEHICLE_TYPES == 96). The console has the same
// mismatch (its array spans this+482136..484223, 2,088 bytes == 65 console ResourcePtrs),
// so a data set with more than 64 vehicle types would overrun on the X360 too. Shipped
// B5TRAFFIC.BNDL is well under. Reported, not accommodated.
//
// The traffic-LIGHT data wave Q7 needs is entirely inside the stage-1 payload: the reply
// binds the whole TrafficData resource, and TrafficData::mTrafficLights is a BY-VALUE member
// of it (BrnTrafficDataResourceType.h:75), already relocated by TrafficData::FixUp.
// ============================================================================
bool TrafficEntityModule::LoadData( BrnTrafficIO::OutputBuffer_Prepare* lpOutputBuffer )
{
    TrafficReceiverQueue& lrReceiverQueue = mReceiverQueue;             // console +0x314
    CgsResource::ResourcePtr<TrafficData>& lrData = mpData;             // console +0x71840

    switch ( meResourceStage )                                          // console +0x2F8
    {
    case E_RESOURCE_LOAD_BASEDATA_NOT_STARTED:
        // 0x82746Axx case 0: LockForWrite(lpOutput); RequestInterface<4096>::LoadTrafficLanes
        // @0x827468C0 (event id 1, pool 5); UnlockForWrite. Then straight into case 1 -- the
        // console does NOT return here.
        lpOutputBuffer->LockForWrite();
        lpOutputBuffer->GetResourceRequestInterface()->LoadTrafficLanes(
            &lrReceiverQueue, KI_DATA_ACQUIRE_REQUEST, KI_LANE_DATA_POOL_ID );
        lpOutputBuffer->UnlockForWrite();
        // fall through -- the console falls into LABEL_3 on the same tick.

    case E_RESOURCE_LOAD_BASEDATA_REQUESTED:
    {
        // LABEL_3 sets the stage FIRST (it is the fall-through landing point as well as
        // case 1's own entry), then tests the reply count.
        meResourceStage = E_RESOURCE_LOAD_BASEDATA_REQUESTED;

        if ( lrReceiverQueue.GetCount() != 1 )
        {
            if ( lrReceiverQueue.GetCount() >= 1 )
            {
                // More than one reply queued -- a non-gating tripwire, then bail.
                CGS_ASSERT( lrReceiverQueue.GetCount() < 1,
                            "mReceiverQueue.GetLength() < 1" );          // baked line 1064
            }
            return false;
        }

        // `v13 = mReceiverQueue.miStartOffset + mReceiverQueue.mpBuffer` then `*v13 == 55`
        // (the record's TYPE word) and `v14 = v13 + 8` (the payload). Expressed through the
        // queue's own accessor, exactly as BrnTriggerQueryManager_Prepare.cpp:224 and
        // BrnDirectorWorldMap.cpp:214 already do for the identical reply.
        const CgsModule::Event* lpEvent = 0;
        s32                     liSize  = 0;
        const s32 liType = lrReceiverQueue.GetFirstEvent( &lpEvent, &liSize );

        if ( liType != KI_RESPONSE_GET_TRAFFIC_LANES )
        {
            CGS_ASSERT( liType == KI_RESPONSE_GET_TRAFFIC_LANES,
                        "TrafficEntityModule::LoadData has received a resource with an ID that wasn't requested" ); // line 1051
        }

        const BrnResource::GameDataIO::GetGameDataEvent* lpAcquire =
            static_cast<const BrnResource::GameDataIO::GetGameDataEvent*>( lpEvent );

        CGS_ASSERT( lpAcquire->miEventId == KI_DATA_ACQUIRE_REQUEST,
                    "lpAcquire->GetEventId() == KI_DATA_ACQUIRE_REQUEST" );   // baked line 1055

        // ---- THE SEAT: `CgsResource::BaseResourcePtr::CreateFromHandle(a1+116240, v14+8)`
        // == mpData = lpAcquire->mHandle. The console reads the handle at payload +0x20; the
        // host handle is 16 bytes where the console's is 8, so it is read BY MEMBER.
        lrData = lpAcquire->mHandle;

        // ⭐ UN-GATED 2026-08-21 (wave T1 round 2, cluster R2C). THE GATE THAT STOOD HERE WAS
        // STALE, AND IT WAS THE ONE THE BOOT LOG NAMED AS THE REASON NO VEH_T*_GR BUNDLE IS
        // EVER REQUESTED. Its claim -- "BrnTrafficCarStreamer.h declares no SetAssetList" --
        // stopped being true when wave T1 cluster C5 rebuilt the streamer as the real
        // BrnWorld::BaseStreamer<64> leaf and landed SetAssetList @0x82753A38
        // (BrnTrafficCarStreamer.cpp / .h). This is the console leg, unchanged:
        //
        //   `SetAssetList(&mStreamer @+0x72B58, mpData->muNumVehicleAssets,
        //    mpData->mpaVehicleAssets)` -- ARGUMENT ORDER MEASURED at 0x82746C1C..0x82746C2C:
        //   `addis r3,r23,7 ; addi r3,r3,0x2B58` (r3 = &mStreamer, this + 469848), then
        //   `lbz r4,0x18(TrafficData)` (r4 = the u8 COUNT) and `mr r5,r31` where r31 came
        //   from `lwz r31,0x34(TrafficData)` (r5 = the asset-array POINTER). So the count is
        //   the FIRST value argument and the pointer the second.
        //
        // This is the CATALOGUE publish only. It does not by itself request a bundle: the
        // request comes from TrafficCarStreamer::Update, pumped by
        // TrafficEntityModule::UpdateStreaming @0x82748848 -- which is still unbodied. See
        // this file's LoadData/Prepare report and the R2C park list.
        mStreamer.SetAssetList( mpData->muNumVehicleAssets, mpData->mpaVehicleAssets );

        lrReceiverQueue.Clear();
    }
    // fall through -- the console does NOT return here; LABEL_3 runs straight into LABEL_10.

    case E_RESOURCE_LOAD_VEHICLELISTAQUIRE:
        // ---- LABEL_10 (case 2) @0x82746xxx ------------------------------------------------
        //   a1[190] = 2;  LockForWrite(a2);
        //   GetVehicleList( OutputBuffer_Prepare_GetResourceRequestInterface(a2),
        //                   &mReceiverQueue, 0 );
        //   mReceiverQueue.Clear();  UnlockForWrite(a2);
        // Note the Clear is INSIDE the lock bracket and AFTER the post -- the console drops
        // whatever the lane reply left behind before it starts waiting for the list.
        meResourceStage = E_RESOURCE_LOAD_VEHICLELISTAQUIRE;

        lpOutputBuffer->LockForWrite();
        lpOutputBuffer->GetResourceRequestInterface()->GetVehicleList(
            &lrReceiverQueue, KI_VEHICLE_LIST_REQUEST );
        lrReceiverQueue.Clear();
        lpOutputBuffer->UnlockForWrite();
        // fall through -- LABEL_11.

    case E_RESOURCE_LOAD_WFVEHICLELISTAQUIRE:
    {
        // ---- LABEL_11 (case 3) ------------------------------------------------------------
        //   v32 = a1[199] (the reply count);  a1[190] = 3;
        //   if ( v32 < 1 ) goto LABEL_50 (return false);
        //   v33 = (a1[200] + a1[197] + 8)          ; the payload
        //   if ( *v33 ) assert "Invalid event id received"          (.cpp:1108)
        //   a1[124129] = v33[8]                     ; mpVehicleList = payload + 0x20
        //
        // `v33[8]` is the payload's +0x20 word -- the SAME slot stage 1 hands to
        // CreateFromHandle -- i.e. GameDataAssetEvent::mHandle. For a LIST resource the
        // console keeps the raw resource memory rather than a ResourcePtr, exactly as
        // GameStateModule::ReceiveListResource does for its own vehicle/wheel lists
        // (BrnGameStateModule.cpp: `*lppOutResource = lpReply->mHandle.mpResourceMemory`,
        // with the same "the host ResourceHandle is 16 bytes where the console's is 8, so
        // every literal offset past it shifts" note). Read BY MEMBER here for that reason.
        //
        // The console's assert message is composed through a StrStream ("Invalid event id
        // received\n"); per this file's convention the CONDITION is what is rebuilt.
        meResourceStage = E_RESOURCE_LOAD_WFVEHICLELISTAQUIRE;

        if ( lrReceiverQueue.GetCount() < 1 )
        {
            return false;
        }

        const CgsModule::Event* lpEvent = 0;
        s32                     liSize  = 0;
        lrReceiverQueue.GetFirstEvent( &lpEvent, &liSize );

        const BrnResource::GameDataIO::GetGameDataEvent* lpListReply =
            static_cast<const BrnResource::GameDataIO::GetGameDataEvent*>( lpEvent );

        CGS_ASSERT( lpListReply->miEventId == KI_VEHICLE_LIST_REQUEST,
                    "Invalid event id received" );                       // baked line 1108

        // (2026-08-21, wave T1 round 3: the cast target moved from the mis-scoped
        //  BrnTraffic::VehicleList forward declaration to the real BrnResource::VehicleList
        //  -- see the note beside the forward decls in BrnTrafficEntityModule.h. Same
        //  storage, same reply, correct type.)
        mpVehicleList = static_cast<const BrnResource::VehicleList*>(
            lpListReply->mHandle.mpResourceMemory );
    }
    // fall through -- LABEL_20. NOTE the console jumps to case 8, NOT case 4.

    case E_RESOURCE_LOAD_ATTRIBS:
        // ---- LABEL_20 (case 8) ------------------------------------------------------------
        //   v35 = a1 + 124128;  a1[190] = 8;  a1[124128] = 0;   (miResourceRequestCount = 0)
        //   LockForWrite(a2);
        //   for ( type = 0 ; type < mpData->muNumVehicleTypes ; ++type )
        //       <UnCompressVehicleAssetName>
        //       v62 = (*v35)++                                  ; event id = the running count
        //       <inlined LoadVehicle: miPoolId 1, meType 4 (ATTRIBS), AddEvent type 26>
        //   mReceiverQueue.Clear();  UnlockForWrite(a2);
        //
        // The event id is the RUNNING REQUEST COUNTER, which for this loop is numerically the
        // vehicle-type index -- but stage 7's own assert spells it as an index into the type
        // array ("liEventId < mpData->muNumVehicleTypes"), and the counter is what the two
        // WFLOAD stages compare their reply count against. Kept as the counter, faithfully.
        meResourceStage = E_RESOURCE_LOAD_ATTRIBS;

        RequestVehicleAssetsForEveryType( lpOutputBuffer, &lrReceiverQueue,
                                          mpData.operator->(), &miResourceRequestCount,
                                          BrnResource::E_ASSETSET_ATTRIBS );
        // fall through -- LABEL_27.

    case E_RESOURCE_WFLOAD_ATTRIBS:
        // ---- LABEL_27 (case 9) ------------------------------------------------------------
        //   v76 = a1[199]; v77 = a1[124128]; a1[190] = 9;
        //   if ( v76 < v77 ) goto LABEL_50 (return false);
        //   if ( a1[199] > 0 ) { walk every record, asserting its KIND word == 50 (.cpp:1214) }
        //
        // ⚠ THE DRAIN BINDS NOTHING. The attrib replies are counted and discarded; only the
        // PHYSICS drain below binds a handle. Reproduced, not "fixed".
        meResourceStage = E_RESOURCE_WFLOAD_ATTRIBS;

        if ( lrReceiverQueue.GetCount() < miResourceRequestCount )
        {
            return false;
        }

        DrainAssetReplies( &lrReceiverQueue );
        // fall through -- LABEL_33.

    case E_RESOURCE_LOAD_PHYSICS:
        // ---- LABEL_33 (case 6) ------------------------------------------------------------
        // Instruction-for-instruction LABEL_20 with meType 1 (PHYSICS) instead of 4, and with
        // BaseEventReceiverQueue::Clear INLINED rather than called (the modulo/wrap block at
        // 0x82746xxx over a1[197..202]; same semantics, so the out-of-line Clear is used).
        meResourceStage = E_RESOURCE_LOAD_PHYSICS;

        RequestVehicleAssetsForEveryType( lpOutputBuffer, &lrReceiverQueue,
                                          mpData.operator->(), &miResourceRequestCount,
                                          BrnResource::E_ASSETSET_PHYSICS );
        // fall through -- LABEL_41.

    case E_RESOURCE_WFLOAD_PHYSICS:
    {
        // ---- LABEL_41 (case 7) ------------------------------------------------------------
        //   v129 = a1[199]; v130 = a1[124128]; a1[190] = 7;
        //   if ( v129 < v130 ) return false;
        //   for each record:
        //       assert kind == 50                                       (.cpp:1291)
        //       v137 = *v132                                            ; reply.miEventId
        //       assert v137 < mpData->muNumVehicleTypes                 (.cpp:1298)
        //       CreateFromHandle(&a1[8*v137 + 120534], v132 + 8)        ; specs[type] = handle
        //       v138 = <ResourcePtr::operator->>(&a1[8*v137 + 120534])  ; sub_825E5210
        //       key  = FindVehicleTypeAttribKey_EXPENSIVE(a1, v137)
        //       VehicleTypeRuntime::Prepare(&a1[32*v137 + 121056], v138, key)
        meResourceStage = E_RESOURCE_WFLOAD_PHYSICS;

        if ( lrReceiverQueue.GetCount() < miResourceRequestCount )
        {
            return false;
        }

        const CgsModule::Event* lpEvent = 0;
        s32                     liSize  = 0;
        s32 liKind = lrReceiverQueue.GetFirstEvent( &lpEvent, &liSize );

        while ( lpEvent != 0 )
        {
            CGS_ASSERT( liKind == KI_RESPONSE_LOAD_GAMEDATA_ASSET,
                        "Invalid event id received" );                   // baked line 1291

            const BrnResource::GameDataIO::GetVehiclePhysicsResponse* lpReply =
                static_cast<const BrnResource::GameDataIO::GetVehiclePhysicsResponse*>( lpEvent );

            const s32 liVehicleType = lpReply->miEventId;

            CGS_ASSERT( static_cast<u32>( liVehicleType ) < mpData->muNumVehicleTypes,
                        "liEventId < mpData->muNumVehicleTypes" );       // baked line 1298

            // THE SEAT. Everything downstream that reads a traffic vehicle's deformation
            // model -- VehicleTypeRuntime::Prepare's bbox/axle extraction, and through it
            // Prepare stage 3's BoxVolume and Vehicle::InitialiseAsStatic's axle offsets --
            // reads it from here.
            maTrafficVehiclePhysicsSpecs[liVehicleType] =
                lpReply->GetVehiclePhysicsObjectHandle();

            // ---- the runtime seat (0x827472xx) --------------------------------------
            // *** UN-GATED 2026-08-21 (wave T1 round 3, closure item 3). Both halves of the
            // round-2 gate are closed: FindVehicleTypeAttribKey_EXPENSIVE @0x8273F0B8 is
            // bodied in THIS FILE (its ledger row's CgsStrStream.h primary_file was a
            // catch-all misattribution), and VehicleTypeRuntime::Prepare @0x82761B10 is
            // bodied in BrnTrafficVehicleTypeRuntime_Prepare.cpp. Recurring bug class (d)
            // -- "ledger-done but bodyless" -- is closed for this leg; the LNK2019 the gate
            // existed to avoid cannot happen because both bodies now exist.
            //
            // THE CONSOLE'S ORDER, reproduced exactly (0x82747210..0x82747238):
            //     CreateFromHandle( maTrafficVehiclePhysicsSpecs[type], reply.mHandle )
            //     lpSpec = maTrafficVehiclePhysicsSpecs[type].operator->()   (sub_825E5210)
            //     key    = FindVehicleTypeAttribKey_EXPENSIVE( type )
            //     maVehicleTypeRuntime[type].Prepare( lpSpec, key )
            // The handle is bound FIRST and the spec pointer is read back out of the member
            // that was just seated -- so the ORDER QUESTION the boot evidence raised is
            // answered inside this one arm and needs no stage reordering: stage 7 binds the
            // spec and calls Prepare on the SAME reply, one vehicle type at a time. Prepare
            // stage 3's AddDynamicVolume loop (further down this file) runs later in the
            // module's own prepare ladder, by which time every type has been through here.
            const BrnPhysics::Deformation::StreamedDeformationSpec* lpPhysicsSpec =
                maTrafficVehiclePhysicsSpecs[liVehicleType].operator->();

            maVehicleTypeRuntime[liVehicleType].Prepare(
                lpPhysicsSpec,
                FindVehicleTypeAttribKey_EXPENSIVE( static_cast<u32>( liVehicleType ) ) );

            if ( IsTrafficDiagOn() && CgsDev::Log::gpDebugPrint != 0 )
            {
                CgsDev::Log::DebugPrint* lpDiag = CgsDev::Log::gpDebugPrint;
                // [T1-scene] one-shot: the FIRST type whose scene box is now REAL. Pairs with
                // the existing "[T1-scene] FIRST traffic volume registered ... halfExtents=
                // (0,0,0) ... ZERO EXTENTS MEAN VehicleTypeRuntime::Prepare NEVER RAN" probe
                // in Prepare stage 3 -- if that one still prints zeros after this line has
                // printed non-zeros, the fault moved and it is NOT this leg. DELETE-WHEN-STABLE.
                static bool sbLogged = false;
                if ( !sbLogged )
                {
                    sbLogged = true;
                    const Vector3 lHalfSize =
                        maVehicleTypeRuntime[liVehicleType].GetBBoxHalfSize();
                    *lpDiag << "[T1-scene] VehicleTypeRuntime::Prepare seated type "
                            << liVehicleType << " bbox halfExtents*1000=("
                            << static_cast<s32>( lHalfSize.x * 1000.0f ) << ","
                            << static_cast<s32>( lHalfSize.y * 1000.0f ) << ","
                            << static_cast<s32>( lHalfSize.z * 1000.0f ) << ")\n";
                }
            }

            const CgsModule::Event* lpNext = 0;
            liKind = lrReceiverQueue.GetNextEvent( lpEvent, &lpNext, &liSize );
            lpEvent = lpNext;
        }
    }
    // fall through -- LABEL_49.

    case E_RESOURCE_ACQUIRE_COUNT:
        // 0x82746xxx LABEL_49 / case 12: `result = 1; a1[190] = 12;`
        meResourceStage = E_RESOURCE_ACQUIRE_COUNT;
        return true;

    default:
        // The console's own default arm. Stages 4/5 (LOAD_VEHICLES / WFLOAD_VEHICLES) and
        // 10/11 (LOAD_WHEELS / WFLOAD_WHEELS) have no case label in the shipped jump table
        // either, so landing here really is a "weird state".
        CGS_ASSERT( false, "TrafficEntityModule::LoadData in a weird state" );  // baked line 1320
        return false;
    }
}

// ============================================================================
// BrnTraffic::TrafficEntityModule::Prepare  @ 0x8274A578  (252 insns)     *** PARTIAL ***
//
// DWARF :1079 `virtual bool Prepare(OutputBuffer_Prepare*)` -- declared NON-virtual here, and
// the reason is NOT "there is no base sub-object". There is one: the class reads
// `TrafficEntityModule : public CgsModule::ModuleSingleBuffered` (BrnTrafficEntityModule.h:379)
// and stage 1 below calls straight through it. The real reason is the one already recorded at
// BrnTrafficEntityModule.h:600-603 -- the base's `virtual bool Prepare()` takes NO argument
// (CgsModuleSingleBuffered.h:42), so this one-argument overload HIDES rather than overrides it,
// and marking it virtual would add a vtable slot at this position that the console does not
// have. FLAG for the wave that reconstructs the module vtable.
//   Base-sub-object placement, from the asm: @0x8274A600 `mr r3,r26` immediately before
//   @0x8274A604 `bl CgsModule__ModuleSingleBuffered__Prepare` -- `this` is passed UNADJUSTED,
//   so the base sits at offset 0, which is what makes the single-inheritance spelling correct.
//   The former banner here also claimed retyping would "change the mangled name and orphan
//   BrnWorldModule.cpp:973 and the WorldLinkStubs.cpp gate". Both halves are wrong and neither
//   is load-bearing: `virtual` is not part of a member function's decorated name under MSVC, so
//   adding it orphans no call site; and the WorldLinkStubs.cpp Prepare gate it refers to has
//   been retired since wave Q7 (WorldLinkStubs.cpp:741-743). Only the vtable-slot argument
//   above stands.
//
// A resumable six-stage ladder on mePrepareStage @+0x2F0 (EPrepareStage, header :486), driven
// once per frame by WorldModule::Prepare stage eWorldPrepareTrafficEntityModule
// (BrnWorldModule.cpp:966-999), which drains this module's request pipe on every FALSE.
//
// REAL as of 2026-08-21 (wave T1 round 2, cluster R2C): stages 0, 1, 2, 3 and 5, the
// console's fall-through chaining, and the default assert. Only stage 4 (debug UI) is gated.
//
// ⭐ STAGE 1 FLIPPED. Its gate banner already said the base and its Construct were both real
//    and that the leg was held back "only because the base Prepare allocates both DataBuffers
//    through CreateInput/OutputDataStructure and this cluster cannot boot-test that". The
//    conductor boot-tests after this round, so the leg is LIVE: `bl CgsModule::
//    ModuleSingleBuffered::Prepare` @0x8274A604, with `this` UNADJUSTED (the base sub-object
//    is at offset 0 -- the same fact BrnTrafficEntityModule.h's class banner pins), and the
//    console's `if (!...) break;` early-out, which is what makes the ladder resumable.
//    ⚠ IF THE BOOT STALLS AT THE WORLD PREPARE LADDER, THIS IS THE FIRST LINE TO SUSPECT:
//    the revert is to restore the LogMissingLeg call in its place. Nothing else changes.
//
// ⭐ STAGE 3 LANDED. Its gate banner was stale THREE times over: the storage is no longer an
//    opaque blob (wave T1 C1 gave the class its real member list, so `this + 0x76390` is
//    `&maVehicleTypeRuntime[0].mBBoxHalfSize` and nothing is out of bounds);
//    rw::collision::BoxVolume::Initialize @0x82BAA188 has had a real body since wave Q5
//    (vendor/renderware/collision/BoxVolume.cpp) and InSceneUpdateInterface::AddDynamicVolume
//    @0x822B1518's DWARF 64-bit form landed in the same wave; and the loop is over VEHICLE
//    TYPES, not traffic-light instances (the scout's own item 4 flagged that reading as
//    needing re-verification -- it is settled below).
//
//   stage 4  DebugComponent::Register(*(this + 0x727B0)) + rw::IResourceAllocator::
//            AllocateMemoryResource(GetDebugAllocator(), 2560, 16, 0) stored into
//            `this + 0x7286C`, then an 8-iteration init loop over `this + 0x79388` /
//            `this + 0x7938C` with a 136-byte (0x88) stride -- again past the end of the
//            modelled storage. Debug-UI only.
//            OFFSETS RE-MEASURED FROM THE ASM (0x8274A8A0..0x8274A918), not inherited:
//              0x8274A8A4/A8  `ori r11,r11,0x27B0` + `lwzx r3,r26,r11` -- Register's argument
//                             is the POINTER LOADED FROM this + 0x727B0, not that address.
//              0x8274A8B4..C0 `li r4,0xA00 ; li r5,0x10 ; li r6,0 ; bl AllocateMemoryResource`
//              0x8274A8C8/D8  `ori r10,r11,0x286C` + `stwx r3,r26,r10` -- the allocation
//                             result lands at this + 0x7286C. The neighbouring
//                             `ori r9,r11,0x2870` + `stwx r31,r26,r9` (0x8274A8D0/E8) is a
//                             SEPARATE zero store into this + 0x72870.
//              0x8274A8E0/F0  `ori r29,r10,0x9388` / `ori r30,r10,0x938C` -- the loop walks
//                             this + 0x79388 (the iteration index) and this + 0x7938C (zero),
//                             `mulli r10,r11,0x88` stride, `cmpwi r11,8` bound.
//
// ⚠️ NOTE stage 3 RETURNS FALSE after doing its work (`result = 0; mePrepareStage = 4`), so
//    the console itself always needs at least one extra frame here.
// ============================================================================
bool TrafficEntityModule::Prepare( BrnTrafficIO::OutputBuffer_Prepare* lpOutputBuffer )
{
    // 2026-08-21: the "KU_OFFSET_RECEIVER_QUEUE + sizeof(queue) < 0x53790" pin that used to
    // stand here is RETIRED -- it asserted an X360 byte budget about a host object, which is
    // exactly the class of pin wave T1 exists to remove. Nothing replaced it as a byte pin and
    // nothing needs to: mReceiverQueue and mTrafficLightManager are both named members, so
    // header declaration order (BrnTrafficEntityModule.h:751 / :816) is the whole fact. What
    // TrafficEntityModule::_AssertLayout does pin by ORDER is the ten-member state block this
    // ladder reads, whose run ends `meTearingDownState before mReceiverQueue`.
    TrafficReceiverQueue& lrReceiverQueue = mReceiverQueue;             // console +0x314

    switch ( mePrepareStage )                                           // console +0x2F0
    {
    case E_PREPARESTAGE_START:
    {
        // ---- 0x8274A5D4 ------------------------------------------------------------
        // ⭐ RETIRED 2026-08-21 (wave T1, cluster C4): the "[FLAG PC bring-up]
        // EventReceiverQueue<4096,16>::Construct RELOCATED" block that stood here is GONE.
        // It existed only because TrafficEntityModule::Construct @0x82740220 was an inert
        // gate in WorldLinkStubs.cpp, so the queue's mpBuffer was NULL when the GameData
        // reply arrived. Construct now has a real body
        // (BrnTrafficEntityModule_wT1_01.cpp) and calls mReceiverQueue.Construct() where the
        // console calls it -- inside Construct, once per instance. The ReceiverQueueBinding
        // per-object guard above is kept ONLY as the residual safety net for a build whose
        // WorldLinkStubs.cpp Construct gate has not been retired yet; it logs and self-heals
        // instead of memcpy'ing through NULL.
        // DELETE-WHEN: WorldLinkStubs.cpp's TrafficEntityModule::Construct gate is retired
        // (cluster C7) and a boot proves the queue is bound before Prepare stage 0 runs.
        if ( !ReceiverQueueBinding::IsBound( lrReceiverQueue ) )
        {
            lrReceiverQueue.Construct();

            static bool sbLoggedReceiverQueueConstruct = false;
            if ( !sbLoggedReceiverQueueConstruct
                 && (CgsDev::Message::gxMessageFilterFlags & 1) != 0
                 && CgsDev::Log::gpDebugPrint != 0 )
            {
                sbLoggedReceiverQueueConstruct = true;
                *CgsDev::Log::gpDebugPrint
                    << "[T1-traffic-leg] TrafficEntityModule::Prepare stage 0 found an UNBOUND "
                       "mReceiverQueue -- TrafficEntityModule::Construct @0x82740220 did not run "
                       "(its WorldLinkStubs.cpp gate is still live). Binding it here as a "
                       "safety net [FLAG PC bring-up]\n";
            }
        }

        // The console's own stage-0 body.
        lrReceiverQueue.Clear();                                    // BaseEventReceiverQueue::Clear

        // ⭐ RESTORED 2026-08-21 (C1 fix round). Both stores WERE parked here as unnameable;
        // both re-derive to modelled members, so parking them was dropping real side effects.
        //
        //   0x8274A5E8  addi r11, r11, 0x13A0   ; r11 = this + 0x713A0 == this + 463776
        //   0x8274A5F4  std  r31, 0(r11)        ; one 64-bit zero  -> the bit array
        //   0x8274A5EC  ori  r10, r10, 0x1B32   ; r10 = 0x71B32 == 465714
        //   0x8274A5F8  stbx r31, r26, r10      ; one byte zero    -> mbInReplay
        //
        // +0x713A0 == 463776 is maTrafficPhysicsInfoListBits (:686), NOT mRaceCarState (:708).
        // mRaceCarState begins at 463904: UpdateTimers @0x82715858 pins :697-:701 at
        // 463860/463861/463864/463868/463872 (the byte frame counter it increments and resets
        // beside `463861 = 1`; `463864 += dt` clamped to 0.1 == KF_UPDATE_TIME_DELTA_NO_SLOWMO)
        // and :706 mfSimTimeStepVec (16 bytes, 16-aligned) at 463888. 463776 is also below
        // mLocalPlayerPosition (:693), which UnhideAllTraffic @0x8274A500 pins at 463824 with an
        // `lvx` feeding KillAllTrafficInCylinder. The positive anchor is UpdateSerialiser
        // @0x8272DA80: `SetPhysicsData(serialiser, this + 463776, this + 360976)` and
        // 463776 - 360976 == 102800 == 25 * 4112 == KU_MAX_PHYSICAL_TRAFFIC_VEHICLES *
        // sizeof(TrafficPhysicsInfo) -- so maTrafficPhysicsInfoList (:685) spans
        // 360976..463776 and the bit array starts exactly at 463776. BitArray<25> is one
        // 64-bit field, which is why the console clears it with a single `std`; Prepare() is
        // that same whole-array zero.
        maTrafficPhysicsInfoListBits.Prepare();

        // +0x71B32 == 465714 is mbInReplay (:757), NOT interior mFuzzyBehaviours (:753) --
        // mFuzzyBehaviours ENDS at 465712, where muUpdateCount (:755, u16) lives:
        // UpdateDecisionFrame @0x8274E508 does `++*(this + 465712)` and tests it `>= 0x64`
        // (== KU_START_PROTECT_UPDATE_FRAME_ONLINE), and UpdateRaceCarHulls @0x82721460
        // compares *(this + 465712) against HullChangeInfo::muUpdateFrame with a `< 0x7FFF`
        // wrap test. u16 at 465712 puts :757/:758 at 465714/465715. A fresh Prepare clearing
        // the replay flag is exactly what the console means here.
        mbInReplay = false;

        mePrepareStage = E_PREPARESTAGE_MANAGER;
    }
    // fall through -- 0x8274A600 is the case-1 entry AND the case-0 fall-through target.

    case E_PREPARESTAGE_MANAGER:
        // ⭐⭐ THE STAGE-1 GATE IS RETIRED -- 2026-08-21, wave T1 round 2, cluster R2C.
        //
        //   0x8274A600  LABEL_3 entry / case-0 fall-through target
        //   0x8274A604  bl  CgsModule::ModuleSingleBuffered::Prepare      ; r3 == `this`,
        //                                                                ;   UNADJUSTED
        //               if ( !result ) goto LABEL_22   -> `result = 0`    ; the resumable
        //                                                                ;   early-out
        //   0x8274A6xx  *(a1 + 752) = 2                                   ; mePrepareStage = 2
        //
        // The gate that stood here had already been re-reasoned twice and its LAST reason was
        // explicitly "flip it and boot-test" -- the base and its Construct are both real (wave
        // T1 C1 gave the class its `: public CgsModule::ModuleSingleBuffered` base; C4 gave
        // TrafficEntityModule::Construct @0x82740220 a body that runs the base Construct), and
        // the only open question was the base Prepare's DataBuffer allocation, which is a
        // RUNTIME question this cluster cannot answer and the conductor can. Flipped.
        //
        // `this` is passed unadjusted because the base sub-object is at offset 0 -- asm-attested
        // here and pinned in BrnTrafficEntityModule.h's class banner.
        //
        // ⚠️⚠️ THE FLIP HAS ONE HARD PRECONDITION, AND IT IS NOT OPTIONAL:
        //     TrafficEntityModule::Construct @0x82740220 MUST HAVE RUN.
        // Its very last store is `mbIsNewModule = true` (0x82741758/60 `li r11,1 ; stb r11,
        // 4(r31)`), and CgsModule::ModuleSingleBuffered::Prepare @0x8286E7A0 branches on that
        // byte at every stage: non-zero -> skip the whole old-style DataStructure ladder and
        // `return 1` at LABEL_20; zero -> call CreateInputDataStructure through the vtable and
        // `if (!v4) return 0`. This module overrides NEITHER CreateInputDataStructure nor
        // CreateOutputDataStructure (the DecFIGS TrafficEntityModule declares neither), so with
        // mbIsNewModule false the base placeholder returns nullptr and the base Prepare returns
        // FALSE EVERY FRAME, FOREVER -- a boot hang at WorldModule::Prepare's traffic stage.
        //
        // That store was MISSING from the reconstructed Construct until this round; it is added
        // (one line, fully bannered) in BrnTrafficEntityModule_wT1_01.cpp. But the body only
        // runs if the WorldLinkStubs.cpp TrafficEntityModule::Construct gate (~:729) has been
        // RETIRED -- while that inert gate is live, Construct never executes, mbIsNewModule
        // stays at the base's zero, and this leg stalls the ladder.
        //
        // ⚠️ CONDUCTOR: retire the WorldLinkStubs.cpp Construct gate in the SAME build as this
        //    flip. If the boot hangs on the world Prepare ladder, the [T1-prepare] one-shot
        //    below names the cause in one line; the revert is to restore a LogMissingLeg call
        //    here and let the ladder walk past.
        if ( !CgsModule::ModuleSingleBuffered::Prepare() )
        {
            // ---- [T1-prepare] bring-up probe (NOT IN THE X360 BINARY) --------------------
            // One-shot on the FIRST false. A single false is normal only if the base is
            // mid-ladder; for this module the base either passes on the first call or never
            // passes at all, so seeing this line at all is the diagnosis. DELETE-WHEN-STABLE.
            static bool sbLogged = false;
            if ( IsTrafficDiagOn() && !sbLogged && CgsDev::Log::gpDebugPrint != 0 )
            {
                sbLogged = true;
                *CgsDev::Log::gpDebugPrint
                    << "[T1-prepare] ModuleSingleBuffered::Prepare returned FALSE for the "
                       "traffic module. If this repeats every frame the world Prepare ladder "
                       "is STALLED: it means mbIsNewModule is still false, i.e. "
                       "TrafficEntityModule::Construct @0x82740220 never ran -- retire the "
                       "WorldLinkStubs.cpp Construct gate [DELETE-WHEN-STABLE]\n";
            }

            return false;                                // console: LABEL_22, `result = 0`
        }

        mePrepareStage = E_PREPARESTAGE_LOADINGWORLD;
        // fall through -- 0x8274A61C.

    case E_PREPARESTAGE_LOADINGWORLD:
        // 0x8274A624 -- the resource ladder. REAL (partial body above).
        if ( !LoadData( lpOutputBuffer ) )
        {
            return false;
        }
        mePrepareStage = E_PREPARESTAGE_VOLUMES;
        // fall through -- 0x8274A63C.

    case E_PREPARESTAGE_VOLUMES:
    {
        // ⭐⭐ THE STAGE-3 GATE IS RETIRED -- 2026-08-21, wave T1 round 2, cluster R2C.
        // 0x8274A63C..0x8274A880.
        //
        // THIS IS LOAD-BEARING FOR THE PARKED-CAR WAVE. It publishes ONE shared collision
        // volume per vehicle TYPE into the scene manager, keyed KU_HACK_BASE_VOLUME_ID + type.
        // Without it the scene has no volume for a traffic car to instance, so a car that
        // spawns can never take part in the fine query.
        //
        // WHAT THE LOOP ITERATES -- SETTLED, and the scout's own item 4 asked for exactly this
        // re-verification. It is VEHICLE TYPES, not traffic-light instances or kill zones:
        //   0x8274A644/64C  r24 = this + 0x71840                   ; &mpData
        //   0x8274A66C/674  r27 = this + 0x76390                   ; &maVehicleTypeRuntime[0]
        //                                                          ;    .mBBoxHalfSize
        //                                                          ;   (0x76380 + 16)
        //   0x8274A86C      addi r27, r27, 0x80                    ; stride 128 ==
        //                                                          ;   sizeof(VehicleTypeRuntime)
        //   0x8274A870/874  bl TrafficData::operator-> ; lhz r11, 0x16(r3)
        //   0x8274A878/87C  cmplw r28, r11 ; blt                   ; bound == muNumVehicleTypes
        // and Feb-2007's own stage 3 (BrnTrafficEntityModule.cpp:386-415) is the same loop with
        // the same names -- `maTrafficVehicleTypeRuntimeData[luVehicleType].mBBoxHalfSize`,
        // `KU_HACK_BASE_VOLUME_ID + luVehicleType`, `E_ENTITYTYPEFLAG_TRAFFIC_VEHICLE`.
        //
        // THE VMX HALF, lane by lane (0x8274A6B0..0x8274A6F8), which is the one place the ship
        // diverges from the leak:
        //   v0  = lvx(&rt.mBBoxHalfSize)                   the four-lane half-extent
        //   v11/v12/v10 = splat(v0.x) / splat(v0.y) / splat(v0.z)
        //   v12 = vminfp(v11, v12) ; v12 = vminfp(v12, v10)         min( hx, hy, hz )
        //   v13 = vmulfp128(v12, splat(0.49000001))                 * KF_..._FATNESS_SCALE
        //   v9  = splat(*(float*)unk_820BA4C4)                      == 0.44f (DUMPED)
        //   v13 = vminfp(v9, v13)                                   clamp
        //   v1  = vsubfp(v0, v13)                                   the box's half-extents
        //   stvx v13 -> the frame slot the next `lfs f0` reads      the FATNESS
        // Feb-2007 subtracts a FIXED KF_VEHICLE_BBOX_FATNESS; the ship's is per-type clamped.
        // The asm wins.
        //
        // THE THREE TRIPWIRES are over the POST-SUBTRACTION half-extents (v1), one per lane,
        // baked .cpp lines 946/947/948 -- the leak's 402/403/404 plus the usual drift. Each is
        // a `vcmpgefp.` against a zero the frame stages with `stfs f31` + three zero words,
        // i.e. `>= 0.0f`. Non-gating: the console re-loads v1 and carries on.
        //
        // THE RESOURCE + THE POST (0x8274A80C..0x8274A860), register for register:
        //   addi r11, r1, var_10B0 ; 5x stw r31(=0)        rw::Resource, five words zeroed
        //   addi r11, r1, var_1080 ; stw r11, var_10B0(r1) word 0 = the volume's memory block
        //   bl   BoxVolume::Initialize                     r3 = the Resource, r30 = the volume
        //   addi r11, r28, 0x24 ; clrldi r11, r11, 32      the VolumeId == (u64)(type + 36),
        //                                                  owner byte 0
        //   lfs  f0, var_1090(r1) ; stfs f0, 0x50(r30)     volume->mfRadius = the fatness
        //                                                  (== Feb-2007's SetRadius)
        //   mr   r3, r21 ; bl OutputBuffer_Prepare::GetSceneInputInterface @0x827109E0
        //   ld   r4, 0(r22) ; mr r5, r30 ; mr r6, r29(=8)  AddDynamicVolume(id, volume, flag)
        //
        // The console passes the WHOLE 64-bit id in r4, so the DWARF's
        // `AddDynamicVolume(VolumeId, const void*, VolumeTypeFlags)` overload is the correct
        // one -- the same width trap wave Q5 recorded for the race car
        // (BrnActiveRaceCar_wQ5_01.cpp's [FLAG BLOCKED 1] note). Here the high dword really is
        // zero (`clrldi`), but going through the narrow EntityId overload would still bind the
        // wrong mangled name for a key that IS a VolumeId.
        //
        // ⚠️ THE STAGE STILL RETURNS FALSE (`result = 0` @0x8274A880) -- the console always
        //    spends one extra frame here. Unchanged.
        lpOutputBuffer->LockForWrite();

        for ( u32 luVehicleType = 0;
              luVehicleType < mpData->muNumVehicleTypes;
              luVehicleType++ )
        {
            const CgsSceneManager::VolumeId lVolumeId(
                static_cast<u64>( KU_HACK_BASE_VOLUME_ID + luVehicleType ) );

            // `lvx128 v0, r0, r27` with r27 == &maVehicleTypeRuntime[type].mBBoxHalfSize.
            // Read through the type's own accessor (the console inlines it -- see the
            // accessor block's FLAG in BrnTrafficVehicleTypeRuntime.h).
            const Vector3 lBBoxHalfSize =
                maVehicleTypeRuntime[luVehicleType].GetBBoxHalfSize();

            const f32 lfHalfX = lBBoxHalfSize.x;
            const f32 lfHalfY = lBBoxHalfSize.y;
            const f32 lfHalfZ = lBBoxHalfSize.z;

            // min( hx, hy, hz ) * 0.49f, clamped to 0.44f.
            f32 lfSmallestHalfExtent = lfHalfX;
            if ( lfHalfY < lfSmallestHalfExtent ) { lfSmallestHalfExtent = lfHalfY; }
            if ( lfHalfZ < lfSmallestHalfExtent ) { lfSmallestHalfExtent = lfHalfZ; }

            f32 lfFatness = lfSmallestHalfExtent * KF_VEHICLE_BBOX_FATNESS_SCALE;
            if ( KF_VEHICLE_BBOX_FATNESS_MAX < lfFatness )
            {
                lfFatness = KF_VEHICLE_BBOX_FATNESS_MAX;
            }

            const f32 lfBoxHalfX = lfHalfX - lfFatness;
            const f32 lfBoxHalfY = lfHalfY - lfFatness;
            const f32 lfBoxHalfZ = lfHalfZ - lfFatness;

            CGS_ASSERT( lfBoxHalfX >= 0.0f, "lBBoxHalfSize.X() >= 0.0f" );   // baked line 946
            CGS_ASSERT( lfBoxHalfY >= 0.0f, "lBBoxHalfSize.Y() >= 0.0f" );   // baked line 947
            CGS_ASSERT( lfBoxHalfZ >= 0.0f, "lBBoxHalfSize.Z() >= 0.0f" );   // baked line 948

            // The console's own stack block. AddDynamicVolume block-copies 128 bytes FROM the
            // volume pointer (`li r5, 0x80` @0x822B1534), so the block must stay at least that
            // large; Feb-2007 sizes it `rw::math::Vector3 laResourceBuffer[256]` and asserts
            // `sizeof(laResourceBuffer) >= BoxVolume::GetResourceDescriptor().GetSize()`.
            // NOT the same size as this tree's other producer of this SDK entry point:
            // BrnActiveRaceCar_wQ5_01.cpp (AddToScene) uses `u8 laVolumeStorage[128]`, sized to
            // exactly the 0x80 the copy reads. KU_VOLUME_SCRATCH_BYTES is 4096 here because this
            // block follows Feb-2007's `Vector3 laResourceBuffer[256]` (3072 B) rounded up, and
            // it is a strict superset of the 128 the over-read needs, so it is safe -- but a
            // future owner reconciling the two producers should know they differ, and that 128
            // is the floor, not 4096. Only the rw::Resource + Initialize spelling is shared.
            u8 laVolumeStorage[KU_VOLUME_SCRATCH_BYTES];

            rw::Resource lVolumeResource = {};                      // 5x stw 0
            lVolumeResource.m_baseResources[0] = laVolumeStorage;   // stw r11, var_10B0(r1)

            rw::collision::BoxVolume* lpVolume = rw::collision::BoxVolume::Initialize(
                lVolumeResource, lfBoxHalfX, lfBoxHalfY, lfBoxHalfZ );

            // `stfs f0, 0x50(r30)` == Volume::mfRadius, which Feb-2007 spells
            // `lpBoxVolume->SetRadius( KF_VEHICLE_BBOX_FATNESS )`. BoxVolume::BoxVolume
            // @0x82BAA0F0 has just written a ZERO there, so this is a plain overwrite.
            lpVolume->mfRadius = lfFatness;

            lpOutputBuffer->GetSceneInputInterface()->AddDynamicVolume(
                lVolumeId, lpVolume, KU8_TRAFFIC_VEHICLE_VOLUME_TYPE_FLAG );

            // ---- [T1-scene] bring-up probe (NOT IN THE X360 BINARY) ---------------------
            // One-shot on the FIRST traffic volume the module registers, with the extents so
            // a zero-sized box (the symptom of LoadData stage 7's gated
            // VehicleTypeRuntime::Prepare) is visible without a debugger. DELETE-WHEN-STABLE.
            {
                static bool sbLogged = false;
                if ( IsTrafficDiagOn() && !sbLogged && CgsDev::Log::gpDebugPrint != 0 )
                {
                    sbLogged = true;
                    *CgsDev::Log::gpDebugPrint
                        << "[T1-scene] FIRST traffic volume registered: volumeId="
                        << static_cast<s32>( KU_HACK_BASE_VOLUME_ID + luVehicleType )
                        << " typeFlag=" << static_cast<s32>( KU8_TRAFFIC_VEHICLE_VOLUME_TYPE_FLAG )
                        << " halfExtents=(" << lfBoxHalfX << ", " << lfBoxHalfY
                        << ", " << lfBoxHalfZ << ") fatness=" << lfFatness
                        << " of " << static_cast<s32>( mpData->muNumVehicleTypes )
                        << " vehicle types."
                        << " ZERO EXTENTS MEAN VehicleTypeRuntime::Prepare NEVER RAN"
                           " [DELETE-WHEN-STABLE]\n";
                }
            }
        }

        lpOutputBuffer->UnlockForWrite();

        mePrepareStage = E_PREPARESTAGE_DEBUG;
        return false;                                    // console: `result = 0` at 0x8274A880
    }

    case E_PREPARESTAGE_DEBUG:
    {
        // 0x8274A8A0..0x8274A918 -- PARTIAL as of 2026-08-21 (wave T1 round 2, cluster R2C).
        // The gate used to cover the WHOLE stage on the strength of "past the end of the
        // modelled storage", which stopped being true when the opaque blob went. Its SECOND
        // half is plain member seeding and is landed below; only the debug-allocator half
        // stays gated, and for a real reason (a missing wire, not a missing layout).
        {
            static bool sbLogged = false;
            LogMissingLeg( sbLogged,
                "Prepare stage 4 FIRST half: DebugComponent::Register(mpDebugComponent) + "
                "BrnResource::GetDebugAllocator() + rw::IResourceAllocator::"
                "AllocateMemoryResource(2560, 16, 0) -> mpaDEBUGVehicleFuzzyLogic, plus the "
                "companion muDEBUGVehicleFuzzyLogicCount = 0 store. All three members are real; "
                "what is missing is the DebugComponent::Register / HeapResourceAllocator wire. "
                "DEBUG-UI ONLY -- nothing on the parked-car path reads either member" );
        }

        // ---- the console's SECOND half, 0x8274A8E0..0x8274A918 -- REAL -------------------
        //   ori r29, r10, 0x9388                 ; r29 = 0x79388 == &maStoredAITrafficData[0]
        //                                        ;                    .meRaceCarIndex
        //   ori r30, r10, 0x938C                 ; r30 = 0x7938C == ....miNumTrafficIDs
        //   mulli r10, r11, 0x88                 ; stride 136 == sizeof(StoredAITrafficData)
        //   stwx r11, r10, r29                   ; meRaceCarIndex = the loop index
        //   stwx r31(=0), r10, r30               ; miNumTrafficIDs = 0
        //   cmpwi r11, 8                         ; bound == E_ACTIVE_RACE_CAR_INDEX_COUNT
        // -- and Feb-2007 spells exactly this at BrnTrafficEntityModule.cpp:432-437, over an
        // `EActiveRaceCarIndex leRaceCarIndex` loop variable. The console's `operator++(v55)`
        // call is the enum's own increment operator, de-inlined back to a plain loop here.
        for ( s32 liRaceCarIndex = 0;
              liRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT;
              liRaceCarIndex++ )
        {
            maStoredAITrafficData[liRaceCarIndex].meRaceCarIndex =
                static_cast<EActiveRaceCarIndex>( liRaceCarIndex );
            maStoredAITrafficData[liRaceCarIndex].miNumTrafficIDs = 0;
        }

        mePrepareStage = E_PREPARESTAGE_DONE;
    }
    // fall through -- LABEL_21.

    case E_PREPARESTAGE_DONE:
        // LABEL_21: `result = 1; *(this+0x2F4) = 0;`
        meReleaseStage = E_RELEASESTAGE_START;

        // -------------------------------------------------------------------------------
        // ⭐⭐ THE meState BRING-UP LATCH IS RETIRED -- 2026-08-21, wave T1, cluster C4.
        //
        // What stood here was an EnterRunningState @0x827080E8 store-set relocated to the
        // tail of Prepare, because neither PostPhysicsUpdate @0x8274E6D0 nor Reset
        // @0x8272CDA0 had a body. It did make PrePhysicsUpdate's meState == E_STATE_RUNNING
        // early-out pass -- but it did so by SKIPPING THE ENTIRE E_STATE_STARTING_UP LADDER,
        // and E_STARTINGUPSTATE_POPULATING is the only place in the whole module that ever
        // creates a parked car (RecalculateActiveHulls -> SpawnNewTraffic -> FillNewHull ->
        // StaticVehicles_Generate -> StaticVehicles_CreateNewVehicles). Any wave-1 build that
        // kept the latch would render an empty world no matter how good the render leg was.
        //
        // Reset @0x8272CDA0 (which drives meState to E_STATE_INVALID and then
        // EnterStartingUpState) and PostPhysicsUpdate @0x8274E6D0's STARTING_UP arm (which
        // drives POPULATING -> WAITING_FOR_STREAMING -> EnterRunningState) both have bodies
        // now, in BrnTrafficEntityModule_wT1_01.cpp. The state machine runs for real.
        //
        // CONSEQUENCE FOR wQ7_01's PrePhysicsUpdate, stated plainly: until
        // TrafficEntityModule::PreSceneUpdate @0x8274A968 is bodied (it owns the
        // WAITING_FOR_PLAYER -> POPULATING transition and is still an inert gate in
        // WorldLinkStubs.cpp), meState stays E_STATE_STARTING_UP and PrePhysicsUpdate's
        // early-out fires every frame -- i.e. the prop->traffic light rings stop being
        // drained. That is a REGRESSION OF THE wQ7 BRING-UP and it is deliberate: it trades a
        // faked running state for a real one, and it is exactly one function away from
        // closing. See the C4 report's park list.
        // -------------------------------------------------------------------------------
        return true;

    default:
        CGS_ASSERT( false, "0" );                        // baked line 1003
        return false;
    }
}

}
