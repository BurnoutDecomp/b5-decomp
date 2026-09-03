#ifndef PARTICLE_DESCRIPTION_RESOURCE_TYPE_H
#define PARTICLE_DESCRIPTION_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"
#include "GameSource/Effects/Particles/BrnParticleDescription.h"   // BrnParticle::ParticleDescription

namespace BrnParticle
{
// ============================================================================
// BrnParticle::ParticleDescriptionCollection -- the serialised .lef import table
// PARTICLES.BUNDLE carries, and the table BrnParticle::ParticleModule::StartLionEffect
// @0x82289F50 searches by effect-name hash.
//
// LAYOUT, from the three handlers below (which are the only code that shapes it):
//   Serialise @0x826782B8      lpDst[1] = lpSrc[1] (the count); lpDst[0] = lpDst + 2, i.e.
//                              the table pointer is written to point at +8; then `count`
//                              4-byte entries are copied there.
//   FixUp @0x8267DF40          `*(u32*)res += (u32)res` -- the +0 word is stored
//                              file-relative and rebased in place, so it stays a 32-bit
//                              slot on the host (the project's low-4 GB convention).
//   GetImportPointer @0x826757E8  entry i is `*(u32*)(table + 4*i)` and its patch offset is
//                              `4*(i + 2)` -- confirming 4-byte entries starting at +8.
//   StartLionEffect @0x82289F50   `v8 = *collection; v9 = *(collection + 4);` then walks
//                              `**v11` (entry i's first word == ParticleDescription::
//                              muNameHash) with a stride of one entry.
//
//   +0x00  u32 muEntries   -- byte address of the entry table (== this + 8 after FixUp)
//   +0x04  u32 muCount     -- number of entries
//   +0x08  u32 [muCount]   -- one import slot per .lef, each a ParticleDescription*
//
// The entries are IMPORT SLOTS: the bundle loader patches each one with the resolved
// address of the ParticleDescription resource it names, so after fix-up every slot is a
// (below-4 GB) ParticleDescription pointer. They are 32-bit on the host by construction --
// widening them would break the serialised layout the porter emits.
// ============================================================================
struct ParticleDescriptionCollection
{
    u32 muEntries;   // +0x00
    u32 muCount;     // +0x04
    // +0x08: u32 maEntries[muCount] -- reached through muEntries, not by member, because
    // the array is variable-length serialised data.

    u32 GetCount() const { return muCount; }

    // Entry luIndex as the ParticleDescription the import fix-up resolved it to.
    ParticleDescription* GetDescription(u32 luIndex) const
    {
        const u32* lpTable = reinterpret_cast<const u32*>(static_cast<uintptr_t>(muEntries));
        return reinterpret_cast<ParticleDescription*>(static_cast<uintptr_t>(lpTable[luIndex]));
    }
};

// Resource-type handlers for particle descriptions, deriving from CgsResource::Type.
// GetTypeID/FixUp/GetImportPointer/DeSerialise are virtual overrides; Serialise is
// the type's own virtual. Base/signatures recovered from the DecFIGS DWARF
// (ParticleDescriptionResourceType.h).
class ParticleDescriptionCollectionResourceType : public CgsResource::Type
{
public:
    uint32_t GetTypeID() const override;
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const override;
    void     GetImportPointer(const void* lpResource, uint32_t luIndex, uint32_t* lpuOffset, const void** lppValue) const override;
    virtual void* Serialise(const void* lpResource, const rw::Resource& lrDest) const;
};

class ParticleDescriptionResourceType : public CgsResource::Type
{
public:
    uint32_t GetTypeID() const override;

    // ⭐ ADDED 2026-09-03. This override was MISSING, and its absence is why every .lef
    // failed to load: without it the base no-op FixUp runs, the definition slot keeps the
    // file's offset 16, and cLionFX::BinLoad is handed a pointer to address 16.
    //
    // It is not in the ledger under this class's name because ICF folded it: the X360
    // vtable settles it. ParticleDescriptionResourceType's vtable is at 0x820A1694 (its
    // GetTypeID 0x82675858 sits at +0x40 of the 0x820A16D4 window) and the sibling
    // ParticleDescriptionCollectionResourceType's is 0x34 words earlier; the two have the
    // SAME shape, and the slot four words after GetTypeID -- the collection's FixUp slot,
    // 0x8267DF40 -- holds 0x8267DF60 for the description type. 0x8267DF60 is exported
    // under the name of the class it was folded with, BrnProgression::
    // ProfileUpgradeResourceType::FixUp, and its whole body is `*(result + 4) += result;`
    // -- i.e. rebase the SECOND word, which is exactly the description's blob slot.
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const override;

    bool     DeSerialise(void* lpResource) const override;
    virtual void* Serialise(const void* lpResource, const rw::Resource& lrDest) const;
};
}

#endif
