#pragma once

// ===========================================================================
//  NFSMixShape -- the NFS audio math helpers (pitch / dB / Q15 / curve conversions),
//  free functions in a global namespace (mangled ...@NFSMixShape@@). Used by the
//  Nicotine dynamic mixer (DMixIO::GetDMixOutput's DMX_PITCH branch) and the NFS mix
//  per-frame DSP.
//
//  These were RODATA-BLOCKED: the conversion tables are not in the X360 per-function
//  export. They are now extracted directly from the BURNOUT_X360_ARTIST decrypted XEX
//  rodata (vaddr -> file offset = 0x3000 + (vaddr - 0x82000000)); see NFSMixShape.cpp.
//
//  Bodied so far: GetPitchMultFromCents @0x82B44F30 + GetIntPitchMultFromCents
//  @0x82B45690 (pitch tables flt_82F8778C / flt_82F877C0 recovered + verified as
//  2^(i/12) / 2^(i/1200)). The dB/Q15/curve helpers (GetdBFromQ15 @0x82B45060,
//  GetQ15FromHundredthsdB @0x82B44ED0, GetCurveOutput @0x82B45160) use the u16 tables
//  at 0x82F862C8.. (extracted/located; bodied in a follow-on -- intricate indexing).
// ===========================================================================

namespace NFSMixShape
{
    // 2^(cents/1200) pitch multiplier (single precision, per the X360 fmuls/fdivs).
    float GetPitchMultFromCents(int liCents);    // @0x82B44F30
    // GetPitchMultFromCents(sext16(liCents)) * 4096, truncated to int (fixed-point 1.0=4096).
    int   GetIntPitchMultFromCents(int liCents); // @0x82B45690

    // Amplitude (Q15) -> dB in hundredths (<=0). @0x82B45060
    int   GetdBFromQ15(int liQ15);
    // dB (hundredths) -> Q15 gain (>=0 dB clamps to 0x7FFF). @0x82B44ED0
    int   GetQ15FromHundredthsdB(int liHundredthsDb);

    // eMIXTABLEID curve shapes (0..9) for GetCurveOutput.
    enum eMixTableId
    {
        SHAPE_DWN_EQPWR = 0, SHAPE_UP_EQPWR = 1,
        SHAPE_DWN_EQPWR_SQ = 2, SHAPE_UP_EQPWR_SQ = 3,
        SHAPE_DWN_ONE_MIN_EQPWR = 4, SHAPE_UP_ONE_MIN_EQPWR = 5,
        SHAPE_DWN_ONE_MIN_EQPWR_SQ = 6, SHAPE_UP_ONE_MIN_EQPWR_SQ = 7,
        SHAPE_DWN_LINEAR = 8, SHAPE_UP_LINEAR = 9, MAX_SHAPE_TYPES = 10,
    };
    // Evaluate curve `liId` at position liPos [0,0x7FFF]; lbDb -> dB hundredths else Q15. @0x82B45160
    int   GetCurveOutput(int liId, int liPos, char lbDb);

    // Pitch multiplier -> cents (inverse of GetPitchMultFromCents). @0x82B455B8
    int   GetCentsFromPitchMult(float lfMult);
    // GetCurveOutput with a [0,1] float position and return. @0x82B456E0
    float GetFloatCurveOutput(int liId, float lfPos01);
    // Blend two curve(liId) evaluations at lpPos[0]/lpPos[1] by liWeight. @0x82B45550
    int   GetAzimShapeOutput(int liId, int liId2, int* lpPos, int liWeight);
}
