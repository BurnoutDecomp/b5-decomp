#ifndef BRN_PROFILE_UPGRADE_RESOURCE_TYPE_H
#define BRN_PROFILE_UPGRADE_RESOURCE_TYPE_H

#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h" // CgsResource::Type (base + rw::Resource fwd)

// =============================================================================
// BrnProfileUpgradeResourceType.h  (OWNING HEADER for BrnProgression::ProfileUpgradeResourceType)
//
// Resource-type HANDLER for the serialised "profile upgrade" resource. Reconstructed from
// BURNOUT_X360_ARTIST.XEX (the X360 baked the source path
// "SharedClasses/Progression/BrnProfileUpgradeResourceType.cpp" into GetImportPointer's assert).
//
//   ProfileUpgradeResourceType::GetSerialisedResourceDescriptor @0x8267D870
//   ProfileUpgradeResourceType::FixUp                           @0x8267DF60
//   ProfileUpgradeResourceType::FixDown                         @0x8267DF70
//   ProfileUpgradeResourceType::GetImportPointer                @0x82676BA8
//
// These are VIRTUAL overrides of the CgsResource::Type vtable (GetSerialisedResourceDescriptor
// slot 1, FixDown slot 3, FixUp slot 4, GetImportPointer slot 8 -- see CgsResourceType.h).
//
// The serialised resource body is one main-memory block whose first u32 is an element count and
// whose second u32 (+4) is a SELF-RELATIVE table offset. FixUp converts that offset to an
// absolute pointer (offset + record base); FixDown is the inverse. The X360 uses the record base
// itself (lpResource) as the relocation delta, NOT the rw::Resource load base, so the fixup
// virtuals ignore their rw::Resource& argument (which Hex-Rays dropped from the 1-arg pseudocode).
// =============================================================================

namespace BrnProgression
{

class ProfileUpgradeResourceType : public CgsResource::Type
{
public:
    // X360 0x8267D870. The serialised descriptor is a single main-memory block: entry[0] =
    // { m_size = (elementCount + 1) * 8, m_alignment = 16 } (elementCount = *(u32*)lpResource),
    // the remaining four entries = { m_size = 0, m_alignment = 1 }.
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;

    // X360 0x8267DF60. Relocate-on-load: convert the self-relative table offset at +4 into an
    // absolute pointer by adding the record base (`*(lpResource + 4) += lpResource`).
    void FixUp(void* lpResource, const rw::Resource& lrResource) const override;

    // X360 0x8267DF70. Relocate-for-save: the inverse (`*(lpResource + 4) -= lpResource`).
    void FixDown(void* lpResource, const rw::Resource& lrResource) const override;

    // X360 0x82676BA8. Import-pointer enumeration is not implemented for this resource type --
    // the X360 body unconditionally fires the "NOT IMPLEMENTED YET" assert and returns.
    void GetImportPointer(const void* lpResource, u32 luIndex, u32* lpuOffset, const void** lppValue) const override;
};

}

#endif // BRN_PROFILE_UPGRADE_RESOURCE_TYPE_H
