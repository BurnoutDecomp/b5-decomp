#ifndef GAMESOURCE_RESOURCE_SHAREDIO_BRNGAMEDATAEVENTS_H
#define GAMESOURCE_RESOURCE_SHAREDIO_BRNGAMEDATAEVENTS_H

#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"
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
    // ========================================================================
    // EGeneratedPoolIds -- the generated resource-pool id table.
    //
    // ⚠ CANONICAL HOME IS **GameSource/Resource/ps3mem.h** (the generated
    // memory-map header), per the DecFIGS DWARF, which spells the whole enum at
    // ps3mem.h:77. That header has NO reconstruction in b5-decomp yet, and the
    // tree consequently carries the pool ids as bare X360 literals with a comment
    // at every site (e.g. BrnRaceCarComponentStreamers.cpp's `KI_POOL_SOUND = 6`,
    // BrnWorldGraphicsStreamer.cpp's `Construct( 3, ... )`, and the ~12 "pool 5"
    // comments across the GameData/GameState/Director TUs). Landing it here --
    // the SharedIO header every one of those request/response consumers already
    // includes -- gives the ids a single named home without inventing a new file.
    // GREPPED before adding: no other declaration of EGeneratedPoolIds or of any
    // E_POOL_* enumerator exists anywhere under b5-decomp/src, so this is NOT a
    // fork. ⛔ MOVE IT to GameSource/Resource/ps3mem.h (and delete it here) the
    // day that generated header is reconstructed.
    //
    // The values are DWARF-verbatim, and THREE of them are independently
    // corroborated on the X360 ARTIST build, which is what makes the whole table
    // trustworthy for that build rather than PS3-only:
    //   E_POOL_OW_GRAPHICS == 3  -- WorldGraphicsStreamer::Construct @0x827CA388
    //                              passes liPoolId 3 to InternalBaseStreamer::Construct.
    //   E_POOL_GAMEDATA    == 5  -- TrafficEntityModule::LoadData @0x82746A88 stage 0
    //                              calls RequestInterface<4096>::LoadTrafficLanes(
    //                              &mReceiverQueue, 1, 5); GetVehicleList @0x82746928
    //                              bakes miPoolId 5; the boot log's
    //                              "LoadBundle 'B5Traffic.bndl' -> pool 5" agrees.
    //   E_POOL_SOUND       == 6  -- RaceCarAudioStreamer::Construct @0x822ECA68
    //                              passes liPoolId 6.
    //
    // E_POOL_TRAFFIC (15) itself is DWARF-only: its single X360 consumer,
    // TrafficCarStreamer::Construct @0x827539A0, is an EXPORT HOLE (no
    // .ida-exports JSON), so the value cannot be read back off the ARTIST build.
    // The leaked Feb-2007 BrnTrafficCarStreamer.cpp names the SYMBOL at that call
    // (`BaseClass::Construct( BrnResource::E_POOL_TRAFFIC, ... )`), and the DWARF
    // supplies its value; the three corroborations above are the evidence that the
    // DWARF's numbering is the shipped numbering. FLAGGED as such at the call site.
    // ========================================================================
    enum EGeneratedPoolIds
    {
        E_POOL_FONTS               = 0,
        E_POOL_PHYSICS             = 1,
        E_POOL_OW_PHYSICS          = 2,
        E_POOL_OW_GRAPHICS         = 3,
        E_POOL_CARS                = 4,
        E_POOL_GAMEDATA            = 5,
        E_POOL_SOUND               = 6,
        E_POOL_ATTRIBSYS           = 7,
        E_POOL_APT_PERSIST         = 8,
        E_POOL_APT_STREAM          = 9,
        E_POOL_GLOBALTEXTURES      = 10,
        E_POOL_GUI_DATA            = 11,
        E_POOL_GUI_SATNAV          = 12,
        E_POOL_VFX                 = 13,
        E_POOL_ICE                 = 14,
        E_POOL_TRAFFIC             = 15,
        E_POOL_ENVIRONMENT         = 16,
        E_POOL_PC0_PHYSICS         = 17,
        E_POOL_PC1_PHYSICS         = 18,
        E_POOL_PC2_PHYSICS         = 19,
        E_POOL_PC3_PHYSICS         = 20,
        E_POOL_PC4_PHYSICS         = 21,
        E_POOL_PC5_PHYSICS         = 22,
        E_POOL_PC6_PHYSICS         = 23,
        E_POOL_PC7_PHYSICS         = 24,
        E_POOL_CARSHARED           = 25,
        E_POOL_FREEBURN_CHALLENGES = 26,
        E_GENERATED_POOL_COUNT     = 27,
    };

namespace GameDataIO
{
    // DWARF: BrnGameDataEvents.h:51
    // Receiver-side reply id for the surface-list fetch (the X360 assert text
    // "liEventId == BrnResource::GameDataIO::EVENT_GET_SURFACE_LIST" in
    // WorldEntityModule::PrepareSurfaceList @0x822F9B70; observed value 66).
    static const s32 EVENT_GET_SURFACE_LIST = 66;

    // ADDITIVE GROW 2026-08-21 (traffic wave T1, cluster C5).
    // Receiver-side reply id for the traffic-lane (TrafficData) fetch.
    // X360-ATTESTED, not inferred: TrafficEntityModule::LoadData @0x82746A88
    // posts the request in stage E_RESOURCE_LOAD_LANES --
    //   RequestInterface<4096>::LoadTrafficLanes( &mReceiverQueue, 1, 5 )
    // -- and then, in the reply stage, reads the receiver record's leading
    // event-kind word and guards it against the literal 55 before accepting the
    // resource:
    //   v13 = mReceiverQueue base;  if ( *v13 == 55 ) { ... }  else assert
    //   "TrafficEntityModule::LoadData has received a resource with an ID that
    //    wasn't requested"  (BrnTrafficEntityModule.cpp:1051)
    // The very next test in the same arm reads the PAYLOAD's own
    // GameDataEvent::miEventId (record + 8) and checks it against 1 -- the
    // console spells that one out in its assert text, "lpAcquire->GetEventId()
    // == KI_DATA_ACQUIRE_REQUEST" (:1055), matching the 1 passed as liEventId
    // above. So 55 is the queue-record kind (this constant) and 1 is the
    // per-request id; the two are different words. The same record shape is what
    // EVENT_GET_SURFACE_LIST (66) names for the surface-list fetch, and the
    // vehicle-graphics / vehicle-physics replies drained later in LoadData are
    // guarded against 50 in exactly the same position (:1214 / :1291).
    static const s32 EVENT_GET_TRAFFIC_LANES = 55;

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
        // The loaded asset's resource handle (WorldGraphicsStreamer::OnLoadComplete
        // @0x827BE5C8 binds the instance-list slot from event+0x20).
        CgsResource::ResourceHandle mHandle;   // +0x20

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

    // DWARF: BrnGameDataEvents.h:637 -- the "asset finished unloading" response. No own
    // data members (the DWARF lists only a Construct(int,int,CgsID,EAssetSet)); it carries
    // the unloaded asset's id in the inherited GameDataAssetEvent::mId, read back via
    // GetGameDataId() (e.g. RaceCarAudioStreamer::OnUnloadComplete @0x822A55C0
    // `ld r10,0x10(event)`). ADDITIVE GROW of this header's event family; the BrnBaseStreamer
    // forward-decl stays compatible. GROW with its Construct when the GameDataEvents TU lands.
    struct UnloadGameDataResponse : public GameDataAssetEvent {};

    // ADDITIVE GROW (VEQ typed-AddEvent family): three more typed GameData request events the
    // per-N RequestInterface builders push via VariableEventQueue<N,16>::AddEvent<EventT>.
    //
    //   * UnloadGameDataEvent : public GameDataAssetEvent (DWARF BrnGameDataEvents.h) -- the
    //     asset-unload request; empty derivative like LoadGameDataEvent. The X360 typed
    //     AddEvent<UnloadGameDataEvent> (VariableEventQueue<2048,16> @ 0x827D10D0) forwards
    //     liSize == 32 == sizeof(GameDataAssetEvent), so it is an asset event.
    //
    //   * SwapInCollisionWorldRequest / SwapOutCollisionWorldRequest : public GameDataEvent
    //     (DWARF BrnGameDataEvents.h:431 "struct SwapInCollisionWorldRequest : public
    //     GameDataEvent"; only a Construct(BaseEventReceiverQueue*, s32), no own members). The
    //     X360 typed AddEvent<Swap*CollisionWorldRequest> (VariableEventQueue<2048,16> @
    //     0x82512E10 / 0x82512D58) forwards liSize == 8 == sizeof(GameDataEvent) (miEventId@0 +
    //     mpReceiverQueue@4 on the X360), confirming the plain GameDataEvent base (NOT the
    //     0x20 asset event). Empty derivatives; the swap builder sets the base fields by name.
    struct UnloadGameDataEvent : public GameDataAssetEvent {};
    struct SwapInCollisionWorldRequest : public GameDataEvent {};
    struct SwapOutCollisionWorldRequest : public GameDataEvent {};

    // ========================================================================
    // ADDITIVE GROW 2026-08-21 (traffic wave T1, cluster C5) -- the two
    // "here is your loaded asset" responses the traffic module consumes.
    //
    // ⭐ READ THIS BEFORE ADDING A HANDLE MEMBER TO EITHER OF THEM.
    // The DecFIGS DWARF declares each of these with its OWN private handle:
    //     GetTrafficVehicleGraphicsResponse : GameDataAssetEvent
    //         ResourceHandle mVehicleGraphicsObjectHandle;   (BrnGameDataEvents.h:232)
    //     GetVehiclePhysicsResponse : GameDataAssetEvent
    //         ResourceHandle mVehiclePhysicsObjectHandle;    (BrnGameDataEvents.h:443)
    // and the DWARF's GameDataAssetEvent has NO handle at all (:123-:126 are
    // miPoolId / mId / meType / mbFailFlag and nothing else). The committed
    // GameDataAssetEvent above nevertheless carries `mHandle` at +0x20 -- an
    // earlier wave hoisted it into the base because that is the byte the console
    // consumers read (WorldGraphicsStreamer::OnLoadComplete @0x827BE5C8 binds
    // from event+0x20). +0x20 is EXACTLY where the derived class's own handle
    // sits, since the base ends at 0x20; the hoist is a naming difference, not a
    // layout one.
    //
    // Re-declaring the handle HERE would therefore create a SECOND handle at
    // +0x28 and every reader would take the wrong one. These two responses are
    // consequently modelled as empty derivatives whose DWARF-named accessors
    // return the inherited `mHandle`, which keeps both the byte layout and the
    // source vocabulary correct. (The right long-term fix is to push mHandle
    // back down out of GameDataAssetEvent into the response family; that is a
    // cross-TU reorder touching BrnBaseStreamer.cpp, BrnRaceCarComponentStreamers
    // .cpp and BrnWorldGraphicsStreamer.cpp, so it is PARKED for a coordinated
    // pass rather than done from this cluster.)
    //
    // X360 attestation for the graphics one: TrafficCarStreamer::OnLoadComplete
    // @0x82758270 does
    //     CgsResource::BaseResourcePtr::CreateFromHandle( &maGraphicsStubs[i],
    //                                                     lpEvent + 32 );
    // i.e. it binds the stub ResourcePtr from the handle at event+0x20 -- the
    // handle this accessor names. sizeof is 0x28 either way.
    // ========================================================================

    // DWARF: BrnGameDataEvents.h:220. The reply to a traffic-vehicle GRAPHICS
    // load (asset set E_ASSETSET_GRAPHICS, pool E_POOL_TRAFFIC); the handle names
    // the loaded BrnTraffic::GraphicsStub (resource type 65557 / 0x10015).
    struct GetTrafficVehicleGraphicsResponse : public GameDataAssetEvent
    {
        // DWARF :229 -- `const ResourceHandle& GetTrafficVehicleGraphicsObjectHandle() const`.
        const CgsResource::ResourceHandle& GetTrafficVehicleGraphicsObjectHandle() const
        {
            return mHandle;
        }
    };

    // DWARF: BrnGameDataEvents.h:425. The reply to a vehicle PHYSICS load (asset
    // set E_ASSETSET_PHYSICS); the handle names the loaded
    // BrnPhysics::Deformation model data the VehicleTypeRuntime is Prepare'd from
    // (TrafficEntityModule::LoadData @0x82746A88 stage E_RESOURCE_LOAD_PHYSICS
    // binds maTrafficVehiclePhysicsSpecs[type] from the record's +0x20 handle and
    // then calls VehicleTypeRuntime::Prepare @0x82761B10).
    struct GetVehiclePhysicsResponse : public GameDataAssetEvent
    {
        // DWARF :437 / :440.
        const CgsResource::ResourceHandle& GetVehiclePhysicsObjectHandle() const
        {
            return mHandle;
        }
        void SetVehiclePhysicsObjectHandle( const CgsResource::ResourceHandle& lrHandle )
        {
            mHandle = lrHandle;
        }
    };
}
}

#endif // GAMESOURCE_RESOURCE_SHAREDIO_BRNGAMEDATAEVENTS_H
