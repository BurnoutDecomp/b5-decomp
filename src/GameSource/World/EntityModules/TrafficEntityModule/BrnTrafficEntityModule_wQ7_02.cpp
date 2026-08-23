// ============================================================================
// BrnTrafficEntityModule_wQ7_02.cpp -- the traffic module's prepare and resource ladders.
//
//   * TrafficEntityModule::FindVehicleTypeAttribKey_EXPENSIVE @0x8273F0B8  COMPLETE
//   * TrafficEntityModule::LoadData @0x82746A88 (465 insns)  gate-free
//   * TrafficEntityModule::Prepare  @0x8274A578 (252 insns)  PARTIAL, one gate left
//
// Prepare stages 0, 1, 2, 3 and 5 are real, with the console's fall-through chaining and the
// default assert. Only stage 4's debug-UI half is gated, and it is the one LogMissingLeg call
// site in the file. LoadData runs the whole ladder: LoadTrafficLanes, bind mpData and publish
// the streamer catalogue, GetVehicleList, N x LoadVehicle ATTRIBS, N x LoadVehicle PHYSICS,
// bind maTrafficVehiclePhysicsSpecs, done.
//
// BOOT RISK: Prepare is a resumable multi-frame ladder that returns false until each reply
// lands, so if a reply never arrives on this route the world Prepare ladder stalls at the
// traffic stage and the game never finishes loading. The revert is per-leg: restore a
// LogMissingLeg call in whichever stage stalls.
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
#include <cstring>   // strstr (the CgsIDUnCompress space truncation)

namespace BrnTraffic
{
namespace
{
    // Is this queue's backing buffer bound yet? `mpBuffer` is protected on
    // CgsModule::BaseEventReceiverQueue and that shared type exposes no accessor, so it is read
    // through a derived-class pointer-to-member, the one form of protected access available
    // without growing a type this file does not own. Zero-initialised module storage has
    // mpBuffer == 0, BaseEventReceiverQueue::Construct binds it to the embedded maBuffer, and
    // Clear() never touches it. So this is a per-object "already Constructed?" test.
    struct ReceiverQueueBinding : public CgsModule::BaseEventReceiverQueue
    {
        static bool IsBound( const CgsModule::BaseEventReceiverQueue& lrQueue )
        {
            u8* CgsModule::BaseEventReceiverQueue::* const lpBufferMember =
                &ReceiverQueueBinding::mpBuffer;
            return (lrQueue.*lpBufferMember) != 0;
        }
    };

    // Console offsets for the named members below, attestation only. They are 32-bit-pointer
    // displacements and never valid on this LP64 host.
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

    // The vehicle-list request's event id: GetVehicleList passes literal 0 and the reply stage
    // asserts the payload's miEventId is 0 (.cpp:1108).
    const s32 KI_VEHICLE_LIST_REQUEST = 0;

    // The receiver-record KIND word both asset drains guard against (.cpp:1214 for ATTRIBS,
    // :1291 for PHYSICS). Same word position as EVENT_GET_TRAFFIC_LANES (55) and
    // EVENT_GET_SURFACE_LIST (66); 50 is the LoadGameData "asset is resident" reply. Promote
    // it to BrnGameDataEvents.h beside its two siblings when a wave that owns that header runs.
    const s32 KI_RESPONSE_LOAD_GAMEDATA_ASSET = 50;

    // The pool every per-vehicle-type asset request is posted to. Both inlined LoadVehicle
    // expansions store the literal 1 into the record's miPoolId
    // (`v149 = 1`, record +0x08) == BrnResource::E_POOL_PHYSICS.
    const s32 KI_VEHICLE_ASSET_POOL_ID = 1;

    // Prepare stage 3's two shrink constants, read into VMX splats at 0x8274A66C..0x8274A688.
    // 0.49000001f is a frame immediate; unk_820BA4C4 == 0.44f is dumped rodata (the four floats
    // at 0x820BA4BC are 50000 / 1.0 / 0.44 / 20.0), not a placeholder zero.
    //
    // SHIP-vs-LEAK, asm wins: Feb-2007 subtracts a fixed KF_VEHICLE_BBOX_FATNESS
    // (BrnTrafficEntityModule.cpp:395-411); the ship clamps per vehicle type,
    //     fatness = min( 0.44f, 0.49f * min( halfX, halfY, halfZ ) )
    // which is what keeps the three `>= 0.0f` asserts satisfiable for a small vehicle type.
    const f32 KF_VEHICLE_BBOX_FATNESS_SCALE = 0.49000001f;
    const f32 KF_VEHICLE_BBOX_FATNESS_MAX   = 0.44f;

    // `addi r11, r28, 0x24` @0x8274A834 -- the scene VOLUME key for vehicle type N is
    // N + 36, zero-extended to the 64-bit VolumeId (`clrldi r11, r11, 32`), i.e. owner byte
    // 0. Feb-2007 spells the base `KU_HACK_BASE_VOLUME_ID` (:395) and the name is kept.
    const u32 KU_HACK_BASE_VOLUME_ID = 36u;

    // `li r29, 8` @0x8274A684 -- the VolumeTypeFlags AddDynamicVolume tags each traffic
    // vehicle-type box with. Feb-2007 names it E_ENTITYTYPEFLAG_TRAFFIC_VEHICLE and
    // BrnEntityTypes.h:64 gives it 0x00000008, so leak and ship agree. That enum has no home
    // in this tree, so the immediate is named rather than decomposed, the same way
    // BrnActiveRaceCar_wQ5_01.cpp names KU8_RACECAR_VOLUME_TYPE_FLAG == 1.
    const u8 KU8_TRAFFIC_VEHICLE_VOLUME_TYPE_FLAG = 8u;

    // The console's stack scratch for the box image. AddDynamicVolume block-copies 128 bytes
    // FROM the volume pointer (`li r5, 0x80` @0x822B1534), so the block must stay at least
    // that large. 4,096 is the console's own size (Feb-2007's `Vector3 laResourceBuffer[256]`)
    // and is kept.
    const size_t KU_VOLUME_SCRATCH_BYTES = 4096;

    // One-shot leg gate, one named line per console leg with no body in the tree.
    // NOT IN THE X360 BINARY.
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

    // Inlining reversal: LoadData's two per-vehicle-type request passes (ATTRIBS at LABEL_20,
    // PHYSICS at LABEL_33) are identical apart from the asset set, so this is the shared "give
    // me the bare asset name for vehicle type N" step.
    //
    // In the pseudocode, `__ROL4__(v52, 3)` is `assetId * 8`, the attested VehicleAsset stride,
    // and the "+4" is Hex-Rays splitting the 8-byte CgsID load on a big-endian host, not a
    // member at +4. TrafficCarStreamer::SetAssetList @0x82753A38 does the same read over the
    // same array, which confirms the 8 bytes are one id and the truncation is at the first
    // space.
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

    // The re-rolled body of both request passes. A free function rather than a member, because
    // a member would need a declaration in BrnTrafficEntityModule.h; every value it needs is
    // passed in.
    //
    // The console re-reads mpData->muNumVehicleTypes through ResourcePtr::operator-> on every
    // iteration of the bound test (four separate operator-> calls appear in the loop). That is
    // an artefact of the resource-pointer accessor, not a semantic, so the bound is read once.
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
        }

        lpReceiverQueue->Clear();
        lpOutputBuffer->UnlockForWrite();
    }

    // The ATTRIBS drain (LABEL_27). Walks every queued record asserting its KIND word is 50 and
    // binds nothing. The console's `if ( v78 != -8 )` guard is Hex-Rays rendering "the queue has
    // no backing buffer" (mpBuffer + miStartOffset), which on the host is exactly "GetFirstEvent
    // handed back a null record".
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

    // LAYOUT: mReceiverQueue (BrnTrafficEntityModule.h:751) and mTrafficLightManager (:816) are
    // ordinary named members, so the compiler places them and declaration order is the whole
    // fact. What _AssertLayout pins is the ORDER of the ten-member state block this ladder
    // reads, the run ending `meTearingDownState before mReceiverQueue`.
}

// ============================================================================
// BrnTraffic::TrafficEntityModule::FindVehicleTypeAttribKey_EXPENSIVE @ 0x8273F0B8  COMPLETE
//
// Maps a traffic vehicle TYPE to the attribsys collection key of the car record it is skinned
// from:
//     type   -> mpData->mpaVehicleTypes[type].muAssetId          (byte at element +5)
//            -> mpData->mpaVehicleAssets[assetId].GetVehicleId() (a CgsID)
//            -> mpVehicleList->GetVehicleIndex(id)               (linear scan -- _EXPENSIVE)
//            -> mpVehicleList->GetVehicleData(index)             (240-byte entry)
//            -> entry->GetAttribCollectionKeyHash()              (AttribSysCollectionKey +0xA0)
//
// Console offsets, all resolved to named members: mpData +0x2C is mpaVehicleTypes, +0x34 is
// mpaVehicleAssets, element +5 is VehicleTypeData::muAssetId, and 8 * assetId is the attested
// VehicleAsset stride. The pseudocode's `__ROL4__(v7, 3)` is that * 8, and the loads at
// `v8 + v9` / `v8 + v9 + 4` are one CgsID read split by Hex-Rays, not two values.
//
// Asserts: :17180 "Unable to find vehicle" and :17185 "The vehicle list is empty!!" are
// reproduced; :17181 "has no AttribCollectionKey" is not, because it cannot fire (see the note
// at the call site). The console builds the first two through a StrStream so the id prints;
// CGS_ASSERT takes a literal, so the id is not interpolated.
//
// The fallback arm is not an error path. When the asset id is not in the vehicle list the
// console asserts the list is non-empty and takes ENTRY 0's key. Traffic cars are often not
// selectable player cars, so that is their ordinary path. It also skips the
// "no AttribCollectionKey" check, jumping straight to LABEL_12.
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

    // The console's :17181 assert is NOT reproduced, because it can never fire:
    //     0x8273F1C8  addic. r11, r22, 0xA0
    //     0x8273F1CC  bne    loc_8273F2D8          -> straight to GetHashKey
    // The test is on `r22 + 0xA0`, the ADDRESS of the entry's mAttribCollectionKey, so it is
    // non-zero for every input including r22 == 0. Spelling it as a null-entry test would fire
    // two released asserts on the null path where the console fires one (:17180).
    //
    // The pseudocode also invites a wrong reading of GetVehicleIndex: the console calls it
    // twice with the same register (`mr r4, r30` at 0x8273F114 and 0x8273F12C), so Hex-Rays
    // renders two calls whose arguments look different. One call is correct here.

    return lpEntry->GetAttribCollectionKeyHash();
}

// ============================================================================
// BrnTraffic::TrafficEntityModule::LoadData  @ 0x82746A88  (465 insns)
// DWARF :1266  bool LoadData(OutputBuffer_Prepare*)
//
// The module's resource-acquire ladder, a resumable switch on meResourceStage
// (EResourceAcquireStage, header :503).
//
// THE CONSOLE'S REAL STAGE ORDER, which is NOT the numeric order of the enum. Every arm falls
// THROUGH to the next on the same tick; the numbered `case` labels exist only so a later frame
// can re-enter mid-ladder:
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
// STAGE 9 BINDS NOTHING, and that is the console. The ATTRIBS drain (LABEL_27) walks every
// reply, asserts its kind is 50, and discards it. Only the PHYSICS drain (LABEL_41) binds. Do
// not "fix" this into a symmetric pair.
//
// ARRAY-BOUND MISMATCH, reproduced not fixed: stage 7 indexes maTrafficVehiclePhysicsSpecs,
// sized [KU_MAX_VEHICLE_ASSETS == 64] (BrnTrafficEntityModule.h:939), by the vehicle TYPE
// index, whose assert bound is mpData->muNumVehicleTypes (up to KU_MAX_VEHICLE_TYPES == 96).
// The console has the same mismatch, its array spanning 2,088 bytes == 65 console ResourcePtrs,
// so a data set with more than 64 vehicle types would overrun there too. Shipped
// B5TRAFFIC.BNDL is well under.
//
// The traffic-light data is entirely inside the stage-1 payload: the reply binds the whole
// TrafficData resource, and TrafficData::mTrafficLights is a by-value member of it
// (BrnTrafficDataResourceType.h:75), already relocated by TrafficData::FixUp.
// ============================================================================
bool TrafficEntityModule::LoadData( BrnTrafficIO::OutputBuffer_Prepare* lpOutputBuffer )
{
    TrafficReceiverQueue& lrReceiverQueue = mReceiverQueue;             // console +0x314
    CgsResource::ResourcePtr<TrafficData>& lrData = mpData;             // console +0x71840

    switch ( meResourceStage )                                          // console +0x2F8
    {
    case E_RESOURCE_LOAD_BASEDATA_NOT_STARTED:
        // Case 0: LoadTrafficLanes @0x827468C0 (event id 1, pool 5) inside a write lock, then
        // straight into case 1. The console does not return here.
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

        // The console reads the record's TYPE word directly out of the queue buffer and takes
        // the payload at +8. Expressed through the queue's accessor, as
        // BrnTriggerQueryManager_Prepare.cpp:224 and BrnDirectorWorldMap.cpp:214 do for the
        // identical reply.
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

        // The seat. The console reads the handle at payload +0x20; the host handle is 16 bytes
        // where the console's is 8, so it is read by member.
        lrData = lpAcquire->mHandle;

        // ARGUMENT ORDER, measured at 0x82746C1C..0x82746C2C: r4 comes from
        // `lbz r4,0x18(TrafficData)` (the u8 COUNT) and r5 from `lwz r31,0x34(TrafficData)`
        // (the asset-array POINTER), so the count is first and the pointer second.
        //
        // This publishes the catalogue only. The bundle request comes from
        // TrafficCarStreamer::Update, pumped by UpdateStreaming @0x82748848.
        mStreamer.SetAssetList( mpData->muNumVehicleAssets, mpData->mpaVehicleAssets );

        lrReceiverQueue.Clear();
    }
    // fall through -- the console does NOT return here; LABEL_3 runs straight into LABEL_10.

    case E_RESOURCE_LOAD_VEHICLELISTAQUIRE:
        // LABEL_10 (case 2). The Clear is inside the lock bracket and after the post: the
        // console drops whatever the lane reply left behind before it waits for the list.
        meResourceStage = E_RESOURCE_LOAD_VEHICLELISTAQUIRE;

        lpOutputBuffer->LockForWrite();
        lpOutputBuffer->GetResourceRequestInterface()->GetVehicleList(
            &lrReceiverQueue, KI_VEHICLE_LIST_REQUEST );
        lrReceiverQueue.Clear();
        lpOutputBuffer->UnlockForWrite();
        // fall through -- LABEL_11.

    case E_RESOURCE_LOAD_WFVEHICLELISTAQUIRE:
    {
        // LABEL_11 (case 3). The console reads mpVehicleList from the payload's +0x20 word,
        // the same slot stage 1 hands to CreateFromHandle, i.e. GameDataAssetEvent::mHandle.
        // For a LIST resource it keeps the raw resource memory rather than a ResourcePtr, as
        // GameStateModule::ReceiveListResource does for its own vehicle/wheel lists. The host
        // ResourceHandle is 16 bytes where the console's is 8, so every literal offset past it
        // shifts; read by member.
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

        mpVehicleList = static_cast<const BrnResource::VehicleList*>(
            lpListReply->mHandle.mpResourceMemory );
    }
    // fall through -- LABEL_20. NOTE the console jumps to case 8, NOT case 4.

    case E_RESOURCE_LOAD_ATTRIBS:
        // LABEL_20 (case 8). The event id is the RUNNING REQUEST COUNTER, numerically the
        // vehicle-type index for this loop; stage 7's assert spells it as a type index, and the
        // counter is what the two WFLOAD stages compare their reply count against. Kept as the
        // counter.
        meResourceStage = E_RESOURCE_LOAD_ATTRIBS;

        RequestVehicleAssetsForEveryType( lpOutputBuffer, &lrReceiverQueue,
                                          mpData.operator->(), &miResourceRequestCount,
                                          BrnResource::E_ASSETSET_ATTRIBS );
        // fall through -- LABEL_27.

    case E_RESOURCE_WFLOAD_ATTRIBS:
        // LABEL_27 (case 9). The drain binds nothing: attrib replies are counted and discarded,
        // asserting each record's KIND word is 50 (.cpp:1214). Only the PHYSICS drain below
        // binds a handle.
        meResourceStage = E_RESOURCE_WFLOAD_ATTRIBS;

        if ( lrReceiverQueue.GetCount() < miResourceRequestCount )
        {
            return false;
        }

        DrainAssetReplies( &lrReceiverQueue );
        // fall through -- LABEL_33.

    case E_RESOURCE_LOAD_PHYSICS:
        // LABEL_33 (case 6): LABEL_20 with meType 1 (PHYSICS) instead of 4, and with
        // BaseEventReceiverQueue::Clear inlined rather than called. Same semantics, so the
        // out-of-line Clear is used.
        meResourceStage = E_RESOURCE_LOAD_PHYSICS;

        RequestVehicleAssetsForEveryType( lpOutputBuffer, &lrReceiverQueue,
                                          mpData.operator->(), &miResourceRequestCount,
                                          BrnResource::E_ASSETSET_PHYSICS );
        // fall through -- LABEL_41.

    case E_RESOURCE_WFLOAD_PHYSICS:
    {
        // LABEL_41 (case 7): per reply, assert the kind and the type index, seat the spec
        // handle, then read the spec back out and Prepare the runtime with it.
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

            // The seat. Everything downstream that reads a traffic vehicle's deformation model
            // gets it from here: VehicleTypeRuntime::Prepare's bbox/axle extraction, and
            // through it Prepare stage 3's BoxVolume and Vehicle::InitialiseAsStatic's axle
            // offsets.
            maTrafficVehiclePhysicsSpecs[liVehicleType] =
                lpReply->GetVehiclePhysicsObjectHandle();

            // Console order at 0x82747210..0x82747238: bind the handle first, then read the
            // spec pointer back out of the member just seated, then Prepare. Stage 7 binds and
            // Prepares on the same reply, one vehicle type at a time; Prepare stage 3's
            // AddDynamicVolume loop runs later, by which time every type has been through here.
            const BrnPhysics::Deformation::StreamedDeformationSpec* lpPhysicsSpec =
                maTrafficVehiclePhysicsSpecs[liVehicleType].operator->();

            maVehicleTypeRuntime[liVehicleType].Prepare(
                lpPhysicsSpec,
                FindVehicleTypeAttribKey_EXPENSIVE( static_cast<u32>( liVehicleType ) ) );

            const CgsModule::Event* lpNext = 0;
            liKind = lrReceiverQueue.GetNextEvent( lpEvent, &lpNext, &liSize );
            lpEvent = lpNext;
        }
    }
    // fall through -- LABEL_49.

    case E_RESOURCE_ACQUIRE_COUNT:
        // LABEL_49 / case 12.
        meResourceStage = E_RESOURCE_ACQUIRE_COUNT;
        return true;

    default:
        // The console's own default arm. Stages 4/5 and 10/11 have no case label in the
        // shipped jump table either, so landing here really is a weird state.
        CGS_ASSERT( false, "TrafficEntityModule::LoadData in a weird state" );  // baked line 1320
        return false;
    }
}

// ============================================================================
// BrnTraffic::TrafficEntityModule::Prepare  @ 0x8274A578  (252 insns)  PARTIAL
// DWARF :1079 `virtual bool Prepare(OutputBuffer_Prepare*)`.
//
// A resumable six-stage ladder on mePrepareStage (EPrepareStage, header :486), driven once per
// frame by WorldModule::Prepare stage eWorldPrepareTrafficEntityModule
// (BrnWorldModule.cpp:966-999), which drains this module's request pipe on every FALSE. Stages
// 0, 1, 2, 3 and 5 are real, along with the fall-through chaining and the default assert; only
// stage 4's debug-UI half is gated.
//
// DECLARED NON-VIRTUAL, deliberately. The base `CgsModule::ModuleSingleBuffered::Prepare()`
// takes no argument (CgsModuleSingleBuffered.h:42), so this one-argument overload hides rather
// than overrides it, and marking it virtual would add a vtable slot the console does not have.
// FLAG for the wave that reconstructs the module vtable. The base sub-object is at offset 0:
// 0x8274A600 `mr r3,r26` passes `this` UNADJUSTED into the base Prepare at 0x8274A604.
//
// Stage 3 RETURNS FALSE after doing its work, so the console itself always spends one extra
// frame there.
// ============================================================================
bool TrafficEntityModule::Prepare( BrnTrafficIO::OutputBuffer_Prepare* lpOutputBuffer )
{
    TrafficReceiverQueue& lrReceiverQueue = mReceiverQueue;             // console +0x314

    switch ( mePrepareStage )                                           // console +0x2F0
    {
    case E_PREPARESTAGE_START:
    {
        // 0x8274A5D4. [FLAG PC bring-up] safety net, NOT IN THE X360 BINARY: the console binds
        // mReceiverQueue inside TrafficEntityModule::Construct @0x82740220, which does not run
        // while its WorldLinkStubs.cpp gate is live, leaving mpBuffer null when the GameData
        // reply arrives. This logs and self-heals instead of copying through null.
        // DELETE WHEN: that Construct gate is retired and a boot proves the queue is bound
        // before Prepare stage 0 runs.
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

        // The console's two stage-0 zero stores:
        //   0x8274A5F4  std  r31, 0(this + 0x713A0)   ; one 64-bit zero -> the bit array
        //   0x8274A5F8  stbx r31, this + 0x71B32      ; one byte zero   -> mbInReplay
        //
        // +0x713A0 == 463776 is maTrafficPhysicsInfoListBits (:686), pinned by UpdateSerialiser
        // @0x8272DA80's `SetPhysicsData(serialiser, this + 463776, this + 360976)` where
        // 463776 - 360976 == 102800 == 25 * sizeof(TrafficPhysicsInfo), so
        // maTrafficPhysicsInfoList (:685) spans 360976..463776 and the bit array starts there.
        // BitArray<25> is one 64-bit field, which is why one `std` clears it.
        maTrafficPhysicsInfoListBits.Prepare();

        // +0x71B32 == 465714 is mbInReplay (:757). mFuzzyBehaviours (:753) ends at 465712,
        // where the u16 muUpdateCount (:755) lives: UpdateDecisionFrame @0x8274E508 increments
        // *(this + 465712) and tests it >= 0x64, and UpdateRaceCarHulls @0x82721460 compares it
        // against HullChangeInfo::muUpdateFrame. A u16 there puts :757/:758 at 465714/465715.
        mbInReplay = false;

        mePrepareStage = E_PREPARESTAGE_MANAGER;
    }
    // fall through -- 0x8274A600 is the case-1 entry AND the case-0 fall-through target.

    case E_PREPARESTAGE_MANAGER:
        // 0x8274A604 `bl CgsModule::ModuleSingleBuffered::Prepare` with `this` unadjusted, then
        // the console's `if (!result) goto LABEL_22` early-out, which is what makes the ladder
        // resumable.
        //
        // HARD PRECONDITION: TrafficEntityModule::Construct @0x82740220 must have run. Its last
        // store is `mbIsNewModule = true` (0x82741758), and ModuleSingleBuffered::Prepare
        // @0x8286E7A0 branches on that byte: non-zero skips the old-style DataStructure ladder
        // and returns 1; zero calls CreateInputDataStructure through the vtable. This module
        // overrides neither Create*DataStructure, so with mbIsNewModule false the base
        // placeholder returns null and Prepare returns FALSE every frame, hanging the boot at
        // WorldModule::Prepare's traffic stage. Construct only runs once its WorldLinkStubs.cpp
        // gate (~:729) is retired, so retire that gate in the same build as this call.
        if ( !CgsModule::ModuleSingleBuffered::Prepare() )
        {
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
        // 0x8274A63C..0x8274A880. Publishes one shared collision volume per vehicle TYPE into
        // the scene manager, keyed KU_HACK_BASE_VOLUME_ID + type. Without it the scene has no
        // volume for a traffic car to instance, so a spawned car cannot take part in the fine
        // query.
        //
        // The loop iterates VEHICLE TYPES, not traffic-light instances: r27 walks
        // &maVehicleTypeRuntime[0].mBBoxHalfSize with `addi r27, r27, 0x80` (stride 128 ==
        // sizeof(VehicleTypeRuntime)) @0x8274A86C, bounded by muNumVehicleTypes read through
        // TrafficData::operator-> @0x8274A870.
        //
        // The three tripwires are over the POST-SUBTRACTION half-extents, one per lane, baked
        // .cpp lines 946/947/948. Each is a `vcmpgefp.` against zero and is non-gating: the
        // console reloads the vector and carries on.
        //
        // The console passes the WHOLE 64-bit id in r4 (`addi r11, r28, 0x24 ; clrldi r11, r11,
        // 32`), so the `AddDynamicVolume(VolumeId, const void*, VolumeTypeFlags)` overload is
        // the right one. The high dword is zero here, but the narrow EntityId overload would
        // bind the wrong mangled name for a key that is a VolumeId.
        lpOutputBuffer->LockForWrite();

        for ( u32 luVehicleType = 0;
              luVehicleType < mpData->muNumVehicleTypes;
              luVehicleType++ )
        {
            const CgsSceneManager::VolumeId lVolumeId(
                static_cast<u64>( KU_HACK_BASE_VOLUME_ID + luVehicleType ) );

            // The console inlines the accessor; read through it here.
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
            // volume pointer (`li r5, 0x80` @0x822B1534), so 128 is the floor. This tree's
            // other producer, BrnActiveRaceCar_wQ5_01.cpp, sizes its block to exactly that;
            // 4096 here follows Feb-2007's `Vector3 laResourceBuffer[256]` and is a superset.
            u8 laVolumeStorage[KU_VOLUME_SCRATCH_BYTES];

            rw::Resource lVolumeResource = {};                      // 5x stw 0
            lVolumeResource.m_baseResources[0] = laVolumeStorage;   // stw r11, var_10B0(r1)

            rw::collision::BoxVolume* lpVolume = rw::collision::BoxVolume::Initialize(
                lVolumeResource, lfBoxHalfX, lfBoxHalfY, lfBoxHalfZ );

            // `stfs f0, 0x50(r30)` == Volume::mfRadius, Feb-2007's `SetRadius(fatness)`.
            // BoxVolume::BoxVolume @0x82BAA0F0 has just written zero there.
            lpVolume->mfRadius = lfFatness;

            lpOutputBuffer->GetSceneInputInterface()->AddDynamicVolume(
                lVolumeId, lpVolume, KU8_TRAFFIC_VEHICLE_VOLUME_TYPE_FLAG );
        }

        lpOutputBuffer->UnlockForWrite();

        mePrepareStage = E_PREPARESTAGE_DEBUG;
        return false;                                    // console: `result = 0` at 0x8274A880
    }

    case E_PREPARESTAGE_DEBUG:
    {
        // 0x8274A8A0..0x8274A918, PARTIAL. The second half is plain member seeding and is
        // landed below; only the debug-allocator half is gated, on a missing wire.
        //
        // Offsets for the gated half, from the asm: Register's argument is the pointer LOADED
        // FROM this + 0x727B0, not that address (0x8274A8A4); the AllocateMemoryResource(2560,
        // 16, 0) result lands at this + 0x7286C (0x8274A8C8), and the neighbouring store into
        // this + 0x72870 is a separate zero.
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

        // The console's second half, 0x8274A8E0..0x8274A918: this + 0x79388 is
        // maStoredAITrafficData[0].meRaceCarIndex and +0x7938C its miNumTrafficIDs, stride 136
        // == sizeof(StoredAITrafficData), bound 8 == E_ACTIVE_RACE_CAR_INDEX_COUNT. Feb-2007
        // spells the same loop at BrnTrafficEntityModule.cpp:432-437; the console's
        // `operator++(v55)` is the enum's increment operator, de-inlined to a plain loop.
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

        // No meState latch here, deliberately. An earlier bring-up set E_STATE_RUNNING at this
        // point, which skipped the whole E_STATE_STARTING_UP ladder, and
        // E_STARTINGUPSTATE_POPULATING is the only place in the module that ever creates a
        // parked car. The module now walks its own state machine: Reset @0x8272CDA0 and
        // PostPhysicsUpdate @0x8274E6D0's STARTING_UP arm are bodied in
        // BrnTrafficEntityModule_wT1_01.cpp, and PreSceneUpdate @0x8274A968 owns the
        // WAITING_FOR_PLAYER -> POPULATING transition in BrnTrafficEntityModule_wT1_02.cpp.
        return true;

    default:
        CGS_ASSERT( false, "0" );                        // baked line 1003
        return false;
    }
}

}
