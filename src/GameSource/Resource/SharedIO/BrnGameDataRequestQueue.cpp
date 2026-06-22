#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"

#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"          // LoadGameDataEvent, GetVehicleListRequest, ...
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"  // CgsResource::Events::LoadBundleRequest
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"     // CgsModule::BaseEventReceiverQueue

// ============================================================================
// BrnResource::GameDataIO::RequestInterface<4096> -- request-builder methods.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Each method builds a typed event on the
// stack (members set BY NAME in the X360 store order) and submits it to the underlying
// VariableEventQueue<4096,16> via the typed AddEvent<EventT> overload (or, for
// LoadBundle, the plain 4-arg AddEvent).
//
//   GetSurfaceList     @ 0x822F1EF0 : LoadGameDataEvent,     type 26, meType=ATTRIBS
//   GetVehicleList     @ 0x82746928 : GetVehicleListRequest, type 49, meType=DATA, poolId 5
//   LoadBundle         @ 0x8229B500 : LoadBundleRequest,     type  2 (plain AddEvent)
//   LoadTrafficLanes   @ 0x827468C0 : LoadGameDataEvent,     type 26, meType=DATA
//   LoadWorldCollision @ 0x822F1E88 : LoadGameDataEvent,     type 26, meType=PHYSICS
//
// The baked CgsID asset ids below are the exact 64-bit immediates the X360 loads
// (lis/ori/insrdi pair of 32-bit halves into one 64-bit literal).
//
// HOST-vs-X360 POINTER WIDTH (FLAG): the X360 typed events are 32 bytes
// (32-bit mpReceiverQueue + 32-bit-packed layout). On the 64-bit host the event grows
// (8-byte pointer + 8-aligned CgsID), so sizeof(EventT) > 32 here. This is correct: the
// queue is host-side and AddEvent uses sizeof(EventT), so producer and consumer agree.
// We deliberately do NOT hardcode the X360 size 32 (members are set by NAME).
// ============================================================================

namespace BrnResource
{
namespace GameDataIO
{
    // @ 0x822F1EF0
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

    // @ 0x82746928
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

    // @ 0x8229B500
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

    // @ 0x827468C0
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

    // @ 0x822F1E88
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

    // Explicit instantiation of the <4096> request interface (this TU's instance).
    template struct RequestInterface<4096>;
}
}
