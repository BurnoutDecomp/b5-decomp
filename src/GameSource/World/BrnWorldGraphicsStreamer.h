#ifndef BRN_WORLD_WORLD_GRAPHICS_STREAMER_H
#define BRN_WORLD_WORLD_GRAPHICS_STREAMER_H

#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"   // CgsResource::ResourcePtr / BaseResourcePtr

// =============================================================================
// BrnWorld::WorldGraphicsStreamer
//   GameSource/World/BrnWorldGraphicsStreamer.{h,cpp}
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The ONE recovered function in this TU
// is the accessor GetInstanceList @ 0x822CD548:
//
//   v2 = this + 0x19FC + 32 * index;            ; &maInstanceLists[index]
//   if (CgsResource::BaseResourcePtr::IsEqual(&dword_82FAD94C, v2)) return 0;
//   return CgsGraphics::InstanceList::GetMemoryR(v2);   ; ResourcePtr<>::GetMemoryResource()
//
// i.e. the streamer holds an INLINE array of CgsResource::ResourcePtr<InstanceList>
// (element stride 32 bytes — sizeof(BaseResourcePtr) rounded up). GetInstanceList
// returns the index'th list's main-memory resource, or nullptr when that slot is the
// empty/default resource pointer (the X360 compares the slot against a static default
// BaseResourcePtr `dword_82FAD94C` via BaseResourcePtr::IsEqual).
//
// LAYOUT NOTE / FLAG (partial keystone view): the X360 places the array at the fixed
// object offset +0x19FC, behind ~6652 bytes of streamer state that has NO DWARF/leak
// home and is NOT recovered. This header therefore models ONLY the inline
// ResourcePtr array BY NAME — it does NOT reconstruct the preceding streamer layout
// and does NOT pin the array to +0x19FC (the host vptr/pointer widths differ anyway).
// The array CAPACITY is likewise not X360-attested here; it is modelled as a single
// flexible-size span the accessor indexes by name. The bounds are the caller's
// responsibility, exactly as in the X360 (the accessor performs no bounds check).
// Absolute offsets/size are NOT static_asserted. FLAG: WorldGraphicsStreamer's full
// layout is a DEFERRED keystone; only GetInstanceList's by-name semantics are homed.
// =============================================================================

namespace CgsGraphics
{
    // Committed-but-headerless type (home: CgsInstance.cpp). ResourcePtr<T> only
    // static_casts a void* to T*, so a forward declaration suffices here.
    struct InstanceList;
}

namespace BrnWorld
{

// Streams the per-region graphics instance lists. Only the instance-list accessor is
// homed (see header FLAG); the streaming state machine is deferred.
class WorldGraphicsStreamer
{
public:
    // GetInstanceList @ 0x822CD548 — returns the index'th instance list's main-memory
    // resource, or nullptr when the slot is the empty/default resource pointer.
    CgsGraphics::InstanceList* GetInstanceList(s32 liIndex);

private:
    // The inline array of per-region instance-list resource pointers (X360 +0x19FC,
    // 32-byte stride). Modelled BY NAME; the preceding streamer state and the true
    // capacity are DEFERRED (see header FLAG). KU_MAX_INSTANCE_LISTS is a modelling
    // placeholder, NOT an X360-attested count.
    static const s32 KU_MAX_INSTANCE_LISTS = 1;
    CgsResource::ResourcePtr<CgsGraphics::InstanceList> maInstanceLists[KU_MAX_INSTANCE_LISTS];
};

} // namespace BrnWorld

#endif // BRN_WORLD_WORLD_GRAPHICS_STREAMER_H
