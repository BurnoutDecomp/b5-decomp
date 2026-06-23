#include "SharedClasses/Progression/BrnProfileUpgradeResourceType.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (GetImportPointer NOT-IMPLEMENTED)
#include "rw/rwcore_structs.h"                         // rw::BaseResourceDescriptor[5] (CgsResource::ResourceDescriptor)
#include <cstdint>                                     // uintptr_t (self-relative pointer relocation)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnProgression::ProfileUpgradeResourceType::GetSerialisedResourceDescriptor @0x8267D870
//   BrnProgression::ProfileUpgradeResourceType::FixUp                           @0x8267DF60
//   BrnProgression::ProfileUpgradeResourceType::FixDown                         @0x8267DF70
//   BrnProgression::ProfileUpgradeResourceType::GetImportPointer                @0x82676BA8

namespace BrnProgression
{

// Serialised profile-upgrade record header. The X360 fixup/descriptor bodies only touch the two
// leading u32s: muElementCount at +0 and a self-relative table offset (relocated in place to an
// absolute pointer by FixUp) at +4. Documented serialised-payload raw-offset view of the on-disk
// resource body -- the rest of the block is opaque element data the descriptor sizes but these
// bodies do not interpret.
struct ProfileUpgradeResource
{
    u32 muElementCount;       // +0  element count (descriptor sizes the block as (count+1)*8)
    u32 muTableOffset;        // +4  self-relative table offset <-> absolute pointer (FixUp/FixDown)
};

// X360 0x8267D870. Build the single-block serialised descriptor. entry[0] sizes the whole block
// at (elementCount + 1) * 8 bytes with 16-byte alignment; the four trailing entries are unused
// ({ m_size = 0, m_alignment = 1 }). The X360 forms entry[0]'s 8-byte {size,align} pair via a
// std of a 64-bit value (big-endian +0 = (count+1)*8, +4 = 16), overwriting the {0,1} it first
// wrote into that slot.
CgsResource::ResourceDescriptor
ProfileUpgradeResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
{
    const ProfileUpgradeResource* lpUpgrade = static_cast<const ProfileUpgradeResource*>(lpResource);
    const u32 luBlockSize = (lpUpgrade->muElementCount + 1u) * 8u;   // X360: ((*lpResource) + 1) << 3

    CgsResource::ResourceDescriptor lDescriptor;
    rw::BaseResourceDescriptor* lpEntries = lDescriptor.m_baseResourceDescriptors;

    lpEntries[0].m_size      = luBlockSize;
    lpEntries[0].m_alignment = 16u;
    for (u32 luEntry = 1u; luEntry < 5u; ++luEntry)
    {
        lpEntries[luEntry].m_size      = 0u;
        lpEntries[luEntry].m_alignment = 1u;
    }
    return lDescriptor;
}

// X360 0x8267DF60. Relocate-on-load: the table offset stored at +4 is self-relative to the record
// base, so add the record base to turn it into an absolute pointer (`*(lpResource + 4) += lpResource`).
// The X360 uses lpResource itself as the relocation delta and ignores the rw::Resource load base.
void ProfileUpgradeResourceType::FixUp(void* lpResource, const rw::Resource& /*lrResource*/) const
{
    ProfileUpgradeResource* lpUpgrade = static_cast<ProfileUpgradeResource*>(lpResource);
    lpUpgrade->muTableOffset += static_cast<u32>(reinterpret_cast<uintptr_t>(lpResource));
}

// X360 0x8267DF70. Relocate-for-save: the inverse of FixUp -- subtract the record base to turn the
// absolute pointer back into a self-relative offset (`*(lpResource + 4) -= lpResource`).
void ProfileUpgradeResourceType::FixDown(void* lpResource, const rw::Resource& /*lrResource*/) const
{
    ProfileUpgradeResource* lpUpgrade = static_cast<ProfileUpgradeResource*>(lpResource);
    lpUpgrade->muTableOffset -= static_cast<u32>(reinterpret_cast<uintptr_t>(lpResource));
}

// X360 0x82676BA8. Import-pointer enumeration is unimplemented for this resource type: the X360
// body unconditionally fires the "NOT IMPLEMENTED YET" assert and returns without touching the
// out-parameters.
void ProfileUpgradeResourceType::GetImportPointer(const void* /*lpResource*/, u32 /*luIndex*/,
                                                  u32* /*lpuOffset*/, const void** /*lppValue*/) const
{
    CGS_ASSERT(false, "NOT IMPLEMENTED YET");
}

}
