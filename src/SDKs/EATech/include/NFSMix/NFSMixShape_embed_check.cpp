#include "SDKs/EATech/include/NFSMix/NFSMixShape.hpp"

// Compile-gate the NFSMixShape pitch helpers (rodata-backed).
static int NFSMixShape_embed_check()
{
    int liPos[2] = { 0x2000, 0x6000 };
    return NFSMixShape::GetIntPitchMultFromCents(0)
         + static_cast<int>(NFSMixShape::GetPitchMultFromCents(0))
         + NFSMixShape::GetdBFromQ15(0x4000)
         + NFSMixShape::GetQ15FromHundredthsdB(-600)
         + NFSMixShape::GetCurveOutput(NFSMixShape::SHAPE_DWN_EQPWR, 0x4000, 0)
         + NFSMixShape::GetCentsFromPitchMult(1.0f)
         + static_cast<int>(NFSMixShape::GetFloatCurveOutput(0, 0.5f))
         + NFSMixShape::GetAzimShapeOutput(0, 0, liPos, 0x2000);
}
