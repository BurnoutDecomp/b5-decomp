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

namespace CgsResource { struct ResourceHandle; }

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

        // ---- additive sibling request-builders (bodied by the per-N instance TUs) ----
        // All share the generic event-builder shape: stack-build a typed GameData event
        // (members set BY NAME in the X360 store order), then queue.AddEvent<EventT>(...).
        // The generic body lives once in BrnGameDataRequestQueueImpl.h; each N's .cpp
        // explicitly instantiates RequestInterface<N>.

        // Push a GetGameDataEvent (type 49) requesting the AI lane data.
        // X360 0x822563C0 (<512>). meType == E_ASSETSET_DATA, mId == baked ai-lane id.
        bool GetAILanes(CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
                        s32 liEventId, s32 liPoolId);

        // Push a LoadGameDataEvent (type 26) requesting the AI lane data -- the Load* twin of
        // GetAILanes (SAME baked id 0x71117D7F4337FFFF, meType DATA) but the LOAD family event/type.
        // X360 0x82396980 (<3072>). Caller: BrnProgression::ProgressionManager::LoadAIData.
        bool LoadAILanes(CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
                         s32 liEventId, s32 liPoolId);

        // Push a GetICEListRequest (type 49) requesting the in-game-camera-editor list.
        // X360 0x82256358 (<512>). miPoolId == 5, meType == E_ASSETSET_DATA, mId baked.
        bool GetICEList(CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
                        s32 liEventId);

        // Push a LoadGameDataEvent (type 26) requesting the PVS (potentially-visible-set)
        // data. X360 0x822FD250 (<512>). meType == E_ASSETSET_DATA, mId == baked pvs id.
        bool LoadPVS(CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
                     s32 liEventId, s32 liPoolId);

        // Push a GetGameDataEvent (type 49) requesting the prop instances for a numbered
        // prop-instance group. X360 0x82303E08 (<1024>). The id is built at runtime by
        // SPrintf("PRP_INST_%d", liInstanceIndex) + CgsIDCompress; meType == PHYSICS.
        bool GetPropInstances(CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
                              s32 liEventId, s32 liInstanceIndex, s32 liPoolId);

        // Push a LoadGameDataEvent (type 26) requesting the prop physics data.
        // X360 0x82303DA0 (<1024>). meType == E_ASSETSET_PHYSICS, mId == baked id.
        bool LoadPropPhysics(CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
                             s32 liEventId, s32 liPoolId);

        // Push a GetWheelListRequest (type 49) requesting the wheel list.
        // X360 0x822FD380 (<8192>). miPoolId == 5, meType == E_ASSETSET_DATA, mId baked.
        bool GetWheelList(CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
                          s32 liEventId);

        // Push a GetFreeburnChallengeListRequest (type 49) requesting the freeburn
        // challenge list. X360 0x8250BBE0 (<32768>). miPoolId == 5, meType == DATA, mId baked.
        bool GetFreeburnChallengeList(CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
                                      s32 liEventId);

        // Push a CgsResource AcquireResourceRequest (type 4) acquiring a named,
        // already-loaded resource from a pool. X360: inlined into
        // BrnGui::ColourCalibrationScreen::Update @0x8246AA28 (<32768>): record
        // { mpUser, miEventId, miPoolId, mResourceId = HashString(name) | poolId<<48 },
        // AddEvent type 4, X360 record 24 bytes. FLAG: the arg split (the caller passes
        // the literal resource name) and the id's packed top nibble == the pool id are
        // from that single attested site.
        bool AcquireResource(CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
                             s32 liEventId, s32 liPoolId, const char* lpcResourceName);

        // Acquire the "TRK_CLIL%d" zone-collision resource list into a caller-owned
        // handle array. DWARF BrnGameDataRequestQueue.h:234
        // `AcquireZoneCollision(BaseEventReceiverQueue*, int, ResourceHandle*, int)`;
        // X360 inline expansion in WorldEntityModule::PrepareZoneCollision @0x82302C38
        // (SPrintf "TRK_CLIL%d" -> ID::HashString -> AcquireResourceListRequest, pool 2,
        // AddEvent type 5, event id == the zone number).
        bool AcquireZoneCollision(CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
                                  s32 liZoneNumber,
                                  CgsResource::ResourceHandle* lpaHandles, s32 liMaxHandles);

        // Swap the collision world in/out (DWARF :240/:237). X360: typed
        // AddEvent<SwapIn/SwapOutCollisionWorldRequest> types 68/67
        // (WorldEntityModule::ValidateCollision @0x82306A48 / InvalidateCollision
        // @0x822F9D78; the request is the plain GameDataEvent pair {id, queue}).
        bool SwapInCollisionWorld(CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
                                  s32 liEventId);
        bool SwapOutCollisionWorld(CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
                                   s32 liEventId);

        // Merge another request interface's queued requests into this one (the X360
        // VariableEventQueue<N,16>::Append<M,16> template call sites:
        // WorldEntityModule::PostPhysicsUpdate @0x823080F0 appends the streamer's <2048>
        // interface; BridgePVSToOutput_Prepare @0x822F9EC8 appends the PVS <512> one).
        template <s32 M>
        void Append(const RequestInterface<M>& lrOther)
        {
            mRequestQueue.Append(lrOther.mRequestQueue);
        }
    };
}
}

#endif // GAMESOURCE_RESOURCE_SHAREDIO_BRNGAMEDATAREQUESTQUEUE_H
