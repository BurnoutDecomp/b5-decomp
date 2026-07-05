#include "GameShared/GameClasses/SceneManager/CgsVolumeStore.h"
#include "rw/rwcore_structs.h"                     // rw::BaseResourceDescriptor
#include "GameShared/GameClasses/Core/CgsAssert.h"

// CgsSceneManager::VolumeStore<4608> -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//   RemoveVolume                 @ 0x828C4108
//   GetVolumeResourceDescriptor  @ 0x828C5A98

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

// Explicit instantiations for the 4608-slot store (the X360 ledger symbols).
template void VolumeStore<4608>::RemoveVolume(s32);
template CgsResource::ResourceDescriptor
VolumeStore<4608>::GetVolumeResourceDescriptor(const VolRef::Volume*);

}
