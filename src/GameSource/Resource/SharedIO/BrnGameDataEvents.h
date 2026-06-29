#ifndef GAMESOURCE_RESOURCE_SHAREDIO_BRNGAMEDATAEVENTS_H
#define GAMESOURCE_RESOURCE_SHAREDIO_BRNGAMEDATAEVENTS_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                  // CgsID (typedef u64)
#include "GameSource/Resource/SharedIO/BrnAssetIds.h"        // BrnResource::EAssetSet
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::Event base

// ============================================================================
// GameSource/Resource/SharedIO/BrnGameDataEvents.h
//
// BrnResource::GameDataIO event payloads -- the typed event structs the GameData
// request interface (BrnGameDataRequestQueue.h) pushes into its underlying
// VariableEventQueue<N,16>. Reconstructed from the DecFIGS DWARF
// (BrnGameDataEvents.h) plus the X360 ARTIST build:
//
//   * GameDataEvent : public CgsModule::Event
//       +0x00  s32                          miEventId
//       +0x04  CgsModule::BaseEventReceiverQueue*  mpReceiverQueue
//   * GameDataAssetEvent : public GameDataEvent
//       +0x08  s32                 miPoolId
//       +0x10  CgsID               mId          (u64, 8-aligned)
//       +0x18  BrnResource::EAssetSet  meType   (4-byte enum)
//       +0x1C  bool                mbFailFlag
//     sizeof(GameDataAssetEvent) == 0x20 (32) -- matches the X360 typed AddEvent size
//     argument (the templated AddEvent<LoadGameDataEvent>/<GetVehicleListRequest> both
//     forward liSize == 32; see 0x82296320 / 0x8273FB48).
//   * LoadGameDataEvent / GetGameDataEvent / GetVehicleListRequest : public
//     GameDataAssetEvent (no own data members -- DWARF; the request-builder methods
//     populate the base fields directly and pick the event TYPE id at the call site).
//
// FIELD-ORDER NOTE (X360 store order authoritative): in
// RequestInterface<N>::GetSurfaceList / LoadWorldCollision / LoadTrafficLanes the
// build sequence is miEventId@0 = eventId, mpReceiverQueue@4 = queue, miPoolId@8 =
// poolId, mId@0x10 = <baked CgsID>, meType@0x18 = <asset set>, mbFailFlag@0x1C = 0.
//
// MINIMAL SLICE: only the layout + inheritance the RequestInterface<4096> TU needs is
// modelled (the Set*/Get*/Construct accessors the DWARF lists for these events are
// DEFERRED to a dedicated GameDataEvents TU; this is a complete, correctly-sized type
// sufficient to build the events on the stack and memcpy them into the queue). GROW
// this header (add the accessor methods + the remaining response/request events) when
// that TU lands -- do NOT fork it.
// ============================================================================

namespace CgsModule { class BaseEventReceiverQueue; }   // referenced by pointer only

namespace BrnResource
{
namespace GameDataIO
{
    // DWARF: BrnGameDataEvents.h:51
    struct GameDataEvent : public CgsModule::Event
    {
        s32                                 miEventId;       // +0x00
        CgsModule::BaseEventReceiverQueue*  mpReceiverQueue; // +0x04
    };

    // DWARF: BrnGameDataEvents.h:86. sizeof == 0x20 (the X360 typed-AddEvent size arg).
    struct GameDataAssetEvent : public GameDataEvent
    {
        s32       miPoolId;    // +0x08
        CgsID     mId;         // +0x10 (u64, 8-aligned)
        EAssetSet meType;      // +0x18 (4-byte enum)
        bool      mbFailFlag;  // +0x1C

        // ADDITIVE GROW: the asset-id accessor the GameData event consumers read. The
        // X360 load-complete handlers read mId straight from +0x10 (e.g.
        // RaceCarBaseComponentStreamer::OnLoadComplete @0x822C0450 `ld r11,0x10(event)`),
        // which is this accessor inlined. Header-inline, zero-risk additive (no layout
        // change). GROW with the remaining Set*/Get* accessors when the dedicated
        // GameDataEvents TU lands -- do NOT fork.
        CgsID GetGameDataId() const { return mId; }
    };

    // DWARF: BrnGameDataEvents.h:138 -- no own members.
    struct LoadGameDataEvent : public GameDataAssetEvent {};

    // DWARF: BrnGameDataEvents.h:169 -- no own members.
    struct GetGameDataEvent : public GameDataAssetEvent {};

    // DWARF: BrnGameDataEvents.h:657 -- no own members (Construct(queue, eventId) only).
    struct GetVehicleListRequest : public GameDataAssetEvent {};

    // ADDITIVE GROW: sibling typed GameData request events used by the per-N
    // RequestInterface request-builders. Each is an empty GameDataAssetEvent derivative
    // (no own members; the builder populates the base fields by NAME and picks the
    // event TYPE id at the call site, exactly like GetVehicleListRequest above). The
    // X360 emits one typed AddEvent<EventT> per family; the distinct event TYPES are
    // what differentiate the request, not the struct shape.
    //   GetICEListRequest              (RequestInterface<512>::GetICEList            @ 0x82256358)
    //   GetWheelListRequest            (RequestInterface<8192>::GetWheelList         @ 0x822FD380)
    //   GetFreeburnChallengeListRequest(RequestInterface<32768>::GetFreeburnChallengeList @ 0x8250BBE0)
    struct GetICEListRequest : public GameDataAssetEvent {};
    struct GetWheelListRequest : public GameDataAssetEvent {};
    struct GetFreeburnChallengeListRequest : public GameDataAssetEvent {};
}
}

#endif // GAMESOURCE_RESOURCE_SHAREDIO_BRNGAMEDATAEVENTS_H
