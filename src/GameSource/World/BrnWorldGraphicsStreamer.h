#ifndef BRN_WORLD_WORLD_GRAPHICS_STREAMER_H
#define BRN_WORLD_WORLD_GRAPHICS_STREAMER_H

#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"   // CgsResource::ResourcePtr / BaseResourcePtr
#include "GameSource/World/BrnBaseStreamer.h"                        // BaseStreamer<N> / StreamerTargetEntry

// =============================================================================
// BrnWorld::WorldGraphicsStreamer
//   GameSource/World/BrnWorldGraphicsStreamer.{h,cpp}
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The world-graphics streamer: a BaseStreamer<32> over the "TRK_UNIT%d"
// GameData ids, owned by (and calling back into) BrnWorld::WorldEntityModule.
// X360 object map (32-bit; PC accesses BY NAME): base streamer +0x0..0x19F7,
// mpWorldEntityModule @ +0x19F8, maInstanceLists[32] @ +0x19FC (32-byte
// ResourcePtr stride).
//
//   Construct        @ 0x827CA388  base Construct(len 32, pool 3, asset-set 0,
//                                  allow-failure true) + module back-pointer +
//                                  every instance-list slot set to the default
//                                  (empty) resource handle.
//   GetIndexFromId   @ 0x827B0CB8  current-entry scan by zone number (the low
//                                  32 bits of muUserId).
//   IsListLoaded     @ 0x827B0D00  forwards InternalBaseStreamer::IsEntryReady.
//   GetInstanceList  @ 0x822CD548  slot -> main-memory InstanceList (or null
//                                  when the slot is the empty/default pointer).
//   QueryLoad        @ 0x827B0C98  -> module->QueryWorldGraphicsLoad.
//   OnLoadBegin      @ 0x827B0CA8  -> module->OnWorldGraphicsLoadBegin (empty
//                                  in the shipped build; ICF-folded alias).
//   OnUnloadBegin    @ 0x827B0CB0  -> module->OnWorldGraphicsUnloadBegin.
//   OnLoadComplete   @ 0x827BE5C8  capture the loaded instance-list handle
//                                  (event + 32) into the slot, then
//                                  module->OnWorldGraphicsLoadComplete.
//   OnUnloadComplete @ 0x827BE638  reset the slot to the default handle, then
//                                  module->OnWorldGraphicsUnloadComplete
//                                  (empty in the shipped build; ICF alias).
//   QueryUnload                    not separately emitted (ICF-folded); the
//                                  unload policy forwards the module's
//                                  QueryWorldGraphicsUnload ("first candidate").
// =============================================================================

namespace CgsGraphics
{
    struct InstanceList;
}

namespace BrnWorld
{

class WorldEntityModule;

class WorldGraphicsStreamer : public BaseStreamer<32>
{
public:
    static const s32 KI_MAX_INSTANCE_LISTS = 32;

    // @ 0x827CA388.
    void Construct( WorldEntityModule* lpWorldEntityModule );

    // @ 0x827B0CB8 -- index of the current entry streaming the given zone
    // number (the low 32 bits of muUserId), or -1.
    s32 GetIndexFromId( s32 liZoneNumber ) const;

    // @ 0x827B0D00 -- forwards InternalBaseStreamer::IsEntryReady.
    bool IsListLoaded( s32 liIndex ) const
    {
        return IsEntryReady( liIndex );
    }

    // @ 0x822CD548 -- the index'th instance list's main-memory resource, or
    // null when that slot is the empty/default resource pointer.
    CgsGraphics::InstanceList* GetInstanceList( s32 liIndex );

protected:
    // BaseStreamer notification hooks -- forward into the owning module.
    virtual s32  QueryLoad( const StreamerTargetEntry* lpPotentialList, s32 liPotentialListLength );   // @0x827B0C98
    virtual s32  QueryUnload( const StreamerTargetEntry* lpPotentialList, s32 liPotentialListLength );
    virtual void OnLoadBegin( s32 liListIndex );                                                       // @0x827B0CA8
    virtual void OnUnloadBegin( s32 liListIndex );                                                     // @0x827B0CB0
    virtual void OnLoadComplete( const BrnResource::GameDataIO::GameDataAssetEvent* lpEvent, s32 liListIndex );      // @0x827BE5C8
    virtual void OnUnloadComplete( const BrnResource::GameDataIO::UnloadGameDataResponse* lpEvent, s32 liListIndex ); // @0x827BE638

private:
    WorldEntityModule* mpWorldEntityModule;   // X360 +0x19F8

    // X360 +0x19FC, 32-byte stride.
    CgsResource::ResourcePtr<CgsGraphics::InstanceList> maInstanceLists[KI_MAX_INSTANCE_LISTS];
};

} // namespace BrnWorld

#endif // BRN_WORLD_WORLD_GRAPHICS_STREAMER_H
