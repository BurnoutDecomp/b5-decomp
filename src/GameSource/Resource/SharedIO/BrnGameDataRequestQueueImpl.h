#ifndef GAMESOURCE_RESOURCE_SHAREDIO_BRNGAMEDATAREQUESTQUEUEIMPL_H
#define GAMESOURCE_RESOURCE_SHAREDIO_BRNGAMEDATAREQUESTQUEUEIMPL_H

#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"

#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"          // LoadGameDataEvent, GetGameDataEvent, Get*Request
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"  // CgsResource::Events::LoadBundleRequest
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"     // CgsModule::BaseEventReceiverQueue
#include "GameShared/GameClasses/Core/CgsID.h"                       // CgsIDCompress, KI_CGSID_STRING_LEN
#include "GameShared/GameClasses/Core/CgsStringUtils.h"              // CgsCore::SPrintf

// ============================================================================
// BrnResource::GameDataIO::RequestInterface<N> -- SHARED generic request-builder
// method bodies (one copy for ALL N). Each per-N instance TU includes this header and
// explicitly instantiates RequestInterface<N>; the generic <s32 N> bodies are defined
// here ONCE so no N forks its own copy.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Each method builds a typed GameData event
// on the stack (members set BY NAME in the X360 store order) and submits it to the
// underlying VariableEventQueue<N,16> via the typed AddEvent<EventT> overload (or, for
// LoadBundle, the plain 4-arg AddEvent). The per-N instances differ ONLY in N (the queue
// buffer size); the event-builder logic is identical store-for-store across N (verified
// against the per-instance X360 addresses listed at each method).
//
// The baked CgsID asset ids below are the exact 64-bit immediates the X360 loads
// (lis/ori/insrdi pair of 32-bit halves into one 64-bit literal).
//
// HOST-vs-X360 POINTER WIDTH (FLAG): the X360 typed events are 32 bytes (32-bit
// mpReceiverQueue + 32-bit-packed layout). On the 64-bit host the event grows (8-byte
// pointer + 8-aligned CgsID), so sizeof(EventT) > 32 here. This is correct: the queue is
// host-side and AddEvent uses sizeof(EventT), so producer and consumer agree. We
// deliberately do NOT hardcode the X360 size 32 (members are set by NAME).
// ============================================================================

namespace BrnResource
{
namespace GameDataIO
{
    // -------- GetSurfaceList @ 0x822F1EF0 (<4096>) --------
    template <s32 N>
    bool RequestInterface<N>::GetSurfaceList(
            CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
            s32 liEventId, s32 liPoolId)
    {
        LoadGameDataEvent lEvent;
        lEvent.miEventId       = liEventId;                 // +0x00
        lEvent.mpReceiverQueue = lpReceiverQueue;           // +0x04
        lEvent.miPoolId        = liPoolId;                  // +0x08
        lEvent.mId             = 0xB95B4FE36709E838ULL;     // +0x10
        lEvent.meType          = E_ASSETSET_ATTRIBS;        // +0x18 (4)
        lEvent.mbFailFlag      = false;                     // +0x1C

        return mRequestQueue.AddEvent(&lEvent, 26);
    }

    // -------- GetVehicleList @ 0x82746928 (<4096>), 0x822562F0 (<512>),
    //          0x822FD318 (<8192>), 0x82512CF0 (<32768>) --------
    template <s32 N>
    bool RequestInterface<N>::GetVehicleList(
            CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
            s32 liEventId)
    {
        GetVehicleListRequest lEvent;
        lEvent.miEventId       = liEventId;                 // +0x00
        lEvent.mpReceiverQueue = lpReceiverQueue;           // +0x04
        lEvent.miPoolId        = 5;                         // +0x08
        lEvent.mId             = 0xC98B447411F97E38ULL;     // +0x10
        lEvent.meType          = E_ASSETSET_DATA;           // +0x18 (3)
        lEvent.mbFailFlag      = false;                     // +0x1C

        return mRequestQueue.AddEvent(&lEvent, 49);
    }

    // -------- LoadBundle @ 0x8229B500 (<4096>), 0x822FD2B8 (<8192>), 0x823CE558 (<32768>) --------
    template <s32 N>
    bool RequestInterface<N>::LoadBundle(
            CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
            s32 liEventId, s32 liPoolId, const char* lpcFileName,
            bool lbUseHDCache)
    {
        CgsResource::Events::LoadBundleRequest lEvent;
        lEvent.mpUser              = lpReceiverQueue;        // +0x00
        lEvent.miEventId           = liEventId;             // +0x04
        lEvent.SetFileName(lpcFileName);                    // macFileName @ +0x0C
        lEvent.miPoolId            = liPoolId;              // +0x8C
        lEvent.mbUseHDCache        = lbUseHDCache;          // +0x91
        lEvent.mbLiveUpdateReplace = false;                 // +0x08
        lEvent.mbAllowFailiure     = false;                 // +0x90

        return mRequestQueue.AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lEvent), 2,
            (s32)sizeof(CgsResource::Events::LoadBundleRequest));
    }

    // -------- UnloadBundle (ADDITIVE 2026-08-16, envstream) --------
    // No X360 export for any <N> instance (the exporter names every function it has; UnloadBundle
    // is absent for all of them -- report grep). It is attested only by its CALL SITES, which give
    // the full signature: EnvironmentManager::StreamOut @0x827D2EB0 passes
    // (this, &receiverQueue, eventId, poolId 16, fileName) in r3..r7. The event it must build is
    // CgsResource::Events::UnloadBundleRequest, which the committed CgsResourceIOEvents.h banner
    // pins as a bare BundleLoaderEvent (the X360 BaseEventQueue<UnloadBundleRequest>::AddEvent
    // @0x828E9F50 memcpys at stride 0x90 == 144 == exactly the base) -- i.e. LoadBundleRequest
    // MINUS mbAllowFailiure/mbUseHDCache. Queue tag 3, the same tag GuiResourceModule::UnloadBundle
    // @0x8285E8C8 posts and the same tag ResourceModule::ProcessResourceRequests routes to the
    // bundle loader's unload intake (CgsResourceModule.cpp:169).
    template <s32 N>
    bool RequestInterface<N>::UnloadBundle(
            CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
            s32 liEventId, s32 liPoolId, const char* lpcFileName)
    {
        CgsResource::Events::UnloadBundleRequest lEvent;
        lEvent.mpUser              = lpReceiverQueue;        // +0x00
        lEvent.miEventId           = liEventId;             // +0x04
        lEvent.SetFileName(lpcFileName);                    // macFileName @ +0x0C
        lEvent.miPoolId            = liPoolId;              // +0x8C
        lEvent.mbLiveUpdateReplace = false;                 // +0x08

        return mRequestQueue.AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lEvent), 3,
            (s32)sizeof(CgsResource::Events::UnloadBundleRequest));
    }

    // -------- LoadTrafficLanes @ 0x827468C0 (<4096>), 0x82256288 (<512>) --------
    template <s32 N>
    bool RequestInterface<N>::LoadTrafficLanes(
            CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
            s32 liEventId, s32 liPoolId)
    {
        LoadGameDataEvent lEvent;
        lEvent.miEventId       = liEventId;                 // +0x00
        lEvent.mpReceiverQueue = lpReceiverQueue;           // +0x04
        lEvent.miPoolId        = liPoolId;                  // +0x08
        lEvent.mId             = 0x7111805C01676561ULL;     // +0x10
        lEvent.meType          = E_ASSETSET_DATA;           // +0x18 (3)
        lEvent.mbFailFlag      = false;                     // +0x1C

        return mRequestQueue.AddEvent(&lEvent, 26);
    }

    // -------- LoadWorldCollision @ 0x822F1E88 (<4096>) --------
    template <s32 N>
    bool RequestInterface<N>::LoadWorldCollision(
            CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
            s32 liEventId, s32 liPoolId)
    {
        LoadGameDataEvent lEvent;
        lEvent.miEventId       = liEventId;                 // +0x00
        lEvent.mpReceiverQueue = lpReceiverQueue;           // +0x04
        lEvent.miPoolId        = liPoolId;                  // +0x08
        lEvent.mId             = 0xBEB7A3F7E9788FFFULL;     // +0x10
        lEvent.meType          = E_ASSETSET_PHYSICS;        // +0x18 (1)
        lEvent.mbFailFlag      = false;                     // +0x1C

        return mRequestQueue.AddEvent(&lEvent, 26);
    }

    // -------- LoadVehicle --------
    // Declaration and the evidence chain: BrnGameDataRequestQueue.h. The two attesting
    // sites are inlined expansions in TrafficEntityModule::LoadData @0x82746A88: stage 8
    // (E_RESOURCE_LOAD_ATTRIBS, miPoolId 1, meType 4) and stage 6 (E_RESOURCE_LOAD_PHYSICS,
    // miPoolId 1, meType 1). MakeVehicleId lives inside this builder, so its own
    // "Vehicle name too long" assert fires from here.
    template <s32 N>
    bool RequestInterface<N>::LoadVehicle(
            CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
            s32 liEventId, s32 liPoolId, const char* lpcVehicleName,
            BrnResource::EAssetSet leAssetSet)
    {
        LoadGameDataEvent lEvent;
        lEvent.miEventId       = liEventId;                          // +0x00
        lEvent.mpReceiverQueue = lpReceiverQueue;                    // +0x04
        lEvent.miPoolId        = liPoolId;                           // +0x08
        lEvent.mId             = MakeVehicleId(lpcVehicleName);      // +0x10
        lEvent.meType          = leAssetSet;                         // +0x18
        lEvent.mbFailFlag      = false;                              // +0x1C

        return mRequestQueue.AddEvent(&lEvent, 26);
    }

    // -------- GetAILanes @ 0x822563C0 (<512>) --------
    // ASM store order: v5[0]@var_30 = liEventId (r5/a3), v5[1]@var_2C = lpReceiverQueue
    // (r4/a2), v5[2]@var_28 = liPoolId (r6/a4); v6 = id, v7 = meType(3), v8 = 0; type 49.
    template <s32 N>
    bool RequestInterface<N>::GetAILanes(
            CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
            s32 liEventId, s32 liPoolId)
    {
        GetGameDataEvent lEvent;
        lEvent.miEventId       = liEventId;                 // +0x00
        lEvent.mpReceiverQueue = lpReceiverQueue;           // +0x04
        lEvent.miPoolId        = liPoolId;                  // +0x08
        lEvent.mId             = 0x71117D7F4337FFFFULL;     // +0x10
        lEvent.meType          = E_ASSETSET_DATA;           // +0x18 (3)
        lEvent.mbFailFlag      = false;                     // +0x1C

        return mRequestQueue.AddEvent(&lEvent, 49);
    }

    // -------- LoadAILanes @ 0x82396980 (<3072>) --------
    // ASM store order: v5[0]@var_30 = liEventId (r5/a3), v5[1]@var_2C = lpReceiverQueue
    // (r4/a2), v5[2]@var_28 = liPoolId (r6/a4); v6 = id, v7 = meType(3, DATA), v8 = 0;
    // type 0x1A == 26; event LoadGameDataEvent. Load* twin of GetAILanes: SAME baked id
    // but LoadGameDataEvent + AddEvent type 26 (same shape as LoadTrafficLanes/LoadPVS).
    // DWARF sig: bool LoadAILanes(BaseEventReceiverQueue*, int, int). Caller:
    // BrnProgression::ProgressionManager::LoadAIData.
    template <s32 N>
    bool RequestInterface<N>::LoadAILanes(
            CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
            s32 liEventId, s32 liPoolId)
    {
        LoadGameDataEvent lEvent;
        lEvent.miEventId       = liEventId;                 // +0x00
        lEvent.mpReceiverQueue = lpReceiverQueue;           // +0x04
        lEvent.miPoolId        = liPoolId;                  // +0x08
        lEvent.mId             = 0x71117D7F4337FFFFULL;     // +0x10
        lEvent.meType          = E_ASSETSET_DATA;           // +0x18 (3)
        lEvent.mbFailFlag      = false;                     // +0x1C

        return mRequestQueue.AddEvent(&lEvent, 26);
    }

    // -------- GetICEList @ 0x82256358 (<512>) --------
    // ASM: v5[0] = liEventId (a3), v5[1] = lpReceiverQueue (a2), v5[2]@var_28 = 5 (literal
    // poolId); id, meType(3), 0; type 49; event GetICEListRequest.
    template <s32 N>
    bool RequestInterface<N>::GetICEList(
            CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
            s32 liEventId)
    {
        GetICEListRequest lEvent;
        lEvent.miEventId       = liEventId;                 // +0x00
        lEvent.mpReceiverQueue = lpReceiverQueue;           // +0x04
        lEvent.miPoolId        = 5;                         // +0x08
        lEvent.mId             = 0x7DDFC0102EE70838ULL;     // +0x10
        lEvent.meType          = E_ASSETSET_DATA;           // +0x18 (3)
        lEvent.mbFailFlag      = false;                     // +0x1C

        return mRequestQueue.AddEvent(&lEvent, 49);
    }

    // -------- LoadPVS @ 0x822FD250 (<512>) --------
    // ASM: v5[0] = liEventId (a3), v5[1] = lpReceiverQueue (a2), v5[2] = liPoolId (a4);
    // id, meType(3), 0; type 0x1A == 26; event LoadGameDataEvent.
    template <s32 N>
    bool RequestInterface<N>::LoadPVS(
            CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
            s32 liEventId, s32 liPoolId)
    {
        LoadGameDataEvent lEvent;
        lEvent.miEventId       = liEventId;                 // +0x00
        lEvent.mpReceiverQueue = lpReceiverQueue;           // +0x04
        lEvent.miPoolId        = liPoolId;                  // +0x08
        lEvent.mId             = 0xBEB7A76555FC1FFFULL;     // +0x10
        lEvent.meType          = E_ASSETSET_DATA;           // +0x18 (3)
        lEvent.mbFailFlag      = false;                     // +0x1C

        return mRequestQueue.AddEvent(&lEvent, 26);
    }

    // -------- GetPropInstances @ 0x82303E08 (<1024>) --------
    // ASM register map (the Hex-Rays HIDWORD aliasing is a 64-bit-arg artifact; the
    // assembly is authoritative):
    //   r4/a2 -> v21[1] = mpReceiverQueue
    //   r5/a3 -> v21[0] = miEventId
    //   r6/a4 -> SPrintf "PRP_INST_%d" format argument (the numbered instance group)
    //   r7/a5 -> v21[2] = miPoolId
    // The id is built at runtime: SPrintf("PRP_INST_%d", liInstanceIndex) into a 13-byte
    // (KI_CGSID_STRING_LEN) buffer, then CgsIDCompress; v23 = meType(1, PHYSICS), v24 = 0;
    // type 49; event GetGameDataEvent.
    template <s32 N>
    bool RequestInterface<N>::GetPropInstances(
            CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
            s32 liEventId, s32 liInstanceIndex, s32 liPoolId)
    {
        char lacName[KI_CGSID_STRING_LEN];
        CgsCore::SPrintf(lacName, KI_CGSID_STRING_LEN, "PRP_INST_%d", liInstanceIndex);

        GetGameDataEvent lEvent;
        lEvent.miEventId       = liEventId;                 // +0x00
        lEvent.mpReceiverQueue = lpReceiverQueue;           // +0x04
        lEvent.miPoolId        = liPoolId;                  // +0x08
        lEvent.mId             = CgsIDCompress(lacName);    // +0x10
        lEvent.meType          = E_ASSETSET_PHYSICS;        // +0x18 (1)
        lEvent.mbFailFlag      = false;                     // +0x1C

        return mRequestQueue.AddEvent(&lEvent, 49);
    }

    // -------- LoadPropPhysics @ 0x82303DA0 (<1024>) --------
    // ASM: v5[0] = liEventId (a3), v5[1] = lpReceiverQueue (a2), v5[2] = liPoolId (a4);
    // id, meType(1, PHYSICS), 0; type 0x1A == 26; event LoadGameDataEvent.
    template <s32 N>
    bool RequestInterface<N>::LoadPropPhysics(
            CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
            s32 liEventId, s32 liPoolId)
    {
        LoadGameDataEvent lEvent;
        lEvent.miEventId       = liEventId;                 // +0x00
        lEvent.mpReceiverQueue = lpReceiverQueue;           // +0x04
        lEvent.miPoolId        = liPoolId;                  // +0x08
        lEvent.mId             = 0xA773D7113DF454BFULL;     // +0x10
        lEvent.meType          = E_ASSETSET_PHYSICS;        // +0x18 (1)
        lEvent.mbFailFlag      = false;                     // +0x1C

        return mRequestQueue.AddEvent(&lEvent, 26);
    }

    // -------- GetWheelList @ 0x822FD380 (<8192>) --------
    // ASM: v5[0] = liEventId (a3), v5[1] = lpReceiverQueue (a2), v5[2]@var_28 = 5 (literal
    // poolId); id, meType(3), 0; type 49; event GetWheelListRequest.
    template <s32 N>
    bool RequestInterface<N>::GetWheelList(
            CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
            s32 liEventId)
    {
        GetWheelListRequest lEvent;
        lEvent.miEventId       = liEventId;                 // +0x00
        lEvent.mpReceiverQueue = lpReceiverQueue;           // +0x04
        lEvent.miPoolId        = 5;                         // +0x08
        lEvent.mId             = 0xCF5D625701228838ULL;     // +0x10
        lEvent.meType          = E_ASSETSET_DATA;           // +0x18 (3)
        lEvent.mbFailFlag      = false;                     // +0x1C

        return mRequestQueue.AddEvent(&lEvent, 49);
    }

    // -------- GetFreeburnChallengeList @ 0x8250BBE0 (<32768>) --------
    // ASM: v5[0] = liEventId (a3), v5[1] = lpReceiverQueue (a2), v5[2]@var_28 = 5 (literal
    // poolId); id, meType(3), 0; type 49; event GetFreeburnChallengeListRequest.
    template <s32 N>
    bool RequestInterface<N>::GetFreeburnChallengeList(
            CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
            s32 liEventId)
    {
        GetFreeburnChallengeListRequest lEvent;
        lEvent.miEventId       = liEventId;                 // +0x00
        lEvent.mpReceiverQueue = lpReceiverQueue;           // +0x04
        lEvent.miPoolId        = 5;                         // +0x08
        lEvent.mId             = 0x5AF30CD3F949F838ULL;     // +0x10
        lEvent.meType          = E_ASSETSET_DATA;           // +0x18 (3)
        lEvent.mbFailFlag      = false;                     // +0x1C

        return mRequestQueue.AddEvent(&lEvent, 49);
    }

    // -------- AcquireResource (inlined into ColourCalibrationScreen::Update @0x8246AA28,
    //          <32768>) --------
    // ASM (the inlined record build): rec[0] = lpUser (&the caller's receiver queue),
    // rec[1] = liEventId (0x91449 at the site), rec[2] = liPoolId (9),
    // rec@+0x10 (qword) = CgsResource::ID::HashString(name) | (u64)poolId << 48;
    // AddEvent(&rec, /*type*/4, /*X360 size*/24). FLAG: the id's packed top nibble equals
    // the pool id at the one attested site. mbCheckRefCount lies outside the X360 24-byte
    // record; set false here for PC determinism.
    template <s32 N>
    bool RequestInterface<N>::AcquireResource(
            CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
            s32 liEventId, s32 liPoolId, const char* lpcResourceName)
    {
        CgsResource::Events::AcquireResourceRequest lEvent;
        lEvent.mpUser          = lpReceiverQueue;
        lEvent.miEventId       = liEventId;
        lEvent.miPoolId        = liPoolId;
        // ⚠️ THE `| (poolId << 48)` IS A HEX-RAYS FUSION ARTIFACT -- REMOVED 2026-07-29.
        // The decompiler fused the interleaved `li r10,<poolId>` (which is the SEPARATE
        // miPoolId store at +0x08) into the `std` of the hash. The id itself is a plain
        // zero-extended 32-bit CRC: CgsResource::ID::HashString @0x828D84A8 ends
        // `clrldi r3,r11,32`, and CgsResource::Pool::FindResource compares the whole 64-bit
        // value, so an id with the pool packed into bits 48+ matches NOTHING and the request
        // parks on the receiver queue for ever. This is the same artifact family already
        // called out at BrnWorldModule.cpp:191-194 and in the two GET handlers in
        // BrnGameDataModule.cpp, and the untagged form is what every attested producer
        // stores (the "TAGGED-ID CORRECTION" the world wave landed in 97992836).
        lEvent.mResourceId.SetHash(
            static_cast<u64>(static_cast<u32>(CgsResource::ID::HashString(
                reinterpret_cast<const u8*>(lpcResourceName)))));
        lEvent.mbCheckRefCount = false;

        return mRequestQueue.AddEvent(&lEvent, 4);
    }

    // -------- AcquireZoneCollision (DWARF :234; inlined into
    //          WorldEntityModule::PrepareZoneCollision @0x82302C38, <4096>) --------
    // ASM record: {mpUser@0 = queue, miEventId@4 = the ZONE number, miPoolId@8 = 2,
    // mListResourceId@0x10 = ID::HashString("TRK_CLIL%d" % zone), mpHandles@0x18,
    // miMaxHandles@0x1C = 1}; AddEvent type 5 (X360 size 32). The receiver reply is
    // the same record echoed with the handle array filled.
    template <s32 N>
    bool RequestInterface<N>::AcquireZoneCollision(
            CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
            s32 liZoneNumber,
            CgsResource::ResourceHandle* lpaHandles, s32 liMaxHandles)
    {
        char lacResourceName[256];
        CgsCore::SPrintf(lacResourceName, 256, "TRK_CLIL%d", liZoneNumber);

        CgsResource::Events::AcquireResourceListRequest lEvent;
        lEvent.mpUser       = lpReceiverQueue;
        lEvent.miEventId    = liZoneNumber;
        lEvent.miPoolId     = 2;
        lEvent.mListResourceId.SetHash(
            static_cast<u64>(static_cast<u32>(CgsResource::ID::HashString(
                reinterpret_cast<const u8*>(lacResourceName)))));
        lEvent.mpHandles    = lpaHandles;
        lEvent.miMaxHandles = liMaxHandles;

        return mRequestQueue.AddEvent(&lEvent, 5);
    }

    // -------- SwapInCollisionWorld / SwapOutCollisionWorld (DWARF :240/:237;
    //          inlined into WorldEntityModule::Validate/InvalidateCollision
    //          @0x82306A48/@0x822F9D78) --------
    // ASM record: the plain GameDataEvent pair {miEventId@0, mpReceiverQueue@4};
    // AddEvent types 68 (swap in) / 67 (swap out), X360 size 8.
    template <s32 N>
    bool RequestInterface<N>::SwapInCollisionWorld(
            CgsModule::BaseEventReceiverQueue* lpReceiverQueue, s32 liEventId)
    {
        SwapInCollisionWorldRequest lEvent;
        lEvent.miEventId       = liEventId;
        lEvent.mpReceiverQueue = lpReceiverQueue;

        return mRequestQueue.AddEvent(&lEvent, 68);
    }

    template <s32 N>
    bool RequestInterface<N>::SwapOutCollisionWorld(
            CgsModule::BaseEventReceiverQueue* lpReceiverQueue, s32 liEventId)
    {
        SwapOutCollisionWorldRequest lEvent;
        lEvent.miEventId       = liEventId;
        lEvent.mpReceiverQueue = lpReceiverQueue;

        return mRequestQueue.AddEvent(&lEvent, 67);
    }
}
}

#endif // GAMESOURCE_RESOURCE_SHAREDIO_BRNGAMEDATAREQUESTQUEUEIMPL_H
