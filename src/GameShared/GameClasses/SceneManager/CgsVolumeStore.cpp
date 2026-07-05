#include "GameShared/GameClasses/SceneManager/CgsVolumeStore.h"
#include "rw/rwcore_structs.h"                     // rw::BaseResourceDescriptor
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <string.h>                                // memcpy (inlined VolumeSlot::SetVolume)

// CgsSceneManager::VolumeStore<4608> -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//   RemoveVolume                 @ 0x828C4108
//   GetVolumeResourceDescriptor  @ 0x828C5A98
//   ReplaceVolume                @ 0x828CC1D8

namespace CgsSceneManager
{

// RemoveVolume @ 0x828C4108 (store-for-store). Free the pooled slot at
// liVolumeIndex. Two gating tripwire asserts (index-in-range and
// slot-currently-allocated) precede the pool free. The X360 body passes the
// store `this` straight into the pool calls because mVolumePool is the store's
// first member (offset 0), so &mVolumePool == this.
template <s32 tiCapacity>
void VolumeStore<tiCapacity>::RemoveVolume(s32 liVolumeIndex)
{
    CGS_ASSERT(liVolumeIndex >= 0 && liVolumeIndex < tiCapacity,
               "liVolumeIndex >= 0 && liVolumeIndex < BufferSize");
    CGS_ASSERT(mVolumePool.IsObjectAllocated(liVolumeIndex),
               "mVolumePool.IsObjectAllocated( liVolumeIndex )");
    mVolumePool.FreeObject(liVolumeIndex);
}

// GetVolumeResourceDescriptor @ 0x828C5A98 (store-for-store). Returns the
// serialised resource descriptor (rw::BaseResourceDescriptors<5>, five
// {u32 size, u32 align} entries) for the collision volume referenced by
// lpVolume. The X360 body reads the volume's RW collision type via the pointer
// at VolRef::Volume+0x40 (lwz r11,0x40(r5); lwz r11,0(r11)) and switches on it:
// the six primitive volume types (1..6) all size to a single {0x60, 0x10} block;
// any other type trips the "Volume type not supported" assert and falls back to
// the identity {0, 1}. Entries 1..4 are always {0, 1}. DWARF (CgsVolumeStore.h:350)
// attests this method NON-const.
template <s32 tiCapacity>
CgsResource::ResourceDescriptor
VolumeStore<tiCapacity>::GetVolumeResourceDescriptor(const VolRef::Volume* lpVolume)
{
    // Attested double indirection: mpRwVolume @ VolRef::Volume+0x40; the RW
    // collision volume-type enum is its first dword. (VolRef::Volume is opaque
    // here, so read through raw bytes rather than fabricate a layout.)
    const s32* const* lppRwVolume =
        reinterpret_cast<const s32* const*>(
            reinterpret_cast<const u8*>(lpVolume) + 0x40);
    const s32 liRwVolumeType = **lppRwVolume;

    // Entry0 size/align, selected by the RW collision-volume type.
    u32 luEntry0Size;
    u32 luEntry0Align;

    switch (liRwVolumeType)
    {
    case 1:   // the six rw::collision primitive volume types
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
        luEntry0Size  = 0x60u;   // 96 bytes
        luEntry0Align = 0x10u;   // 16-byte aligned
        break;
    default:
        CGS_ASSERT(false, "Volume type not supported by CgsVolumeManager");
        luEntry0Size  = 0u;
        luEntry0Align = 1u;
        break;
    }

    CgsResource::ResourceDescriptor lDescriptor;
    rw::BaseResourceDescriptor* lpEntries = lDescriptor.m_baseResourceDescriptors;

    lpEntries[0].m_size = luEntry0Size;  lpEntries[0].m_alignment = luEntry0Align;
    lpEntries[1].m_size = 0;             lpEntries[1].m_alignment = 1;
    lpEntries[2].m_size = 0;             lpEntries[2].m_alignment = 1;
    lpEntries[3].m_size = 0;             lpEntries[3].m_alignment = 1;
    lpEntries[4].m_size = 0;             lpEntries[4].m_alignment = 1;

    return lDescriptor;
}

// ReplaceVolume @ 0x828CC1D8 (store-for-store). Overwrite the pooled slot at
// liVolumeIndex with a fresh copy of lpVolume. The X360 body inlines both
// IsPrimitiveVolume and VolumeSlot::SetVolume (neither has its own ledger symbol):
// only the primitive gate, the descriptor sizing, the two size tripwire asserts, the
// pool operator[] and the final memcpy remain.
//
// The inlined IsPrimitiveVolume reads the RW collision-volume type via the double
// indirection at VolRef::Volume+0x40 (lwz r11,0x40; lwz r11,0(r11)) and treats the block
// as primitive when (type-1) <= 4, i.e. types 1..5 (SPHERE, CAPSULE, TRIANGLE, BOX,
// CYLINDER) -- NOT the aggregate type 6. Anything else trips the aggregate assert and
// returns false. mVolumePool is the store's first member (offset 0), so the X360 passes
// `this` straight into the pool calls.
template <s32 tiCapacity>
bool VolumeStore<tiCapacity>::ReplaceVolume(s32 liVolumeIndex, const VolRef::Volume* lpVolume)
{
    CGS_ASSERT(liVolumeIndex >= 0 && liVolumeIndex < tiCapacity,
               "liVolumeIndex >= 0 && liVolumeIndex < BufferSize");

    // Inlined IsPrimitiveVolume: read the RW collision volume-type enum through
    // VolRef::Volume+0x40 (mpRwVolume), whose first dword is the type. Primitive when the
    // type is one of 1..5; the aggregate type (6) and everything else fall through.
    const s32* const* lppRwVolume =
        reinterpret_cast<const s32* const*>(
            reinterpret_cast<const u8*>(lpVolume) + 0x40);
    const s32 liRwVolumeType = **lppRwVolume;

    if (static_cast<u32>(liRwVolumeType - 1) > 4u)
    {
        CGS_ASSERT(false, "Aggregate volumes are not supported by the scene manager");
        return false;
    }

    // Size the serialised descriptor for this primitive volume; entry0's size is the number
    // of bytes to copy into the slot.
    CgsResource::ResourceDescriptor lDescriptor = GetVolumeResourceDescriptor(lpVolume);
    const s32 liVolumeSize = static_cast<s32>(lDescriptor.m_baseResourceDescriptors[0].m_size);
    CGS_ASSERT(liVolumeSize <= KI_VOLUME_SLOT_SIZE,
               "liVolumeSize <= KI_VOLUME_SLOT_SIZE");

    // Inlined VolumeSlot::SetVolume: fetch the slot (pool operator[]) and copy the serialised
    // volume in place. The tripwire mirrors the source-level bounds on the copy size (>=
    // sizeof(rw::collision::Volume) == 96, <= KI_VOLUME_SLOT_SIZE).
    VolumeSlot& lrSlot = mVolumePool[liVolumeIndex];
    CGS_ASSERT(liVolumeSize >= 0x60 && liVolumeSize <= KI_VOLUME_SLOT_SIZE,
               "( miSize >= ( int32_t ) sizeof( rw::collision::Volume ) ) && ( miSize <= KI_VOLUME_SLOT_SIZE )");
    memcpy(lrSlot.GetVolume(), lpVolume, static_cast<size_t>(liVolumeSize));
    return true;
}

// Explicit instantiations for the 4608-slot store (the X360 ledger symbols).
template void VolumeStore<4608>::RemoveVolume(s32);
template CgsResource::ResourceDescriptor
VolumeStore<4608>::GetVolumeResourceDescriptor(const VolRef::Volume*);
template bool VolumeStore<4608>::ReplaceVolume(s32, const VolRef::Volume*);

}
