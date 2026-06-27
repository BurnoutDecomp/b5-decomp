#include "SDKs/EATech/include/NFSMix/NFSMixShape.hpp"

// Compile-gate the NFSMixShape pitch helpers (rodata-backed).
static int NFSMixShape_embed_check()
{
    return NFSMixShape::GetIntPitchMultFromCents(0) + static_cast<int>(NFSMixShape::GetPitchMultFromCents(0));
}
