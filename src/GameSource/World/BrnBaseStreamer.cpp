#include "GameSource/World/BrnBaseStreamer.h"
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"   // Load/UnloadGameDataEvent
#include "GameShared/GameClasses/Development/Log/CgsLog.h"    // [stream] multi-reply trace

// =============================================================================
// BrnWorld::InternalBaseStreamer  (GameSource/World/BrnBaseStreamer.h DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. This is the real .cpp home for the
// whole streamer engine: the target/current entry bookkeeping and the three-state
// (IDLE / LOADING / UNLOADING) machine that turns a target list into GameData
// load/unload requests and consumes the replies.
//
// StreamerCurrentEntry on X360 is 24 bytes: mResourceId(CgsID, u64 @+0) +
// mbSafe(bool @+8) + meStatus(EEntryStatus @+0xC) + muUserId(u64 @+0x10);
// StreamerTargetEntry is 24 bytes too: mResourceId@+0, mbSafe@+8, muUserId@+0x10.
// Every body below indexes the lists BY NAME (the X360 24-byte stride arithmetic
// in the asm is that member access, not a byte offset the host must reproduce).
//
// X360 ADDRESSES (all reconstructed in this pass unless noted):
//   Construct                    0x827C4A60      AddEntry                0x827C4B58
//   RemoveEntry                  0x827BDC40      ClearTargetList         0x827B0B50
//   ClearCurrentList             0x827B0B98      IsStreamComplete        0x827B0BE8
//   CalculatePotentialLoadList   0x827BDCB0      CalculatePotentialUnloadList 0x827BDD88
//   FindInTargetList             0x827BDEA8      FindInCurrentList       0x827BE0B8
//   AttemptLoad                  0x827BE2C8      AttemptUnload           0x827BE3D8
//   DebugGetAssetStatus          0x827BE4C8      UpdateIdle              0x827C4D38
//   UpdateLoading                0x827D41E8      UpdateUnloading         0x827D4670
//   Update                       0x827D5F50      IsAssetLoaded           0x822A1300
//   IsEntryReady                 0x827B0258
//
// The GameData request events the machine emits are the committed
// BrnResource::GameDataIO::LoadGameDataEvent (queue type 26) and
// UnloadGameDataEvent (queue type 39) -- the two ids GameDataModule::
// ProcessGameDataEvent @0x826744F0 already dispatches.
// =============================================================================

namespace BrnWorld
{

// ---------------------------------------------------------------------------
// Construct  @ 0x827C4A60
//
// Bind the three entry lists + the pending-entry ring storage, seed the sizing /
// policy fields, empty both lists, then bring up the GameData receiver queue and
// the request queue. Parameter set + store order are the console's.
// ---------------------------------------------------------------------------
void InternalBaseStreamer::Construct( StreamerTargetEntry*   lpTargetEntryList,
                                      StreamerTargetEntry*   lpTargetBuffer,
                                      StreamerCurrentEntry*  lpCurrentEntryList,
                                      s32*                   lpPendingEntries,
                                      s32                    liStreamListLength,
                                      s32                    liPoolId,
                                      bool                   lbSlotPoolSystem,
                                      BrnResource::EAssetSet leAssetSet,
                                      bool                   lbAllowFailure )
{
    mpTargetEntryList     = lpTargetEntryList;
    mpPotentialList       = lpTargetBuffer;
    mpCurrentEntryList    = lpCurrentEntryList;
    miStreamListLength    = liStreamListLength;
    miPotentialListLength = 0;

    miPoolId          = liPoolId;
    meAssetSet        = leAssetSet;
    mbAllowFailure    = lbAllowFailure;
    mbSlotPoolSystem  = lbSlotPoolSystem;
    mbLoadHasFailed   = false;

    meCurrentStreamMode  = E_STREAMMODE_IDLE;
    meLoadStreamStage    = E_LOADSTREAM_REQUEST;
    meUnloadStreamStage  = E_UNLOADSTREAM_REQUEST;
    mePreviousStreamMode = E_STREAMMODE_IDLE;

    mPendingEntryQueue.Construct( lpPendingEntries, liStreamListLength );

    // The X360 empties the TARGET list inline here (the same loop ClearTargetList
    // runs) and calls ClearCurrentList out-of-line for the current list.
    ClearTargetList();
    ClearCurrentList();

    mGDReceiverQueue.Construct();
    mGDRequestInterface.mRequestQueue.Construct();
    mGDRequestInterface.mRequestQueue.Clear();
}

// ---------------------------------------------------------------------------
// ClearTargetList  @ 0x827B0B50 -- empty every target slot (id 0 == "unused").
// ---------------------------------------------------------------------------
void InternalBaseStreamer::ClearTargetList()
{
    for ( s32 liEntry = 0; liEntry < miStreamListLength; liEntry++ )
    {
        mpTargetEntryList[liEntry].mResourceId = 0;
        mpTargetEntryList[liEntry].mbSafe      = false;
    }
}

// ---------------------------------------------------------------------------
// ClearCurrentList  @ 0x827B0B98
// ---------------------------------------------------------------------------
void InternalBaseStreamer::ClearCurrentList()
{
    for ( s32 liEntry = 0; liEntry < miStreamListLength; liEntry++ )
    {
        mpCurrentEntryList[liEntry].mbSafe      = false;
        mpCurrentEntryList[liEntry].mResourceId = 0;
        mpCurrentEntryList[liEntry].meStatus    = StreamerCurrentEntry::E_STATUS_EMPTY;
    }
}

// ---------------------------------------------------------------------------
// FindInTargetList / FindInCurrentList  @ 0x827BDEA8 / 0x827BE0B8
// Linear scans; the two-argument forms also match the user id (the X360 compares
// both 8-byte fields with cmpld). -1 when absent.
// ---------------------------------------------------------------------------
s32 InternalBaseStreamer::FindInTargetList( CgsID lId ) const
{
    for ( s32 liEntry = 0; liEntry < miStreamListLength; liEntry++ )
    {
        if ( mpTargetEntryList[liEntry].mResourceId == lId )
        {
            return liEntry;
        }
    }
    return -1;
}

s32 InternalBaseStreamer::FindInTargetList( CgsID lId, u64 luUserId ) const
{
    for ( s32 liEntry = 0; liEntry < miStreamListLength; liEntry++ )
    {
        if ( ( mpTargetEntryList[liEntry].mResourceId == lId ) &&
             ( mpTargetEntryList[liEntry].muUserId    == luUserId ) )
        {
            return liEntry;
        }
    }
    return -1;
}

s32 InternalBaseStreamer::FindInCurrentList( CgsID lId ) const
{
    for ( s32 liEntry = 0; liEntry < miStreamListLength; liEntry++ )
    {
        if ( mpCurrentEntryList[liEntry].mResourceId == lId )
        {
            return liEntry;
        }
    }
    return -1;
}

s32 InternalBaseStreamer::FindInCurrentList( CgsID lId, u64 luUserId ) const
{
    for ( s32 liEntry = 0; liEntry < miStreamListLength; liEntry++ )
    {
        if ( ( mpCurrentEntryList[liEntry].mResourceId == lId ) &&
             ( mpCurrentEntryList[liEntry].muUserId    == luUserId ) )
        {
            return liEntry;
        }
    }
    return -1;
}

// ---------------------------------------------------------------------------
// AddEntry  @ 0x827C4B58 -- put one wanted asset into the target list.
// ---------------------------------------------------------------------------
bool InternalBaseStreamer::AddEntry( CgsID lId, bool lbIsSafe, u64 luUserId )
{
    const s32 liExisting = FindInTargetList( lId, luUserId );
    if ( liExisting >= 0 )
    {
        CGS_ASSERT( mpTargetEntryList[liExisting].mbSafe == lbIsSafe,
                    "Entry already in list with a different safe flag" );
        return true;
    }

    s32 liFree = -1;
    for ( s32 liEntry = 0; liEntry < miStreamListLength; liEntry++ )
    {
        if ( mpTargetEntryList[liEntry].mResourceId == 0 )
        {
            liFree = liEntry;
            break;
        }
    }
    CGS_ASSERT( liFree >= 0, "Stream target list is full" );
    if ( liFree < 0 )
    {
        // The X360 stores through the -1 index after firing the assert; the store is
        // guarded here [documented X360 defect] -- the return value is unchanged.
        return true;
    }

    mpTargetEntryList[liFree].mbSafe      = lbIsSafe;
    mpTargetEntryList[liFree].mResourceId = lId;
    mpTargetEntryList[liFree].muUserId    = luUserId;
    return true;
}

// ---------------------------------------------------------------------------
// RemoveEntry  @ 0x827BDC40 (the 2-argument form; the 1-argument form is the same
// scan without the user-id match and was ICF-folded on the console).
// ---------------------------------------------------------------------------
bool InternalBaseStreamer::RemoveEntry( CgsID lId, u64 luUserId )
{
    const s32 liEntry = FindInTargetList( lId, luUserId );
    if ( liEntry >= 0 )
    {
        mpTargetEntryList[liEntry].mResourceId = 0;
    }
    return true;
}

bool InternalBaseStreamer::RemoveEntry( CgsID lId )
{
    const s32 liEntry = FindInTargetList( lId );
    if ( liEntry >= 0 )
    {
        mpTargetEntryList[liEntry].mResourceId = 0;
    }
    return true;
}

// ---------------------------------------------------------------------------
// CalculatePotentialLoadList  @ 0x827BDCB0 -- every wanted entry that is not yet
// in the current list. Fills mpPotentialList (the caller's "target buffer").
// ---------------------------------------------------------------------------
s32 InternalBaseStreamer::CalculatePotentialLoadList()
{
    miPotentialListLength = 0;

    for ( s32 liEntry = 0; liEntry < miStreamListLength; liEntry++ )
    {
        const StreamerTargetEntry& lrTarget = mpTargetEntryList[liEntry];
        if ( lrTarget.mResourceId == 0 )
        {
            continue;
        }
        if ( FindInCurrentList( lrTarget.mResourceId, lrTarget.muUserId ) < 0 )
        {
            mpPotentialList[miPotentialListLength] = lrTarget;
            miPotentialListLength++;
        }
    }
    return miPotentialListLength;
}

// ---------------------------------------------------------------------------
// CalculatePotentialUnloadList  @ 0x827BDD88 -- every live entry that is no longer
// wanted (and is not already unloading).
// ---------------------------------------------------------------------------
s32 InternalBaseStreamer::CalculatePotentialUnloadList()
{
    miPotentialListLength = 0;

    for ( s32 liEntry = 0; liEntry < miStreamListLength; liEntry++ )
    {
        const StreamerCurrentEntry& lrCurrent = mpCurrentEntryList[liEntry];
        if ( lrCurrent.mResourceId == 0 )
        {
            continue;
        }
        if ( lrCurrent.meStatus == StreamerCurrentEntry::E_STATUS_UNLOADING )
        {
            continue;
        }
        if ( FindInTargetList( lrCurrent.mResourceId, lrCurrent.muUserId ) < 0 )
        {
            mpPotentialList[miPotentialListLength].mbSafe      = lrCurrent.mbSafe;
            mpPotentialList[miPotentialListLength].muUserId    = lrCurrent.muUserId;
            mpPotentialList[miPotentialListLength].mResourceId = lrCurrent.mResourceId;
            miPotentialListLength++;
        }
    }
    return miPotentialListLength;
}

// ---------------------------------------------------------------------------
// AttemptLoad  @ 0x827BE2C8 -- pick one potential load (through the leaf's
// QueryLoad policy) into the first free current-list slot; -1 when nothing to do.
// ---------------------------------------------------------------------------
s32 InternalBaseStreamer::AttemptLoad()
{
    if ( CalculatePotentialLoadList() <= 0 )
    {
        return -1;
    }

    s32 liFree = -1;
    for ( s32 liEntry = 0; liEntry < miStreamListLength; liEntry++ )
    {
        if ( mpCurrentEntryList[liEntry].mResourceId == 0 )
        {
            liFree = liEntry;
            break;
        }
    }
    if ( liFree < 0 )
    {
        return -1;
    }

    const s32 liPick = QueryLoad( mpPotentialList, miPotentialListLength );
    if ( liPick < 0 )
    {
        return -1;
    }

    mpCurrentEntryList[liFree].mbSafe      = mpPotentialList[liPick].mbSafe;
    mpCurrentEntryList[liFree].mResourceId = mpPotentialList[liPick].mResourceId;
    mpCurrentEntryList[liFree].meStatus    = StreamerCurrentEntry::E_STATUS_LOADING;
    mpCurrentEntryList[liFree].muUserId    = mpPotentialList[liPick].muUserId;
    return liFree;
}

// ---------------------------------------------------------------------------
// AttemptUnload  @ 0x827BE3D8 -- pick one potential unload (through the leaf's
// QueryUnload policy) and mark its current-list slot UNLOADING.
// ---------------------------------------------------------------------------
s32 InternalBaseStreamer::AttemptUnload()
{
    if ( CalculatePotentialUnloadList() <= 0 )
    {
        return -1;
    }

    const s32 liPick = QueryUnload( mpPotentialList, miPotentialListLength );
    if ( liPick < 0 )
    {
        return -1;
    }

    const s32 liEntry = FindInCurrentList( mpPotentialList[liPick].mResourceId,
                                           mpPotentialList[liPick].muUserId );
    // The X360 falls straight through the not-found path into the status store, i.e.
    // it writes mpCurrentEntryList[-1].meStatus when the picked potential entry has
    // no current-list slot -- a genuine console out-of-bounds store (unreachable in
    // practice: the potential list is built FROM the current list). Guarded here
    // [documented X360 defect]; the return value is identical.
    if ( liEntry >= 0 )
    {
        mpCurrentEntryList[liEntry].meStatus = StreamerCurrentEntry::E_STATUS_UNLOADING;
    }
    return liEntry;
}

// ---------------------------------------------------------------------------
// Stage the GameData load request for one current-list slot. The X360 open-codes
// this event build three times inside UpdateLoading (0x827D41E8); it is the
// committed LoadGameDataEvent posted with queue type id 26.
// ---------------------------------------------------------------------------
void InternalBaseStreamer::PostLoadRequest( s32 liListIndex )
{
    BrnResource::GameDataIO::LoadGameDataEvent lEvent;
    lEvent.miEventId       = liListIndex;
    lEvent.mpReceiverQueue = &mGDReceiverQueue;
    // "Slot pool system" streamers give every list slot its own pool id.
    lEvent.miPoolId        = miPoolId + ( mbSlotPoolSystem ? liListIndex : 0 );
    lEvent.mId             = mpCurrentEntryList[liListIndex].mResourceId;
    lEvent.meType          = meAssetSet;
    lEvent.mbFailFlag      = mbAllowFailure;
    mGDRequestInterface.mRequestQueue.AddEvent( &lEvent, 26 );
}

// ---------------------------------------------------------------------------
// UpdateIdle  @ 0x827C4D38 -- nothing in flight: drain every possible unload into
// the pending queue first, otherwise start one load.
// ---------------------------------------------------------------------------
InternalBaseStreamer::EStreamMode InternalBaseStreamer::UpdateIdle()
{
    CGS_ASSERT( mPendingEntryQueue.GetLength() == 0, "mPendingEntryQueue.GetLength() == 0" );

    s32 liUnload = AttemptUnload();
    while ( liUnload >= 0 )
    {
        OnUnloadBegin( liUnload );
        mPendingEntryQueue.Push( &liUnload );
        liUnload = AttemptUnload();
    }

    if ( mPendingEntryQueue.GetLength() > 0 )
    {
        meUnloadStreamStage = E_UNLOADSTREAM_REQUEST;
        return E_STREAMMODE_UNLOADING;
    }

    miCurrentEntryIndex = AttemptLoad();
    if ( miCurrentEntryIndex < 0 )
    {
        return E_STREAMMODE_IDLE;
    }

    OnLoadBegin( miCurrentEntryIndex );
    meLoadStreamStage = E_LOADSTREAM_REQUEST;

    // The console immediately queues a SECOND load so the request stage can post two
    // requests back to back (double-buffered streaming).
    s32 liSecond = AttemptLoad();
    if ( liSecond >= 0 )
    {
        mPendingEntryQueue.Push( &liSecond );
    }
    return E_STREAMMODE_LOADING;
}

// ---------------------------------------------------------------------------
// UpdateLoading  @ 0x827D41E8 -- the three-stage load leg. Returns true when the
// leg is finished and the machine should go back to IDLE.
// ---------------------------------------------------------------------------
bool InternalBaseStreamer::UpdateLoading()
{
    switch ( meLoadStreamStage )
    {
    case E_LOADSTREAM_REQUEST:
        {
            PostLoadRequest( miCurrentEntryIndex );
            if ( mPendingEntryQueue.GetLength() > 0 )
            {
                PostLoadRequest( mPendingEntryQueue[0] );
            }
            mbLoadHasFailed   = false;
            meLoadStreamStage = E_LOADSTREAM_WAIT;
            return false;
        }

    case E_LOADSTREAM_WAIT:
        {
            mGDRequestInterface.mRequestQueue.Clear();

            const s32 liNumReplies = mGDReceiverQueue.GetLength();
            if ( liNumReplies == 0 )
            {
                return false;
            }

            // The X360 asserts exactly one reply per visit ("Expected only 1 event from
            // game data module", BrnBaseStreamer.cpp:704 in UpdateLoading @0x827D41E8),
            // then consumes only the FIRST event and Clear()s -- any extra reply is
            // discarded. The invariant holds on the console because its async bundle
            // pipeline delivers at most one completion between streamer updates; on PC
            // the module pump can run more than once between streamer visits, so BOTH
            // pipelined loads' replies can be queued here. Dropping the second de-syncs
            // the FIFO pairing (every later slot binds the PREVIOUS unit's resources).
            // PC divergence: log instead of asserting, and consume every queued reply in
            // arrival order, running the console's E_LOADSTREAM_DONE leg between replies
            // -- the same work N consecutive console updates would do.
            if ( liNumReplies != 1 )
            {
                *CgsDev::Log::gpDebugPrint << "[stream] " << liNumReplies
                    << " GameData replies in one streamer update (X360 invariant is 1,"
                       " asserts @BrnBaseStreamer.cpp:704 and drops extras); consuming"
                       " all in order\n";
            }

            const CgsModule::Event* lpEventData = 0;
            s32 liEventSize = 0;
            s32 liEventType = mGDReceiverQueue.GetFirstEvent( &lpEventData, &liEventSize );

            while ( lpEventData != 0 )
            {
                CGS_ASSERT( liEventType >= 26 && liEventType < 69, "Unexpected GameData reply type" );

                const BrnResource::GameDataIO::GameDataAssetEvent* lpReply =
                    static_cast<const BrnResource::GameDataIO::GameDataAssetEvent*>( lpEventData );

                // Replies arrive FIFO in request order, so each must be for the machine's
                // current entry (the reply echoes the request's list index in miEventId --
                // PostLoadRequest stages it, GameDataModule::PostGameDataResponse echoes it).
                CGS_ASSERT( lpReply == 0 || lpReply->miEventId == miCurrentEntryIndex,
                            "GameData reply out of order" );

                if ( lpReply != 0 && lpReply->mbFailFlag )
                {
                    StreamerCurrentEntry& lrEntry = mpCurrentEntryList[miCurrentEntryIndex];
                    lrEntry.mbSafe      = false;
                    lrEntry.mResourceId = 0;
                    lrEntry.meStatus    = StreamerCurrentEntry::E_STATUS_EMPTY;
                    lrEntry.muUserId    = 0;
                    OnLoadFail( lpReply, miCurrentEntryIndex );
                    mbLoadHasFailed = true;
                }
                else
                {
                    mpCurrentEntryList[miCurrentEntryIndex].meStatus = StreamerCurrentEntry::E_STATUS_READY;
                    OnLoadComplete( lpReply, miCurrentEntryIndex );
                }

                liEventType = mGDReceiverQueue.GetNextEvent( lpEventData, &lpEventData, &liEventSize );
                if ( lpEventData == 0 )
                {
                    break;
                }

                // Another reply is queued: it belongs to the pipelined pending load. Run
                // the console E_LOADSTREAM_DONE leg (@0x827D41E8 stage 2) now so it is
                // consumed against the right entry.
                CGS_ASSERT( mPendingEntryQueue.GetLength() > 0, "GameData reply with no pending load" );
                if ( mPendingEntryQueue.GetLength() <= 0 )
                {
                    break;   // orphan reply -- dropped, as the console's Clear() would
                }

                mPendingEntryQueue.Pop( &miCurrentEntryIndex );
                OnLoadBegin( miCurrentEntryIndex );

                if ( !mbLoadHasFailed && CalculatePotentialUnloadList() == 0 )
                {
                    const s32 liNext = AttemptLoad();
                    if ( liNext >= 0 )
                    {
                        PostLoadRequest( liNext );
                        mPendingEntryQueue.Push( &liNext );
                    }
                }
            }

            mGDReceiverQueue.Clear();
            meLoadStreamStage = E_LOADSTREAM_DONE;
            return false;
        }

    case E_LOADSTREAM_DONE:
        {
            if ( mPendingEntryQueue.GetLength() <= 0 )
            {
                return true;
            }

            mPendingEntryQueue.Pop( &miCurrentEntryIndex );
            OnLoadBegin( miCurrentEntryIndex );

            // A failed load, or anything now wanting to unload, ends the pipelining --
            // the already-queued request just runs to its WAIT stage.
            if ( !mbLoadHasFailed && CalculatePotentialUnloadList() == 0 )
            {
                const s32 liNext = AttemptLoad();
                if ( liNext >= 0 )
                {
                    PostLoadRequest( liNext );
                    mPendingEntryQueue.Push( &liNext );
                }
            }

            meLoadStreamStage = E_LOADSTREAM_WAIT;
            return false;
        }

    default:
        CGS_ASSERT( false, "Invalid load stream stage" );
        return false;
    }
}

// ---------------------------------------------------------------------------
// UpdateUnloading  @ 0x827D4670 -- the unload leg: post one UnloadGameDataEvent
// (queue type 39) per pending index the leaf lets go, then consume the replies.
// ---------------------------------------------------------------------------
bool InternalBaseStreamer::UpdateUnloading()
{
    switch ( meUnloadStreamStage )
    {
    case E_UNLOADSTREAM_REQUEST:
        {
            bool lbAnyHeldBack = false;

            for ( s32 liSlot = 0; liSlot < mPendingEntryQueue.GetLength(); liSlot++ )
            {
                const s32 liListIndex = mPendingEntryQueue[static_cast<u32>( liSlot )];
                if ( liListIndex < 0 )
                {
                    continue;
                }

                if ( !OnAttemptUnload( liListIndex ) )
                {
                    lbAnyHeldBack = true;
                    continue;
                }

                BrnResource::GameDataIO::UnloadGameDataEvent lEvent;
                lEvent.miEventId       = liListIndex;
                lEvent.mpReceiverQueue = &mGDReceiverQueue;
                lEvent.miPoolId        = miPoolId + ( mbSlotPoolSystem ? liListIndex : 0 );
                lEvent.mId             = mpCurrentEntryList[liListIndex].mResourceId;
                lEvent.meType          = meAssetSet;
                lEvent.mbFailFlag      = false;
                mGDRequestInterface.mRequestQueue.AddEvent( &lEvent, 39 );

                mPendingEntryQueue[static_cast<u32>( liSlot )] = -1;
            }

            if ( !lbAnyHeldBack )
            {
                meUnloadStreamStage = E_UNLOADSTREAM_WAIT;
            }
            return false;
        }

    case E_UNLOADSTREAM_WAIT:
        {
            mGDRequestInterface.mRequestQueue.Clear();

            if ( mGDReceiverQueue.GetLength() < mPendingEntryQueue.GetLength() )
            {
                return false;
            }
            CGS_ASSERT( mGDReceiverQueue.GetLength() == mPendingEntryQueue.GetLength(),
                        "mGDReceiverQueue.GetLength() == mPendingEntryQueue.GetLength()" );

            const CgsModule::Event* lpEventData = 0;
            s32 liEventSize = 0;
            mGDReceiverQueue.GetFirstEvent( &lpEventData, &liEventSize );

            while ( lpEventData != 0 )
            {
                const BrnResource::GameDataIO::UnloadGameDataResponse* lpReply =
                    static_cast<const BrnResource::GameDataIO::UnloadGameDataResponse*>( lpEventData );
                const s32 liListIndex = lpReply->miEventId;

                OnUnloadComplete( lpReply, liListIndex );
                mpCurrentEntryList[liListIndex].mResourceId = 0;
                mpCurrentEntryList[liListIndex].meStatus    = StreamerCurrentEntry::E_STATUS_EMPTY;

                mGDReceiverQueue.GetNextEvent( lpEventData, &lpEventData, &liEventSize );
            }

            mGDReceiverQueue.Clear();
            mPendingEntryQueue.Clear();
            meUnloadStreamStage = E_UNLOADSTREAM_DONE;
            return false;
        }

    case E_UNLOADSTREAM_DONE:
        return true;

    default:
        CGS_ASSERT( false, "Invalid unload stream stage" );
        return false;
    }
}

// ---------------------------------------------------------------------------
// Update  @ 0x827D5F50 -- one frame of the machine. The request queue is cleared
// FIRST so the frame's requests are exactly what this call stages.
// ---------------------------------------------------------------------------
void InternalBaseStreamer::Update()
{
    mGDRequestInterface.mRequestQueue.Clear();

    switch ( meCurrentStreamMode )
    {
    case E_STREAMMODE_IDLE:
        meCurrentStreamMode = UpdateIdle();
        break;

    case E_STREAMMODE_LOADING:
        if ( UpdateLoading() )
        {
            meCurrentStreamMode = E_STREAMMODE_IDLE;
        }
        break;

    case E_STREAMMODE_UNLOADING:
        if ( UpdateUnloading() )
        {
            meCurrentStreamMode = E_STREAMMODE_IDLE;
        }
        break;

    default:
        CGS_ASSERT( false, "Invalid stream mode" );
        break;
    }
}

// ---------------------------------------------------------------------------
// IsStreamComplete  @ 0x827B0BE8 -- true when every wanted entry is live and READY.
// ---------------------------------------------------------------------------
bool InternalBaseStreamer::IsStreamComplete() const
{
    for ( s32 liEntry = 0; liEntry < miStreamListLength; liEntry++ )
    {
        const StreamerTargetEntry& lrTarget = mpTargetEntryList[liEntry];
        if ( lrTarget.mResourceId == 0 )
        {
            continue;
        }

        const s32 liCurrent = FindInCurrentList( lrTarget.mResourceId, lrTarget.muUserId );
        if ( liCurrent < 0 )
        {
            return false;
        }
        if ( mpCurrentEntryList[liCurrent].meStatus != StreamerCurrentEntry::E_STATUS_READY )
        {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// DebugGetAssetStatus  @ 0x827BE4C8 -- the debug-overlay status of one asset id.
// ---------------------------------------------------------------------------
InternalBaseStreamer::EAssetStatus InternalBaseStreamer::DebugGetAssetStatus( CgsID lId ) const
{
    const s32 liTarget  = FindInTargetList( lId );
    const s32 liCurrent = FindInCurrentList( lId );

    if ( liTarget >= 0 )
    {
        if ( liCurrent < 0 )
        {
            return E_AS_PENDING_LOAD;
        }
        switch ( mpCurrentEntryList[liCurrent].meStatus )
        {
        case StreamerCurrentEntry::E_STATUS_LOADING:   return E_AS_LOADING;
        case StreamerCurrentEntry::E_STATUS_UNLOADING: return E_AS_UNLOADING;
        case StreamerCurrentEntry::E_STATUS_READY:     return E_AS_READY;
        default:                                       return E_AS_UNKNOWN;
        }
    }

    if ( liCurrent < 0 )
    {
        return E_AS_NOT_IN_LIST;
    }
    switch ( mpCurrentEntryList[liCurrent].meStatus )
    {
    case StreamerCurrentEntry::E_STATUS_LOADING:   return E_AS_PENDING_ABORT_LOAD;
    case StreamerCurrentEntry::E_STATUS_UNLOADING: return E_AS_UNLOADING;
    case StreamerCurrentEntry::E_STATUS_READY:     return E_AS_PENDING_UNLOAD;
    default:                                       return E_AS_UNKNOWN;
    }
}

// IsAssetLoaded @ 0x822A1300.
//   miStreamListLength @ +0x10 (lwz r9,0x10(r3)); early-out 0 when <= 0.
//   mpCurrentEntryList @ +0xC  (lwz r11,0xC(r3)).
//   Per entry (stride 0x18): match when mResourceId (ld @0, 8 bytes) == lId AND
//   muUserId (ld @0x10, 8 bytes) == luUserData AND meStatus (lwz @0xC) == 3
//   (E_STATUS_READY). Returns 1 on the first match, 0 if none / empty list.
bool InternalBaseStreamer::IsAssetLoaded( CgsID lId, u64 luUserData ) const
{
    for( s32 liIndex = 0; liIndex < miStreamListLength; liIndex++ )
    {
        if( ( mpCurrentEntryList[liIndex].mResourceId == lId ) &&
            ( mpCurrentEntryList[liIndex].muUserId    == luUserData ) &&
            ( mpCurrentEntryList[liIndex].meStatus    == StreamerCurrentEntry::E_STATUS_READY ) )
        {
            return true;
        }
    }
    return false;
}

// IsEntryReady @ 0x827B0258.
//   Asserts liIndex < miStreamListLength (BrnBaseStreamer.h:409, "liIndex <
//   miStreamListLength"); the assert collapses to the house CGS_ASSERT.
//   Returns mpCurrentEntryList[liIndex].meStatus == 3 (E_STATUS_READY): the X360
//   computes 24*liIndex (slwi r11,liIndex,1; add liIndex; slwi r11,3), adds
//   mpCurrentEntryList, loads meStatus @ +0xC, subtracts 3, then cntlzw/extrwi to
//   produce the bool (status == 3).
bool InternalBaseStreamer::IsEntryReady( s32 liIndex ) const
{
    CGS_ASSERT( liIndex < miStreamListLength, "liIndex < miStreamListLength" );
    return mpCurrentEntryList[liIndex].meStatus == StreamerCurrentEntry::E_STATUS_READY;
}

} // namespace BrnWorld
