#ifndef BRN_WORLD_BRN_BASE_STREAMER_H
#define BRN_WORLD_BRN_BASE_STREAMER_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsID.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Resource/SharedIO/BrnAssetIds.h"          // BrnResource::EAssetSet
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h" // BrnResource::GameDataIO::RequestInterface<N>

// The load/unload completion events appear only as pointer parameters of the
// pure-virtual notification hooks below, so a forward declaration is sufficient.
// GameDataAssetEvent is fully homed in BrnGameDataEvents.h; UnloadGameDataResponse
// is not yet reconstructed (FLAG: pointer-only use here; resolve when its home lands).
namespace BrnResource { namespace GameDataIO {
    struct GameDataAssetEvent;
    struct UnloadGameDataResponse;
} }

// =============================================================================
// BrnWorld streamer base family
//   GameSource/World/BrnBaseStreamer.h (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The streamer base is the generic asset-streaming state machine that drives a
// target/current entry list against the game-data request/response queues. The
// leaf class WorldGraphicsStreamer derives BaseStreamer<32> and forwards its
// per-index readiness query (IsListLoaded) straight to the base
// InternalBaseStreamer::IsEntryReady().
//
// This TU bodies ONE ledger function:
//   BrnWorld::WorldGraphicsStreamer::IsListLoaded  @ 0x827B0D00
//     -> tail-calls BrnWorld::InternalBaseStreamer::IsEntryReady(liIndex)
//        (asm @ 0x827B0D00: `b BrnWorld__InternalBaseStreamer__IsEntryReady`).
//
// IsListLoaded's DWARF home is BrnWorldGraphicsStreamer.h (WorldGraphicsStreamer
// derives BaseStreamer<32>); since that home is not yet reconstructed and the
// body is a one-line forward into the base owned HERE, a minimal flagged
// WorldGraphicsStreamer shell is provided at the end of this header so the
// forwarder can be bodied without fabricating the rest of that class. See FLAG
// below -- when BrnWorldGraphicsStreamer.h lands, IsListLoaded moves there.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): IsEntryReady touches only
// mpCurrentEntryList[] and miStreamListLength by NAME. No absolute offsets are
// asserted across the pointer members (X360 4-byte vs host 8-byte). The heavy
// request/receiver queue members are modelled by NAME and SEQUENCE so the class
// shape is coherent, but their exact byte sizes are not load-bearing here.
// =============================================================================

namespace BrnWorld
{

// BrnBaseStreamer.h:45 (DWARF). One desired ("target") entry in a stream list.
class StreamerTargetEntry
{
public:
    CgsID GetId()   const { return mResourceId; }
    bool  IsSafe()  const { return mbSafe; }

private:
    CgsID mResourceId;   // BrnBaseStreamer.h:59
    bool  mbSafe;        // BrnBaseStreamer.h:60
    u64   muUserId;      // BrnBaseStreamer.h:61

    friend class InternalBaseStreamer;
};

// BrnBaseStreamer.h:75 (DWARF). One live ("current") entry, carrying its
// load/unload status.
class StreamerCurrentEntry
{
public:
    // BrnBaseStreamer.h:78 (DWARF).
    enum EEntryStatus
    {
        E_STATUS_EMPTY     = 0,
        E_STATUS_LOADING   = 1,
        E_STATUS_UNLOADING = 2,
        E_STATUS_READY     = 3,
    };

    CgsID        GetId()     const { return mResourceId; }
    bool         IsSafe()    const { return mbSafe; }
    EEntryStatus GetStatus() const { return meStatus; }

private:
    CgsID        mResourceId;  // BrnBaseStreamer.h:97
    bool         mbSafe;       // BrnBaseStreamer.h:98
    EEntryStatus meStatus;     // BrnBaseStreamer.h:99
    u64          muUserId;     // BrnBaseStreamer.h:100

    friend class InternalBaseStreamer;
};

// FLAG: minimal stand-in for the un-homed CgsModule::EventReceiverQueue<N,M>
// generic. The committed CgsModule::BaseEventReceiverQueue is the non-templated
// base, but the templated EventReceiverQueue<N,M> is not yet reconstructed in
// its own home. Modelled by NAME so InternalBaseStreamer's member shape is
// coherent; IsEntryReady does not touch it. Replace with the committed generic
// when it lands -- do NOT treat this size as X360 fact.
namespace detail
{
    template <s32 N, s32 M = 16>
    struct EventReceiverQueuePlaceholder
    {
        // DWARF (BrnBaseStreamer.h:239) shows EventReceiverQueue<2048,16>; only
        // its presence (not byte size) is load-bearing for this TU.
        u8 mStorage[N + 16];
    };
}

// BrnBaseStreamer.h:114 (DWARF). The non-templated streamer engine. Polymorphic:
// the Query/On* hooks are pure-virtual and supplied by each concrete streamer
// (e.g. WorldGraphicsStreamer).
class InternalBaseStreamer
{
public:
    // BrnBaseStreamer.h:117 (DWARF).
    enum EStreamMode
    {
        E_STREAMMODE_IDLE      = 0,
        E_STREAMMODE_LOADING   = 1,
        E_STREAMMODE_UNLOADING = 2,
        E_STREAMMODE_COUNT     = 3,
    };

    // BrnBaseStreamer.h:125 (DWARF).
    enum ELoadStreamStage
    {
        E_LOADSTREAM_REQUEST = 0,
        E_LOADSTREAM_WAIT    = 1,
        E_LOADSTREAM_DONE    = 2,
    };

    // BrnBaseStreamer.h:132 (DWARF).
    enum EUnloadStreamStage
    {
        E_UNLOADSTREAM_REQUEST = 0,
        E_UNLOADSTREAM_WAIT    = 1,
        E_UNLOADSTREAM_DONE    = 2,
    };

    // BrnBaseStreamer.h:139 (DWARF).
    enum EAssetStatus
    {
        E_AS_UNKNOWN            = 0,
        E_AS_NOT_IN_LIST        = 1,
        E_AS_PENDING_LOAD       = 2,
        E_AS_LOADING            = 3,
        E_AS_READY              = 4,
        E_AS_PENDING_ABORT_LOAD = 5,
        E_AS_UNLOADING          = 6,
        E_AS_PENDING_UNLOAD     = 7,
    };

    // BrnBaseStreamer.h:238/239 (DWARF): RequestInterface<2048> / EventReceiverQueue<2048,16>.
    typedef BrnResource::GameDataIO::RequestInterface<2048>      StreamerGDRequestInterface;
    typedef detail::EventReceiverQueuePlaceholder<2048, 16>      StreamerGDReceiverQueue;

    virtual ~InternalBaseStreamer() {}

    void Update();
    void ClearTargetList();

    const StreamerGDRequestInterface* GetGameDataRequestInterface() const { return &mGDRequestInterface; }

    bool AddEntry( CgsID lId, bool lbIsSafe, u64 luUserId = 0 );
    bool RemoveEntry( CgsID lId );
    bool RemoveEntry( CgsID lId, u64 luUserId );

    s32  GetEntryListLength() { return miStreamListLength; }

    EAssetStatus DebugGetAssetStatus( CgsID lId ) const;

    u64  GetUserId( s32 liIndex ) const { return mpCurrentEntryList[liIndex].muUserId; }

    // BrnBaseStreamer.h:390 (DWARF / ground-truth source). X360 @0x822A1300.
    // Defined out-of-line in BrnBaseStreamer.cpp (this TU's real .cpp home).
    bool IsAssetLoaded( CgsID lId, u64 luUserData ) const;

    bool IsStreamComplete() const;

protected:
    void Construct(
        StreamerTargetEntry*  lpTargetEntryList,
        StreamerTargetEntry*  lpTargetBuffer,
        StreamerCurrentEntry* lpCurrentEntryList,
        s32                   liStreamListLength,
        s32                   liPoolId,
        BrnResource::EAssetSet leAssetSet,
        bool                  lbAllowFailure );

    virtual s32  QueryLoad( const StreamerTargetEntry* lpPotentialList, s32 liPotentialListLength ) = 0;
    virtual s32  QueryUnload( const StreamerTargetEntry* lpPotentialList, s32 liPotentialListLength ) = 0;
    virtual void OnLoadBegin( s32 liListIndex )   = 0;
    virtual void OnUnloadBegin( s32 liListIndex ) = 0;
    virtual void OnLoadComplete( const BrnResource::GameDataIO::GameDataAssetEvent* lpEvent, s32 liListIndex )      = 0;
    virtual void OnUnloadComplete( const BrnResource::GameDataIO::UnloadGameDataResponse* lpEvent, s32 liListIndex ) = 0;
    virtual void OnLoadFail( const BrnResource::GameDataIO::GameDataAssetEvent* lpEvent, s32 liListIndex ) {}

    // BrnBaseStreamer.h:390/391 (DWARF / ground-truth source). The exact target
    // of WorldGraphicsStreamer::IsListLoaded (asm @ 0x827B0D00 tail-call). X360
    // @0x827B0258. Defined out-of-line in BrnBaseStreamer.cpp (this TU's .cpp home).
    bool IsEntryReady( s32 liIndex ) const;

private:
    StreamerTargetEntry*       mpTargetEntryList;     // BrnBaseStreamer.h:233
    StreamerTargetEntry*       mpPotentialList;       // BrnBaseStreamer.h:234
    StreamerCurrentEntry*      mpCurrentEntryList;    // BrnBaseStreamer.h:235
    s32                        miStreamListLength;    // BrnBaseStreamer.h:236
    s32                        miPotentialListLength; // BrnBaseStreamer.h:237
    StreamerGDRequestInterface mGDRequestInterface;   // BrnBaseStreamer.h:238
    StreamerGDReceiverQueue    mGDReceiverQueue;      // BrnBaseStreamer.h:239
    EStreamMode                meCurrentStreamMode;   // BrnBaseStreamer.h:240
    ELoadStreamStage           meLoadStreamStage;     // BrnBaseStreamer.h:241
    EUnloadStreamStage         meUnloadStreamStage;   // BrnBaseStreamer.h:242
    EStreamMode                mePreviousStreamMode;  // BrnBaseStreamer.h:243
    s32                        miCurrentEntryIndex;   // BrnBaseStreamer.h:244
    s32                        miPoolId;              // BrnBaseStreamer.h:245
    BrnResource::EAssetSet     meAssetSet;            // BrnBaseStreamer.h:246
    bool                       mbAllowFailure;        // BrnBaseStreamer.h:247

    // Private helpers (declared for home completeness; not bodied by this TU).
    void ClearCurrentList();
    s32  CalculatePotentialLoadList();
    s32  CalculatePotentialUnloadList();
    s32  FindInTargetList( CgsID lId ) const;
    s32  FindInTargetList( CgsID lId, u64 luUserId ) const;
    s32  FindInCurrentList( CgsID lId ) const;
    s32  FindInCurrentList( CgsID lId, u64 luUserId ) const;
    EStreamMode UpdateIdle();
    s32  AttemptLoad();
    s32  AttemptUnload();
    bool UpdateLoading();
    bool UpdateUnloading();
};

// BrnBaseStreamer.h:299 (DWARF / ground-truth source). The fixed-capacity
// streamer template that embeds its own entry storage.
template <int ListLength>
class BaseStreamer : public InternalBaseStreamer
{
protected:
    void Construct( s32 liPoolId, BrnResource::EAssetSet leAssetSet, bool lbAllowFailure )
    {
        InternalBaseStreamer::Construct(
            maStreamerTargetEntries,
            maStreamerTargetBuffer,
            maStreamerCurrentEntries,
            ListLength,
            liPoolId,
            leAssetSet,
            lbAllowFailure );
    }

private:
    StreamerTargetEntry  maStreamerTargetEntries[ListLength];  // BrnBaseStreamer.h:309
    StreamerTargetEntry  maStreamerTargetBuffer[ListLength];   // BrnBaseStreamer.h:310
    StreamerCurrentEntry maStreamerCurrentEntries[ListLength]; // BrnBaseStreamer.h:311
};

// -----------------------------------------------------------------------------
// FLAG: WorldGraphicsStreamer's true DWARF home is BrnWorldGraphicsStreamer.h
// (it derives BaseStreamer<32> and adds mpWorldEntityModule + a
// ResourcePtr<CgsGraphics::InstanceList>[32] table). That home is NOT yet
// reconstructed. Only IsListLoaded is in this TU's ledger, and its body is a
// one-line forward into the base owned here. A MINIMAL shell deriving
// BaseStreamer<32> is provided so IsListLoaded can be bodied faithfully without
// fabricating the rest of the class. When BrnWorldGraphicsStreamer.h lands,
// this shell is replaced and IsListLoaded moves into it (the body is identical).
// -----------------------------------------------------------------------------
class WorldGraphicsStreamer : public BaseStreamer<32>
{
public:
    // IsListLoaded @0x827B0D00 (DWARF: `bool IsListLoaded(int32_t) const`).
    // @ 0x827B0D00 -- tail-calls InternalBaseStreamer::IsEntryReady(liIndex).
    bool IsListLoaded( s32 liIndex ) const
    {
        return IsEntryReady( liIndex );
    }
};

} // namespace BrnWorld

#endif // BRN_WORLD_BRN_BASE_STREAMER_H
