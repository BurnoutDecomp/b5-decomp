#include "GameSource/World/BrnWorldGraphicsStreamer.h"

// =============================================================================
// BrnWorld::WorldGraphicsStreamer — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnWorldGraphicsStreamer.h for the
// inline-ResourcePtr-array shape and the partial-keystone layout note.
//
// dep_flags: depends on two DONE TUs, called BY NAME:
//   * CgsResource::BaseResourcePtr::IsEqual @ 0x8227D298 (CgsResourcePtr.h home).
//   * CgsResource::ResourcePtr<CgsGraphics::InstanceList>::GetMemoryResource (the X360
//     CgsGraphics::InstanceList::GetMemoryR @ 0x822CA008) — instantiated in
//     CgsResourcePtr_CgsGraphics_InstanceList.cpp; declared inline in CgsResourcePtr.h.
// =============================================================================

namespace BrnWorld
{

// The static empty/default resource pointer the X360 compares each slot against
// (dword_82FAD94C). A default-constructed BaseResourcePtr has a null mpResourceMemory
// and null identity, which IsEqual treats as "the slot is empty/unset".
static const CgsResource::BaseResourcePtr KS_EMPTY_INSTANCE_LIST_PTR;

// ---------------------------------------------------------------------------
// GetInstanceList  @ 0x822CD548
//
//   v2 = &maInstanceLists[index];
//   if (BaseResourcePtr::IsEqual(&KS_EMPTY_INSTANCE_LIST_PTR, v2)) return nullptr;
//   return maInstanceLists[index].GetMemoryResource();
//
// The X360 calls IsEqual with the static-default pointer as `this` and the slot as the
// argument; IsEqual is symmetric over the compared identity dwords, so the by-name
// form below (slot.IsEqual(&default)) reproduces the same test.
// ---------------------------------------------------------------------------
CgsGraphics::InstanceList* WorldGraphicsStreamer::GetInstanceList(s32 liIndex)
{
    CgsResource::ResourcePtr<CgsGraphics::InstanceList>& lrSlot = maInstanceLists[liIndex];

    if (lrSlot.IsEqual(&KS_EMPTY_INSTANCE_LIST_PTR))
    {
        return 0;
    }

    return lrSlot.GetMemoryResource();
}

} // namespace BrnWorld
