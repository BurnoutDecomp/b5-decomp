#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"  // CgsResource::GetLoadBase64
#include "rw/rwcore_structs.h"                                            // rw::Resource

// Out-of-line body for BrnPhysics::Props::PropTypeData::IsLamppost.
//
// Reconstructed store-for-store from the X360 ARTIST asm at 0x822A1A00: load the
// 32-bit id at console offset 0x58 -- muSceneUriId, per the DWARF member order (an earlier
// pass named it muGraphicsId from what this body does with it; GetGraphicsId() is retained
// as an alias accessor) -- and return true when it matches any of eight fixed lamppost
// prop ids (the literal `lis r10,N; ori r10,r10,M` constants in the disasm). Any other id
// returns false. The pseudocode renders the constants in decimal; the hex literals from
// the disasm are noted alongside each.
//
// Callers (X360 xrefs): BrnWorld::PropEntityInstance::InitialiseFromData and
// BrnPhysics::Props::PropManager::SetupAndValidatePropContact.

namespace BrnPhysics
{
namespace Props
{
    bool PropTypeData::IsLamppost() const
    {
        switch (muSceneUriId)
        {
            case 331611u:  // 0x50F5B
            case 428420u:  // 0x68984
            case 428477u:  // 0x689BD
            case 428772u:  // 0x68AE4
            case 428484u:  // 0x689C4
            case 428491u:  // 0x689CB
            case 428364u:  // 0x6894C
            case 428388u:  // 0x68964
                return true;
            default:
                return false;
        }
    }

    // ---- record relocations -------------------------------------------------------
    // DWARF BrnPhysicsPropTypeData.h:85/:88 and BrnPhysicsPropPartTypeData.h:72/:75. The
    // X360 compiler folded all four bodies into PropPhysicsDataHeader::FixUp @0x8267F570 /
    // FixDown @0x8267F658, where they appear as the relocations at PropTypeData +0x3C/+0x40
    // and PropPartTypeData +0x24 (`lwz r6,0x3C(r11); lwz r7,0x40(r11); add ...; stw ...`).
    // Outlined back here so the header walks the records by name.
    //
    // Neither slot is null-checked: the console adds the base unconditionally. A prop type
    // with muNumberOfParts == 0 carries the bake tool's uninitialised sentinel in maParts
    // (0xFDD1F800 in the shipped resource, 191 of 219 records) rather than null, and the
    // rebased garbage is never dereferenced because the part loops are bounded by the count.
    // That is faithful X360 behaviour, so it is reproduced rather than "fixed".

    void PropTypeData::FixUp(const ::rw::Resource& lrBaseResource)
    {
        const uintptr_t luBase = CgsResource::GetLoadBase64(lrBaseResource);

        maCollisionVolumes = reinterpret_cast< ::rw::collision::Volume*>(
            reinterpret_cast<uintptr_t>(maCollisionVolumes) + luBase);
        maParts = reinterpret_cast<PropPartTypeData*>(
            reinterpret_cast<uintptr_t>(maParts) + luBase);
    }

    void PropTypeData::FixDown(const ::rw::Resource& lrBaseResource)
    {
        const uintptr_t luBase = CgsResource::GetLoadBase64(lrBaseResource);

        maCollisionVolumes = reinterpret_cast< ::rw::collision::Volume*>(
            reinterpret_cast<uintptr_t>(maCollisionVolumes) - luBase);
        maParts = reinterpret_cast<PropPartTypeData*>(
            reinterpret_cast<uintptr_t>(maParts) - luBase);
    }

    void PropPartTypeData::FixUp(const ::rw::Resource& lrBaseResource)
    {
        maCollisionVolumes = reinterpret_cast< ::rw::collision::Volume*>(
            reinterpret_cast<uintptr_t>(maCollisionVolumes)
            + CgsResource::GetLoadBase64(lrBaseResource));
    }

    void PropPartTypeData::FixDown(const ::rw::Resource& lrBaseResource)
    {
        maCollisionVolumes = reinterpret_cast< ::rw::collision::Volume*>(
            reinterpret_cast<uintptr_t>(maCollisionVolumes)
            - CgsResource::GetLoadBase64(lrBaseResource));
    }
}
}
