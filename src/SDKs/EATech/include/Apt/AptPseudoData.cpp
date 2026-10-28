#include "SDKs/EATech/include/Apt/AptPseudoData.h"

// ARTIST @0x82AD9910: conditional captures from the resident place command.
AptPseudoData_t::AptPseudoData_t(const AptPlaceObjectInfo_t* lpSource,
                                 s16                          li16CharacterId,
                                 void*                        lpData)
{
    // ARTIST addi r11,r4,4: the body immediately follows the tag.
    const AptPlaceObjectBody_t* const lpBody = lpSource->GetBody();

    const u32 luxFlags = lpBody->muxFlags;

    mpData          = lpData;
    mi16CharacterId = li16CharacterId;
    muxFlags        = luxFlags;
    // ARTIST LWZ command+0x38, then STH snapshot+0x1A.
    mi16Depth       = static_cast<s16>(lpBody->miClipDepth);

    mpMatrix         = (luxFlags & 0x04u)
                           ? const_cast<u8*>(lpBody->maMatrix)
                           : 0;
    mpColorTransform = (luxFlags & 0x08u)
                           ? const_cast<u8*>(lpBody->maColorTransform)
                           : 0;
    // Borrow the same resident authored handler block as initial placement.
    mpClipActions = (luxFlags & 0x80u) ? lpBody->mpClipActions : 0;
    mfRatio           = (luxFlags & 0x10u) ? lpBody->mfRatio             : 0.0f;
}
