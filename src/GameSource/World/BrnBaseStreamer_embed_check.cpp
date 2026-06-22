// Embed check for GameSource/World/BrnBaseStreamer.h
// Exercises the single ledger function bodied by this TU:
//   BrnWorld::WorldGraphicsStreamer::IsListLoaded  @ 0x827B0D00
// (which tail-calls InternalBaseStreamer::IsEntryReady), plus the streamer base
// family shape (StreamerCurrentEntry / StreamerTargetEntry / BaseStreamer<32>).

#include "GameSource/World/BrnBaseStreamer.h"

using namespace BrnWorld;

// Concrete streamer so the abstract InternalBaseStreamer pure-virtuals are
// satisfied for instantiation in this test (WorldGraphicsStreamer overrides them
// in its real home; here a trivial concrete just proves the forwarder compiles).
namespace
{
class TestStreamer : public WorldGraphicsStreamer
{
protected:
    s32  QueryLoad( const StreamerTargetEntry*, s32 ) override { return 0; }
    s32  QueryUnload( const StreamerTargetEntry*, s32 ) override { return 0; }
    void OnLoadBegin( s32 ) override {}
    void OnUnloadBegin( s32 ) override {}
    void OnLoadComplete( const BrnResource::GameDataIO::GameDataAssetEvent*, s32 ) override {}
    void OnUnloadComplete( const BrnResource::GameDataIO::UnloadGameDataResponse*, s32 ) override {}
};
}

// Enum-value parity (DWARF BrnBaseStreamer.h:78).
static_assert( StreamerCurrentEntry::E_STATUS_READY == 3, "ready status" );
static_assert( InternalBaseStreamer::E_AS_PENDING_UNLOAD == 7, "asset-status tail" );

// IsListLoaded forwards to IsEntryReady -> returns bool.
static_assert( sizeof( decltype( ( (TestStreamer*)nullptr )->IsListLoaded( 0 ) ) ) == sizeof( bool ),
               "IsListLoaded returns bool" );

void brn_base_streamer_embed_check( TestStreamer& rStreamer, s32 liIndex )
{
    // Drive the bodied forwarder.
    volatile bool lbLoaded = rStreamer.IsListLoaded( liIndex );
    (void)lbLoaded;
}
