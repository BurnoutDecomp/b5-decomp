#ifndef GAMESOURCE_RESOURCE_SHAREDIO_BRNGAMEDATAREQUESTQUEUE_H
#define GAMESOURCE_RESOURCE_SHAREDIO_BRNGAMEDATAREQUESTQUEUE_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                          // CgsID
#include "GameSource/Resource/SharedIO/BrnAssetIds.h"                // BrnResource::EAssetSet
#include "GameShared/GameClasses/System/Resource/CgsResourceRequestQueue.h"  // ResourceRequestQueue<N>

// ============================================================================
// GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h
//
// BrnResource::GameDataIO::RequestQueue<N> and RequestInterface<N>.
// DWARF home: GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h.
//
// Inheritance chain (DecFIGS DWARF, confirmed against the X360 ARTIST build):
//   RequestInterface<N>      { RequestQueue<N> mRequestQueue; }   (only member, @0x00)
//   RequestQueue<N>          : public CgsResource::ResourceIO::ResourceRequestQueue<N>
//   ResourceRequestQueue<N>  : public CgsModule::VariableEventQueue<N, 16>
//
// Because mRequestQueue is at offset 0 and the whole base chain is single-inheritance
// at offset 0, a RequestInterface<N>* is layout-compatible with its embedded
// VariableEventQueue<N,16>*. The X360 RequestInterface<N> request-builder methods
// exploit this: each builds a typed GameData event on the stack and calls
// queue.AddEvent<EventT>(&event, eventTypeId) directly on the underlying queue.
//
//   sizeof(RequestInterface<N>) == sizeof(VariableEventQueue<N,16>) == N + 16
//   (the 16-byte VEQ header: bool mbIsConstructed + 3*s32, see CgsVariableEventQueue.h).
//   This matches the committed in-tree evidence: BrnGameStateModuleIO.h reserves
//   0x4024-0x3414 == 3088 == 3072+16 bytes for its RequestInterface<3072>.
//
// MINIMAL SLICE: only the RequestInterface<N> request-builder methods this TU bodies
// (RequestInterface<4096>: GetSurfaceList / GetVehicleList / LoadBundle /
// LoadTrafficLanes / LoadWorldCollision) are declared here. The DWARF lists ~50 more
// RequestInterface methods (CreatePool/LoadGameData/AcquireResource/...); those are
// DEFERRED to their own per-instance TUs. Each method is a thin event-builder, so the
// generic template body covers every N once declared. GROW this header additively when
// sibling methods/instances land -- do NOT fork it.
//
// EVENT TYPE IDS (X360 immediate operands; the `liType` passed to AddEvent):
//   LoadGameDataEvent     family -> 26 (0x1A)   (GetSurfaceList/LoadWorldCollision/LoadTrafficLanes)
//   GetVehicleListRequest        -> 49 (0x31)   (GetVehicleList)
//   LoadBundleRequest            ->  2          (LoadBundle, via the plain 4-arg AddEvent)
// ============================================================================

namespace CgsModule { class BaseEventReceiverQueue; }   // referenced by pointer only

namespace BrnResource
{
namespace GameDataIO
{
    // DWARF: BrnGameDataRequestQueue.h (RequestQueue<N>). Empty derived shape -- no own
    // data members; size/layout is entirely the ResourceRequestQueue<N> base (which is
    // VariableEventQueue<N,16>). The ~50 typed request methods are DEFERRED.
    template <s32 N>
    struct RequestQueue : public CgsResource::ResourceIO::ResourceRequestQueue<N>
    {
    };

    // DWARF: BrnGameDataRequestQueue.h:136 (RequestInterface<512> shown; same shape for
    // every N). Holds exactly one RequestQueue<N> at offset 0.
    template <s32 N>
    struct RequestInterface
    {
        RequestQueue<N> mRequestQueue;   // @0x00 (only member)

        // ---- request-builder methods owned/bodied by the RequestInterface<4096> TU ----
        // (declared on the generic so any N inherits them; only <4096> is instantiated
        //  by this TU's .cpp).

        // Push a LoadGameDataEvent (type 26) requesting the world surface list.
        // X360 0x822F1EF0. meType == E_ASSETSET_ATTRIBS, mId == baked surface-list id.
        bool GetSurfaceList(CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
                            s32 liEventId, s32 liPoolId);

        // Push a GetVehicleListRequest (type 49) requesting the vehicle list.
        // X360 0x82746928. miPoolId == 5, meType == E_ASSETSET_DATA, mId == baked id.
        bool GetVehicleList(CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
                            s32 liEventId);

        // Push a LoadBundleRequest (type 2) to load a named bundle.
        // X360 0x8229B500. Builds a CgsResource::Events::LoadBundleRequest on the stack.
        bool LoadBundle(CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
                        s32 liEventId, s32 liPoolId, const char* lpcFileName,
                        bool lbUseHDCache);

        // Push a LoadGameDataEvent (type 26) requesting the traffic-lane data.
        // X360 0x827468C0. meType == E_ASSETSET_DATA, mId == baked traffic-lane id.
        bool LoadTrafficLanes(CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
                              s32 liEventId, s32 liPoolId);

        // Push a LoadGameDataEvent (type 26) requesting the world collision.
        // X360 0x822F1E88. meType == E_ASSETSET_PHYSICS, mId == baked world-collision id.
        bool LoadWorldCollision(CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
                                s32 liEventId, s32 liPoolId);
    };
}
}

#endif // GAMESOURCE_RESOURCE_SHAREDIO_BRNGAMEDATAREQUESTQUEUE_H
