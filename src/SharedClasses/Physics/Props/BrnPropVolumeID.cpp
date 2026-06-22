#include "SharedClasses/Physics/Props/BrnPropEntityID.h"

// Out-of-line body for BrnWorld::PropVolumeID::Set, reconstructed from
// BURNOUT_X360_ARTIST.XEX (0x822B7C20). PropVolumeID's home + layout banner live in
// BrnPropEntityID.h (flagged additive grow), alongside its siblings PropEntityID /
// PropVolumeInstanceID. Store order + bit-fields reproduced exactly from the X360 asm.
//
// HOST-vs-X360 NOTE: the asm reads the owner via the big-endian word
// (`ld r11,0(this); srdi r11,r11,16; clrlwi r11,r11,24` == bits [16..23]) and the
// pack/store uses `std r11,0(this)` of the full 64-bit word. The owner read here is
// expressed as the endian-independent field extraction VolumeId::GetOwner()
// (== (mId >> 16) & 0xFF). The compared constant is E_ENTITYTYPE_PROP (== 3), as the asm.

namespace BrnWorld
{
    // Owner tripwire: baked "mVolumeId.GetOwner() == E_ENTITYTYPE_PROP"
    // (..\\..\\..\\SharedClasses\\Physics/Props/BrnPropEntityID.h:387). Reads the
    // pre-existing owner byte (the asm checks it BEFORE the new word is stored).
    void PropVolumeID::AssertIsProp() const
    {
        CGS_ASSERT(mVolumeId.GetOwner() == E_ENTITYTYPE_PROP,
                   "mVolumeId.GetOwner() == E_ENTITYTYPE_PROP");
    }

    // 0x822B7C20: assert owner (existing byte) == 3, then the two payload-range
    // tripwires, then build the packed word and store it. Asm builds
    //   mId = ((luPropType << 6) & 0xFFC0) | (luVolumeNumber & 0xFF) | 0x30000
    // (`clrlslwi` type<<6 keeping 10 bits, `clrldi` vol&0xFF, `oris ...,3` == owner<<16).
    void PropVolumeID::Set(u16 luPropType, u8 luVolumeNumber)
    {
        AssertIsProp();                                                              // line 387
        CGS_ASSERT((luPropType >> KU_BITS_FOR_TYPE_ID) == 0,
                   "(luPropType >> KU_BITS_FOR_TYPE_ID) == 0");                       // line 311
        CGS_ASSERT((luVolumeNumber >> KU_BITS_FOR_VOLUME_NUMBER) == 0,
                   "(luVolumeNumber >> KU_BITS_FOR_VOLUME_NUMBER) == 0");             // line 312

        mVolumeId.mId =
              ((static_cast<u32>(luPropType) << KU_TYPE_ID_OFFSET) & KU_TYPE_ID_MASK)
            | (static_cast<u32>(luVolumeNumber) & 0xFFu)
            | (static_cast<u32>(E_ENTITYTYPE_PROP) << CgsSceneManager::VolumeId::KU_OWNER_BASE);
    }
}
