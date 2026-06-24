#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"
#include "GameShared/GameClasses/RenderWare/FixableVolume.h"   // BrnPhysics::Props::FixableVolume::FixUp/FixDown
#include "rw/rwcore_structs.h"                                  // rw::Resource

// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   BrnPhysics::Props::PropPhysicsDataHeader::FixUp   @ 0x8267F570
//   BrnPhysics::Props::PropPhysicsDataHeader::FixDown @ 0x8267F658
//
// Both walk the three stored pointer arrays and relocate them against the load base
// held in rw::Resource::m_baseResources[0] (asm: r29/r7 = *(&lBaseResource) — the first
// word of the Resource). FixUp resolves serialised OFFSETS into runtime POINTERS by
// adding the base; FixDown reverses it by subtracting the base. The element-record inner
// pointer fields the asm also relocates (PropTypeData +0x3C/+0x40, PropPartTypeData +0x24)
// are touched by their console byte offsets (serialised-payload raw-offset precedent): the
// element types' full member layout is not modelled, only the relocation slots the asm
// reads/writes.
//
// COUNTS (asm-pinned): the prop-type loop bounds on muNumberOfPropTypes (header +0), the
// part-type loop on muNumberOfPartTypes (header +8), the volume loop on
// muNumberOfVolumeTypes (header +4). The three array base offsets in the asm are
// mapPropTypes @ +0x10, mapPropPartTypes @ +0x7E0, mapVolumeTypes @ +0xC90 — which pins
// mapPropPartTypes to 300 entries (0xC90-0x7E0 = 1200 = 300*4). See flagged_type_changes.

namespace BrnPhysics
{
namespace Props
{
    // Console byte offsets of the inner relocation slots the asm rewrites. These are
    // pointer-valued fields inside the serialised element records (relocated alongside the
    // array pointers themselves), addressed raw because the element layouts are not modelled.
    static const u32 KU_PROP_TYPE_RELOC0 = 0x3C;   // PropTypeData +0x3C
    static const u32 KU_PROP_TYPE_RELOC1 = 0x40;   // PropTypeData +0x40
    static const u32 KU_PART_TYPE_RELOC0 = 0x24;   // PropPartTypeData +0x24

    // @ 0x8267F570 — serialised offsets -> runtime pointers (add load base).
    void PropPhysicsDataHeader::FixUp(const rw::Resource& lBaseResource)
    {
        const intptr_t liBase = reinterpret_cast<intptr_t>(lBaseResource.m_baseResources[0]);

        // mapPropTypes[]: rebase the array pointer, then its two inner reloc slots.
        for (uint32_t luIndex = 0; luIndex < muNumberOfPropTypes; ++luIndex)
        {
            u8* lpType = reinterpret_cast<u8*>(reinterpret_cast<intptr_t>(mapPropTypes[luIndex]) + liBase);
            mapPropTypes[luIndex] = reinterpret_cast<PropTypeData*>(lpType);

            *reinterpret_cast<intptr_t*>(lpType + KU_PROP_TYPE_RELOC0) += liBase;
            *reinterpret_cast<intptr_t*>(lpType + KU_PROP_TYPE_RELOC1) += liBase;
        }

        // mapPropPartTypes[]: rebase the array pointer, then its one inner reloc slot.
        for (uint32_t luIndex = 0; luIndex < muNumberOfPartTypes; ++luIndex)
        {
            u8* lpPart = reinterpret_cast<u8*>(reinterpret_cast<intptr_t>(mapPropPartTypes[luIndex]) + liBase);
            mapPropPartTypes[luIndex] = reinterpret_cast<PropPartTypeData*>(lpPart);

            *reinterpret_cast<intptr_t*>(lpPart + KU_PART_TYPE_RELOC0) += liBase;
        }

        // mapVolumeTypes[]: rebase the array pointer, then run the per-volume FixUp.
        for (uint32_t luIndex = 0; luIndex < muNumberOfVolumeTypes; ++luIndex)
        {
            rw::collision::Volume* lpVolume = reinterpret_cast<rw::collision::Volume*>(
                reinterpret_cast<intptr_t>(mapVolumeTypes[luIndex]) + liBase);
            mapVolumeTypes[luIndex] = lpVolume;

            reinterpret_cast<FixableVolume*>(lpVolume)->FixUp();
        }
    }

    // @ 0x8267F658 — runtime pointers -> serialised offsets (subtract load base).
    void PropPhysicsDataHeader::FixDown(const rw::Resource& lBaseResource)
    {
        const intptr_t liBase = reinterpret_cast<intptr_t>(lBaseResource.m_baseResources[0]);

        // mapPropTypes[]: rebase the two inner reloc slots (still pointers), then the array
        // pointer. The asm reads the inner slots, stores (value - base), then subtracts base
        // from the array pointer last.
        for (uint32_t luIndex = 0; luIndex < muNumberOfPropTypes; ++luIndex)
        {
            u8* lpType = reinterpret_cast<u8*>(mapPropTypes[luIndex]);

            *reinterpret_cast<intptr_t*>(lpType + KU_PROP_TYPE_RELOC0) -= liBase;
            *reinterpret_cast<intptr_t*>(lpType + KU_PROP_TYPE_RELOC1) -= liBase;

            mapPropTypes[luIndex] = reinterpret_cast<PropTypeData*>(
                reinterpret_cast<intptr_t>(mapPropTypes[luIndex]) - liBase);
        }

        // mapPropPartTypes[]: rebase the one inner reloc slot, then the array pointer.
        for (uint32_t luIndex = 0; luIndex < muNumberOfPartTypes; ++luIndex)
        {
            u8* lpPart = reinterpret_cast<u8*>(mapPropPartTypes[luIndex]);

            *reinterpret_cast<intptr_t*>(lpPart + KU_PART_TYPE_RELOC0) -= liBase;

            mapPropPartTypes[luIndex] = reinterpret_cast<PropPartTypeData*>(
                reinterpret_cast<intptr_t>(mapPropPartTypes[luIndex]) - liBase);
        }

        // mapVolumeTypes[]: run the per-volume FixDown on the live pointer, then subtract base.
        for (uint32_t luIndex = 0; luIndex < muNumberOfVolumeTypes; ++luIndex)
        {
            reinterpret_cast<FixableVolume*>(mapVolumeTypes[luIndex])->FixDown();

            mapVolumeTypes[luIndex] = reinterpret_cast<rw::collision::Volume*>(
                reinterpret_cast<intptr_t>(mapVolumeTypes[luIndex]) - liBase);
        }
    }
}
}
