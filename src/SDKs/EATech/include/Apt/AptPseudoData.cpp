#include "SDKs/EATech/include/Apt/AptPseudoData.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82AD9910
//   AptPseudoData_t::AptPseudoData_t(this, source, characterId, data)
//
// Behaviour-faithful to the X360 pseudocode:
//     this->mpData          = data;
//     this->mi16CharacterId = characterId;
//     this->muxFlags        = source->muxFlags;          // [+0x14] <- src[+4]
//     this->mi16Depth       = source->mi16Depth;         // [+0x1A] <- src[+56]
//     // optional fields, gated on the source's PlaceObject flag bits:
//     this->mpMatrix         = (flags & 0x04) ? &source->maMatrix          : 0;
//     this->mpColorTransform = (flags & 0x08) ? &source->maColorTransform  : 0;
//     this->miClipActionValue= (flags & 0x80) ?  source->miClipActionValue : 0;
//     this->mfRatio          = (flags & 0x10) ?  source->mfRatio           : 0.0;
//
// The flag bits match the SWF/GFx PlaceObject2 tag layout
// (0x04 HasMatrix, 0x08 HasColorTransform, 0x10 HasRatio, 0x80 HasClipActions).
// The matrix/cxform fields are captured by ADDRESS (pointers into the source
// record), the ratio/clip fields are captured by VALUE.
AptPseudoData_t::AptPseudoData_t(const AptPlaceObjectInfo_t* lpSource,
                                 s16                          li16CharacterId,
                                 void*                        lpData)
{
    const u32 luxFlags = lpSource->muxFlags;

    mpData          = lpData;
    mi16CharacterId = li16CharacterId;
    muxFlags        = luxFlags;
    mi16Depth       = lpSource->mi16Depth;

    mpMatrix         = (luxFlags & 0x04u)
                           ? const_cast<u8*>(lpSource->maMatrix)
                           : nullptr;
    mpColorTransform = (luxFlags & 0x08u)
                           ? const_cast<u8*>(lpSource->maColorTransform)
                           : nullptr;
    miClipActionValue = (luxFlags & 0x80u) ? lpSource->miClipActionValue : 0;
    mfRatio           = (luxFlags & 0x10u) ? lpSource->mfRatio           : 0.0f;
}
