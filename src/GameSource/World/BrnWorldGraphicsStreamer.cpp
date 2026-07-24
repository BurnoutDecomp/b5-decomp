#include "GameSource/World/BrnWorldGraphicsStreamer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"
#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModule.h"

// =============================================================================
// BrnWorld::WorldGraphicsStreamer -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX; see BrnWorldGraphicsStreamer.h
// for the per-function address map.
// =============================================================================

namespace BrnWorld
{

// The default/empty resource handle the X360 binds into every slot at
// Construct and on unload (dword_8300FAA0): a default-constructed handle whose
// bound ResourcePtr reads back as the empty/default pointer.
static const CgsResource::ResourceHandle KS_DEFAULT_INSTANCE_LIST_HANDLE;

// The static empty/default resource pointer each slot is compared against in
// GetInstanceList (dword_82FAD94C).
static const CgsResource::BaseResourcePtr KS_EMPTY_INSTANCE_LIST_PTR;

// ---------------------------------------------------------------------------
// Construct  @ 0x827CA388
// ---------------------------------------------------------------------------
void
WorldGraphicsStreamer::Construct( WorldEntityModule* lpWorldEntityModule )
{
    // X360: list length 32, pool 3 (E_POOL_OW_GRAPHICS), asset-set 0,
    // allow-failure true.
    BaseStreamer<32>::Construct( 3, static_cast<BrnResource::EAssetSet>( 0 ), true );

    mpWorldEntityModule = lpWorldEntityModule;

    for ( s32 liList = 0; liList < KI_MAX_INSTANCE_LISTS; liList++ )
    {
        maInstanceLists[ liList ] = KS_DEFAULT_INSTANCE_LIST_HANDLE;
    }
}

// ---------------------------------------------------------------------------
// GetIndexFromId  @ 0x827B0CB8
// ---------------------------------------------------------------------------
s32
WorldGraphicsStreamer::GetIndexFromId( s32 liZoneNumber ) const
{
    for ( s32 liEntry = 0; liEntry < GetEntryListLength(); liEntry++ )
    {
        if ( static_cast<u32>( GetUserId( liEntry ) ) == static_cast<u32>( liZoneNumber ) )
        {
            return liEntry;
        }
    }

    return -1;
}

// ---------------------------------------------------------------------------
// GetInstanceList  @ 0x822CD548
//
//   v2 = &maInstanceLists[index];
//   if (BaseResourcePtr::IsEqual(&KS_EMPTY_INSTANCE_LIST_PTR, v2)) return 0;
//   return maInstanceLists[index].GetMemoryResource();
//
// The X360 calls IsEqual with the static-default pointer as `this` and the slot
// as the argument; IsEqual is symmetric over the compared identity dwords, so
// the by-name form below (slot.IsEqual(&default)) reproduces the same test.
// ---------------------------------------------------------------------------
CgsGraphics::InstanceList*
WorldGraphicsStreamer::GetInstanceList( s32 liIndex )
{
    CgsResource::ResourcePtr<CgsGraphics::InstanceList>& lrSlot = maInstanceLists[ liIndex ];

    if ( lrSlot.IsEqual( &KS_EMPTY_INSTANCE_LIST_PTR ) )
    {
        return 0;
    }

    return lrSlot.GetMemoryResource();
}

// ---------------------------------------------------------------------------
// QueryLoad  @ 0x827B0C98
// ---------------------------------------------------------------------------
s32
WorldGraphicsStreamer::QueryLoad( const StreamerTargetEntry* lpPotentialList,
                                  s32 liPotentialListLength )
{
    return mpWorldEntityModule->QueryWorldGraphicsLoad( lpPotentialList, liPotentialListLength );
}

// ---------------------------------------------------------------------------
// QueryUnload  (not separately emitted; ICF-folded "first candidate" policy)
// ---------------------------------------------------------------------------
s32
WorldGraphicsStreamer::QueryUnload( const StreamerTargetEntry* lpPotentialList,
                                    s32 liPotentialListLength )
{
    return mpWorldEntityModule->QueryWorldGraphicsUnload( lpPotentialList, liPotentialListLength );
}

// ---------------------------------------------------------------------------
// OnLoadBegin  @ 0x827B0CA8
// ---------------------------------------------------------------------------
void
WorldGraphicsStreamer::OnLoadBegin( s32 liListIndex )
{
    mpWorldEntityModule->OnWorldGraphicsLoadBegin( liListIndex );
}

// ---------------------------------------------------------------------------
// OnUnloadBegin  @ 0x827B0CB0
// ---------------------------------------------------------------------------
void
WorldGraphicsStreamer::OnUnloadBegin( s32 liListIndex )
{
    mpWorldEntityModule->OnWorldGraphicsUnloadBegin( liListIndex );
}

// ---------------------------------------------------------------------------
// OnLoadComplete  @ 0x827BE5C8
// ---------------------------------------------------------------------------
void
WorldGraphicsStreamer::OnLoadComplete( const BrnResource::GameDataIO::GameDataAssetEvent* lpEvent,
                                       s32 liListIndex )
{
    CGS_ASSERT( mpWorldEntityModule, "mpWorldEntityModule" );

    maInstanceLists[ liListIndex ] = lpEvent->mHandle;

    mpWorldEntityModule->OnWorldGraphicsLoadComplete( lpEvent, liListIndex );
}

// ---------------------------------------------------------------------------
// OnUnloadComplete  @ 0x827BE638
// ---------------------------------------------------------------------------
void
WorldGraphicsStreamer::OnUnloadComplete( const BrnResource::GameDataIO::UnloadGameDataResponse* lpEvent,
                                         s32 liListIndex )
{
    maInstanceLists[ liListIndex ] = KS_DEFAULT_INSTANCE_LIST_HANDLE;

    mpWorldEntityModule->OnWorldGraphicsUnloadComplete( lpEvent );
}

} // namespace BrnWorld
