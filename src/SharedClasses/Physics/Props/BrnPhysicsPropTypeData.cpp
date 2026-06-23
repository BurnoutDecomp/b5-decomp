#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"

// Out-of-line body for BrnPhysics::Props::PropTypeData::IsLamppost.
//
// Reconstructed store-for-store from the X360 ARTIST asm at 0x822A1A00: load the
// 32-bit graphics/name id at console offset 0x58, and return true when it matches any
// of eight fixed lamppost prop-graphics ids (the literal `lis r10,N; ori r10,r10,M`
// constants in the disasm). Any other id returns false. The pseudocode renders the
// constants in decimal; the hex literals from the disasm are noted alongside each.
//
// Callers (X360 xrefs): BrnWorld::PropEntityInstance::InitialiseFromData and
// BrnPhysics::Props::PropManager::SetupAndValidatePropContact.

namespace BrnPhysics
{
namespace Props
{
    bool PropTypeData::IsLamppost() const
    {
        switch (muGraphicsId)
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
}
}
