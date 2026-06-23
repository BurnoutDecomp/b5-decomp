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
}
}

#endif // GAMESOURCE_RESOURCE_SHAREDIO_BRNGAMEDATAREQUESTQUEUEIMPL_H
