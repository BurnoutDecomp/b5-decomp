#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"
#include "GameShared/GameClasses/RenderWare/FixableVolume.h"   // BrnPhysics::Props::FixableVolume::FixUp/FixDown
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"  // CgsResource::GetLoadBase64
#include "rw/rwcore_structs.h"                                  // rw::Resource

// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   BrnPhysics::Props::PropPhysicsDataHeader::FixUp   @ 0x8267F570
//   BrnPhysics::Props::PropPhysicsDataHeader::FixDown @ 0x8267F658
//
// Both walk the three stored pointer arrays and relocate them against the load base held in
// rw::Resource::m_baseResources[0] (asm: r29 = *(&lBaseResource) -- the first word of the
// Resource). FixUp resolves serialised OFFSETS into runtime POINTERS by adding the base;
// FixDown reverses it by subtracting the base.
//
// COUNTS + ARRAY BASES (asm-pinned): the prop-type loop bounds on muNumberOfPropTypes
// (header +0) walking the array at +0x10, the part-type loop on muNumberOfPartTypes
// (header +8) walking +0x7E0, the volume loop on muNumberOfVolumeTypes (header +4) walking
// +0xC90. Those three bases are what fix the array bounds at 500 / 300 / 2048.
//
// DE-INLINING: the X360 compiler folded PropTypeData::FixUp/FixDown and
// PropPartTypeData::FixUp/FixDown (both DWARF-declared) into this body -- they surface here
// only as the raw relocations at PropTypeData +0x3C/+0x40 and PropPartTypeData +0x24. They
// are outlined back to the records that own them (bodied in BrnPhysicsPropTypeData.cpp), so
// every field is now reached BY NAME. The earlier revision poked those CONSOLE byte offsets
// through `intptr_t*` casts, which on x64 wrote 8-byte pointers at +0x3C/+0x40 -- straddling
// mfSphereRadius and landing maParts one slot short of where the host compiler puts it.

namespace BrnPhysics
{
namespace Props
{
    // @ 0x8267F570 -- serialised offsets -> runtime pointers (add load base).
    void PropPhysicsDataHeader::FixUp(const rw::Resource& lBaseResource)
    {
        const uintptr_t luBase = CgsResource::GetLoadBase64(lBaseResource);

        // mapPropTypes[]: rebase the array slot, then the record's own two pointers.
        for (uint32_t luIndex = 0; luIndex < muNumberOfPropTypes; ++luIndex)
        {
            mapPropTypes[luIndex] = reinterpret_cast<PropTypeData*>(
                reinterpret_cast<uintptr_t>(mapPropTypes[luIndex]) + luBase);

            mapPropTypes[luIndex]->FixUp(lBaseResource);
        }

        // mapPropPartTypes[]: rebase the array slot, then the record's own pointer.
        for (uint32_t luIndex = 0; luIndex < muNumberOfPartTypes; ++luIndex)
        {
            mapPropPartTypes[luIndex] = reinterpret_cast<PropPartTypeData*>(
                reinterpret_cast<uintptr_t>(mapPropPartTypes[luIndex]) + luBase);

            mapPropPartTypes[luIndex]->FixUp(lBaseResource);
        }

        // mapVolumeTypes[]: rebase the array slot, then run the per-volume FixUp (which
        // swaps the on-disk VOLUMETYPE enum for the shared runtime handler pointer).
        for (uint32_t luIndex = 0; luIndex < muNumberOfVolumeTypes; ++luIndex)
        {
            rw::collision::Volume* lpVolume = reinterpret_cast<rw::collision::Volume*>(
                reinterpret_cast<uintptr_t>(mapVolumeTypes[luIndex]) + luBase);
            mapVolumeTypes[luIndex] = lpVolume;

            reinterpret_cast<FixableVolume*>(lpVolume)->FixUp();
        }
    }

    // @ 0x8267F658 -- runtime pointers -> serialised offsets (subtract load base).
    void PropPhysicsDataHeader::FixDown(const rw::Resource& lBaseResource)
    {
        const uintptr_t luBase = CgsResource::GetLoadBase64(lBaseResource);

        // The X360 order is inner-slots-first, then the array slot (the array slot is still
        // a live pointer while the record is being read through it).
        for (uint32_t luIndex = 0; luIndex < muNumberOfPropTypes; ++luIndex)
        {
            mapPropTypes[luIndex]->FixDown(lBaseResource);

            mapPropTypes[luIndex] = reinterpret_cast<PropTypeData*>(
                reinterpret_cast<uintptr_t>(mapPropTypes[luIndex]) - luBase);
        }

        for (uint32_t luIndex = 0; luIndex < muNumberOfPartTypes; ++luIndex)
        {
            mapPropPartTypes[luIndex]->FixDown(lBaseResource);

            mapPropPartTypes[luIndex] = reinterpret_cast<PropPartTypeData*>(
                reinterpret_cast<uintptr_t>(mapPropPartTypes[luIndex]) - luBase);
        }

        for (uint32_t luIndex = 0; luIndex < muNumberOfVolumeTypes; ++luIndex)
        {
            reinterpret_cast<FixableVolume*>(mapVolumeTypes[luIndex])->FixDown();

            mapVolumeTypes[luIndex] = reinterpret_cast<rw::collision::Volume*>(
                reinterpret_cast<uintptr_t>(mapVolumeTypes[luIndex]) - luBase);
        }
    }
}
}
