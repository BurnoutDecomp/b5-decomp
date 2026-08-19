#include "GameShared/GameClasses/SceneManager/CgsVolumeStore.h"
#include "rw/rwcore_structs.h"                     // rw::BaseResourceDescriptor
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <string.h>                                // memcpy (VolumeSlot::SetVolume)

// CgsSceneManager::VolumeStore<4608> + VolumeSlot -- reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//
// Out-of-line X360 bodies:
//   VolumeStore<4608>::RemoveVolume                @ 0x828C4108 (36)
//   VolumeStore<4608>::GetVolumeResourceDescriptor @ 0x828C5A98
//   VolumeStore<4608>::ReplaceVolume               @ 0x828CC1D8 (74)
//
// Header-inline on the console (no export of their own); recovered from the two bodies
// that inlined them, and de-inlined back into their DWARF-declared homes here:
//   VolumeStore<4608>::Construct   (h:172)  <- DWARF VolumeManager::Construct call list
//   VolumeStore<4608>::Prepare     (h:208)  \_ VolumeManager::Prepare @0x828CFD38 emits
//   VolumeStore<4608>::Clear       (h:227)  /  ObjectPool<VolumeSlot,4608,int>::Clear TWICE
//   VolumeStore<4608>::AddVolume   (h:267)  <- VolumeManager::AddDynamicVolume @0x828D1CB8..
//   VolumeStore<4608>::GetVolume   (h:421)  <- VolumeManager::GetRwVolume     @0x828C5F04..
//   VolumeSlot::SetVolume          (h:56)   <- VolumeStore::ReplaceVolume     @0x828CC1D8
//   VolumeSlot::GetVolume          (h:59)   <- VolumeManager::GetRwVolume tail call
//
// mVolumePool is the store's FIRST (and only) member, so the console passes the store's
// own `this` straight into every pool call -- that is why the disassembly shows
// `VolumeSlot,4608,int>::Clear(store)` rather than `(store+offset)`.

namespace CgsSceneManager
{

// ---------------------------------------------------------------------------
// VolumeSlot
// ---------------------------------------------------------------------------

// h:59. The slot's only member is the buffer at offset 0, so "the volume" IS the slot
// address -- the console's inlined form is literally the pool's operator[] result, which
// VolumeManager::GetRwVolume @0x828C5F30 tail-returns unchanged. DWARF declares the
// method const but the returned pointer non-const.
VolRef::Volume* VolumeSlot::GetVolume() const
{
    return reinterpret_cast<VolRef::Volume*>(const_cast<u8*>(macBuffer));
}

// h:56. Copy a serialised volume image into the slot. The tripwire is the one the X360
// bakes inside ReplaceVolume's inlined expansion of this method (it names this method's
// own size parameter, `miSize`), with the lower bound spelled
// `(int32_t) sizeof( rw::collision::Volume )` == 96 == 0x60 (the serialised record size
// pinned a second time by SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h:201).
void VolumeSlot::SetVolume(const VolRef::Volume* lpVolume, s32 liSizeInBytes)
{
    CGS_ASSERT(liSizeInBytes >= 0x60 && liSizeInBytes <= KI_VOLUME_SLOT_SIZE,
               "( miSize >= ( int32_t ) sizeof( rw::collision::Volume ) ) && ( miSize <= KI_VOLUME_SLOT_SIZE )");
    memcpy(macBuffer, lpVolume, static_cast<size_t>(liSizeInBytes));
}

// ---------------------------------------------------------------------------
// VolumeStore<N>
// ---------------------------------------------------------------------------

// h:172 (store-for-store with the DWARF call list of VolumeManager::Construct, whose two
// calls are VolumeStore<4608>::Construct and ObjectPool<VolumeManagerVolume,5048,int>::
// Construct). ObjectPool::Construct is the lifecycle half of Clear -- occupancy bits only,
// no free-queue refill (the owner Clear()s through Prepare before first use).
template <s32 tiCapacity>
void VolumeStore<tiCapacity>::Construct()
{
    mVolumePool.Construct();
}

// h:208. VolumeManager::Prepare @0x828CFD38 opens with TWO back-to-back
// `ObjectPool<VolumeSlot,4608,int>::Clear(this)` calls -- the DWARF names them
// VolumeStore<4608>::Prepare followed by VolumeStore<4608>::Clear -- so each of the pair
// is exactly one pool Clear(). The caller ignores the result (`li r3,1` unconditionally),
// and this body has no failure path.
template <s32 tiCapacity>
bool VolumeStore<tiCapacity>::Prepare()
{
    mVolumePool.Clear();
    return true;
}

// h:227. The second of the pair above.
template <s32 tiCapacity>
void VolumeStore<tiCapacity>::Clear()
{
    mVolumePool.Clear();
}

// h:267 (inlined into VolumeManager::AddDynamicVolume @0x828D1CB8-0x828D1CEC, recovered
// store-for-store):
//     bl  ObjectPool<VolumeSlot,4608,int>::AllocateObject   ; r29 = slot
//     cmpwi r29,-1 ; beq  -> loc_828D1CE8
//     bl  VolumeStore<4608>::ReplaceVolume(store, r29, volume)
//     clrlwi/cmplwi ; bne -> keep r29
//   loc_828D1CE8: li r29,-1
// i.e. take a free slot, copy the volume into it, and hand back the slot index; -1 when
// the pool is exhausted OR the copy was refused (an aggregate/unsupported volume).
// ⚠️ FAITHFUL: the refusal path does NOT free the slot it just allocated. The console
// leaks it exactly like this; not "fixed" here.
template <s32 tiCapacity>
s32 VolumeStore<tiCapacity>::AddVolume(const VolRef::Volume* lpVolume)
{
    s32 liVolumeIndex = mVolumePool.AllocateObject();
    if (liVolumeIndex == KI_INVALID_VOLUME_INDEX)
    {
        return KI_INVALID_VOLUME_INDEX;
    }

    if (!ReplaceVolume(liVolumeIndex, lpVolume))
    {
        liVolumeIndex = KI_INVALID_VOLUME_INDEX;
    }
    return liVolumeIndex;
}

// h:295 @ 0x828C4108 (store-for-store). Free the pooled slot at liVolumeIndex. Two gating
// tripwire asserts (index-in-range and slot-currently-allocated) precede the pool free.
// The X360 body passes the store `this` straight into the pool calls because mVolumePool
// is the store's first member (offset 0), so &mVolumePool == this.
template <s32 tiCapacity>
void VolumeStore<tiCapacity>::RemoveVolume(s32 liVolumeIndex)
{
    CGS_ASSERT(liVolumeIndex >= 0 && liVolumeIndex < tiCapacity,
               "liVolumeIndex >= 0 && liVolumeIndex < BufferSize");
    CGS_ASSERT(mVolumePool.IsObjectAllocated(liVolumeIndex),
               "mVolumePool.IsObjectAllocated( liVolumeIndex )");
    mVolumePool.FreeObject(liVolumeIndex);
}

// h:421 (inlined into VolumeManager::GetRwVolume @0x828C5F04-0x828C5F40):
//     lwz    r31, 0x14(r3)                ; the caller's miVolumeStoreIndex
//     cmplwi r31, 0x11FF ; bgt -> return 0    (UNSIGNED: catches both < 0 -- i.e. the
//                                              KI_INVALID_VOLUME_INDEX sentinel -- and
//                                              >= 4608)
//     bl     ObjectPool<VolumeSlot,4608,int>::IsObjectAllocated ; beq -> return 0
//     bl     ObjectPool<VolumeSlot,4608,int>::operator[]        ; tail-returned
// ⚠️ The console inlined this into its single caller, so the SOURCE-LEVEL side of the
// range test (here vs. in GetRwVolume) is not separately attested. Both placements are
// behaviour-identical for that one caller, and the DWARF gives this method a
// plain `int` index parameter with no assert of its own, which is what a gating
// (rather than asserting) test looks like -- unlike its RemoveVolume sibling above.
template <s32 tiCapacity>
const VolRef::Volume* VolumeStore<tiCapacity>::GetVolume(s32 liVolumeIndex) const
{
    if (static_cast<u32>(liVolumeIndex) >= static_cast<u32>(tiCapacity))
    {
        return NULL;
    }
    if (!mVolumePool.IsObjectAllocated(liVolumeIndex))
    {
        return NULL;
    }
    return mVolumePool[liVolumeIndex].GetVolume();
}

// h:350 @ 0x828C5A98 (store-for-store). Returns the serialised resource descriptor
// (rw::BaseResourceDescriptors<5>, five {u32 size, u32 align} entries) for the collision
// volume referenced by lpVolume. The X360 body reads the volume's RW collision type via
// the pointer at VolRef::Volume+0x40 (lwz r11,0x40(r5); lwz r11,0(r11)) and switches on
// it: the six primitive volume types (1..6) all size to a single {0x60, 0x10} block; any
// other type trips the "Volume type not supported" assert and falls back to the identity
// {0, 1}. Entries 1..4 are always {0, 1}. DWARF (CgsVolumeStore.h:350) attests this method
// NON-const.
template <s32 tiCapacity>
CgsResource::ResourceDescriptor
VolumeStore<tiCapacity>::GetVolumeResourceDescriptor(const VolRef::Volume* lpVolume)
{
    // Attested double indirection on the console: `Volume::vTable` @ VolRef::Volume+0x40
    // is the per-TYPE descriptor pointer and the RW collision volume-type enum is its
    // first dword (lwz r11,0x40 ; lwz r11,0(r11)). HOST REPRESENTATION (2026-08-18, wave
    // Q5 integration): the +0x40 slot holds the 4-byte type enum itself (an x64 pointer
    // would overlap +0x44; see SDKs/EATech/rwcollision/volume_debug_access.h), so the
    // console's second load is the identity here. (VolRef::Volume is opaque in this TU,
    // so read through raw bytes -- the documented serialised-data exception; the typed
    // form is rw::collision::Volume::GetType().)
    const s32 liRwVolumeType =
        *reinterpret_cast<const s32*>(reinterpret_cast<const u8*>(lpVolume) + 0x40);

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

// h:386. The primitive-volume predicate, read out of the copy ReplaceVolume inlined:
// the RW collision-volume type comes from the double indirection at VolRef::Volume+0x40
// (lwz r11,0x40; lwz r11,0(r11); on the host the slot is the enum itself) and the block is primitive when (type-1) <= 4, i.e.
// types 1..5 (SPHERE, CAPSULE, TRIANGLE, BOX, CYLINDER) -- NOT the aggregate type 6.
// ⚠️ De-inlined into its DWARF-declared home (h:386) as the pure predicate its name
// states; the console folded it into ReplaceVolume, so which side of that boundary the
// "Aggregate volumes are not supported" assert stood on is NOT separately attested. Both
// placements are behaviour-identical (the assert is a non-gating tripwire and the caller
// returns false either way); it is kept at the call site because the message is about the
// STORE refusing the volume, not about the test.
template <s32 tiCapacity>
bool VolumeStore<tiCapacity>::IsPrimitiveVolume(const VolRef::Volume* lpVolume) const
{
    // Host: the +0x40 slot IS the type enum (see GetVolumeResourceDescriptor above).
    const s32 liRwVolumeType =
        *reinterpret_cast<const s32*>(reinterpret_cast<const u8*>(lpVolume) + 0x40);

    return static_cast<u32>(liRwVolumeType - 1) <= 4u;
}

// h:318 @ 0x828CC1D8 (store-for-store). Overwrite the pooled slot at liVolumeIndex with a
// fresh copy of lpVolume. The X360 body inlines both IsPrimitiveVolume and
// VolumeSlot::SetVolume (neither has its own ledger symbol); both are called by name here.
// mVolumePool is the store's first member (offset 0), so the X360 passes `this` straight
// into the pool call.
template <s32 tiCapacity>
bool VolumeStore<tiCapacity>::ReplaceVolume(s32 liVolumeIndex, const VolRef::Volume* lpVolume)
{
    CGS_ASSERT(liVolumeIndex >= 0 && liVolumeIndex < tiCapacity,
               "liVolumeIndex >= 0 && liVolumeIndex < BufferSize");

    if (!IsPrimitiveVolume(lpVolume))
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

    mVolumePool[liVolumeIndex].SetVolume(lpVolume, liVolumeSize);
    return true;
}

// Explicit instantiations for the 4608-slot store (the X360 ledger symbols + the
// header-inline members VolumeManager's TU calls across the TU boundary).
template void VolumeStore<4608>::Construct();
template bool VolumeStore<4608>::Prepare();
template void VolumeStore<4608>::Clear();
template s32  VolumeStore<4608>::AddVolume(const VolRef::Volume*);
template void VolumeStore<4608>::RemoveVolume(s32);
template bool VolumeStore<4608>::ReplaceVolume(s32, const VolRef::Volume*);
template const VolRef::Volume* VolumeStore<4608>::GetVolume(s32) const;
template CgsResource::ResourceDescriptor
VolumeStore<4608>::GetVolumeResourceDescriptor(const VolRef::Volume*);
template bool VolumeStore<4608>::IsPrimitiveVolume(const VolRef::Volume*) const;

}
