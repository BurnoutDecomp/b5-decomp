// ============================================================================
// ParticleWaveForm.cpp -- cParticleWaveForm / cParticleWaveFormTable bodies.
//
// Bodied store-for-store from the X360 asm. Ledger functions recon'd here:
//   cParticleWaveForm::Evaluate(f32) const          @0x82909DE8
//   cParticleWaveFormTable::Init                     @0x8290E7F0
//   cParticleWaveFormTable::NoiseTableBuild          @0x82909F68
//   cParticleWaveFormTable::SawToothInvTableBuild    @0x82909FC0
//   cParticleWaveFormTable::SinTableBuild            @0x8290A008
//   cParticleWaveFormTable::TriangleTableBuild       @0x8290A0A8
//
// SawToothTableBuild / SquareTableBuild are inlined into Init in the X360 build
// (the 0x800 and 0x2000 fill loops in Init's asm) -- they are reproduced inline
// here exactly as the asm fills them, not as separate calls.
//
// The float magic constants are taken directly from the build-loop arithmetic:
//   1/511 == 0.0019569471f   (flt_820FEDA0)
//   1.0f  (flt_82001C98) / 0.0f (flt_82001CC0) / 0.5f (flt_82001DA0)
//   2*PI  == 6.2831855f      (flt_82001C94)
//   1/256 == 0.00390625f     (flt_820B1B64)
//   noise scale == 0.000015259022f (flt_8207AD48) ; 511.0f (flt_82013F98)
// These are derived from the algorithm itself, not asserted as external rodata.
//
// dep_flags: none new -- reuses committed Lion sibling cParticleRandomSeed.
// ============================================================================

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleWaveForm.h"

#include <cmath>

// ParticleWaveForm.h:174 singleton storage definition.
cParticleWaveFormTable cParticleWaveFormTable::mSingleton;

namespace
{
    const f32 KF_INV_511   = 0.0019569471f;     // 1/511
    const f32 KF_INV_256   = 0.00390625f;       // 1/256
    const f32 KF_TWO_PI    = 6.2831855f;
    const f32 KF_NOISE_SCL = 0.000015259022f;
    const f32 KF_511       = 511.0f;
}

// cParticleWaveFormTable::GetTable @ inline
//
// Maps an mType selector to the lookup table base the Evaluate switch samples:
//   the singleton table block base is mNoiseTable, and the switch indexes
//   off it (case3->Square @0x2000, case4->SawTooth @0x800, case5->SawToothInv
//   @0x1000, case6->Sin @0x1800, case7->Triangle @0x2800). GetTable exposes the
//   same block by logical index for callers that want a raw table pointer.
f32* cParticleWaveFormTable::GetTable(u32 auIndex)
{
    switch (auIndex)
    {
        case E_TABLE_NOISE:        return mNoiseTable;
        case E_TABLE_SAWTOOTH:     return mSawToothTable;
        case E_TABLE_SAWTOOTH_INV: return mSawToothInvTable;
        case E_TABLE_SIN:          return mSinTable;
        case E_TABLE_SQUARE:       return mSquareTable;
        case E_TABLE_TRIANGLE:     return mTriangleTable;
        default:                   return mNoiseTable;
    }
}

// cParticleWaveFormTable::GetCos @ inline
//
// Quarter-phase shift into the sin table (cos(x) == sin(x + pi/2); the sin table
// spans one full period over 512 entries, so +128 entries == +pi/2).
f32 cParticleWaveFormTable::GetCos(s32 aiIndex)
{
    return mSinTable[(aiIndex + (KU_TABLE_SIZE / 4)) & (KU_TABLE_SIZE - 1)];
}

// cParticleWaveForm::Evaluate(f32) const @ 0x82909DE8
//
// X360 sequence reproduced:
//   phase = (mFreq * time + mPhase) * 511.0; idx = (s32)phase (truncate toward 0)
//   switch (mType):
//     1: v = NoiseTable[idx & 0x1FF]                        (4*idx & 0x7FC)
//     2: lerp NoiseTable[idx]..[idx+1] by frac(phase)
//     3: v = SquareTable[idx & 0x1FF]
//     4: v = SawToothTable[idx & 0x1FF]
//     5: v = SawToothInvTable[idx & 0x1FF]
//     6: v = SinTable[idx & 0x1FF]
//     7: v = TriangleTable[idx & 0x1FF]
//     default: v = 1.0
//   out = mAmp * v + mBase
//   out = fsel(out - mClampMax, mClampMax, out)  -> min(out, mClampMax)
//   out = fsel(out - mClampMin, out, mClampMin)  -> max(out, mClampMin)
f32 cParticleWaveForm::Evaluate(f32 afTime) const
{
    cParticleWaveFormTable* lpTable = cParticleWaveFormTable::GetMe();

    const f32 lfPhase = (mFreq * afTime + mPhase) * KF_511;   // fmadds then *511
    const s32 liIndex = static_cast<s32>(lfPhase);            // fctiwz (truncate)
    const s32 liMask  = cParticleWaveFormTable::KU_TABLE_SIZE - 1;   // 0x1FF (0x7FC>>2)

    f32 lfVal;
    switch (mType)
    {
        case E_TYPE_NOISE:
            lfVal = lpTable->GetNoiseTable()[liIndex & liMask];
            break;
        case E_TYPE_NOISE_LERP:
        {
            const f32* lpNoise = lpTable->GetNoiseTable();
            const f32  lf0     = lpNoise[liIndex & liMask];
            const f32  lf1     = lpNoise[(liIndex + 1) & liMask];
            const f32  lfFrac  = lfPhase - static_cast<f32>(liIndex);
            lfVal = (lf1 - lf0) * lfFrac + lf0;               // fmadds
            break;
        }
        case E_TYPE_SQUARE:
            lfVal = lpTable->GetSquareTable()[liIndex & liMask];
            break;
        case E_TYPE_SAWTOOTH:
            lfVal = lpTable->GetSawToothTable()[liIndex & liMask];
            break;
        case E_TYPE_SAWTOOTH_INV:
            lfVal = lpTable->GetSawToothInvTable()[liIndex & liMask];
            break;
        case E_TYPE_SIN:
            lfVal = lpTable->GetSinTable()[liIndex & liMask];
            break;
        case E_TYPE_TRIANGLE:
            lfVal = lpTable->GetTriangleTable()[liIndex & liMask];
            break;
        default:
            lfVal = 1.0f;
            break;
    }

    f32 lfOut = mAmp * lfVal + mBase;                          // fmadds f12,f11,f12,f10
    // fsel f0, (out-max), max, out : out>=max ? max : out  == min(out,max)
    lfOut = (lfOut - mClampMax >= 0.0f) ? mClampMax : lfOut;
    // fsel f1, (out-min), out, min : out>=min ? out : min   == max(out,min)
    lfOut = (lfOut - mClampMin >= 0.0f) ? lfOut : mClampMin;
    return lfOut;
}

// cParticleWaveFormTable::NoiseTableBuild @ 0x82909F68
//
// 512-entry pseudo-random noise via a 32-bit LCG seeded with 0x12345678,
// recurrence v = 69069*v + 41 (0x10DCD, 0x29). Each entry stores the UNSIGNED
// low 16 bits of the value-before-update (X360 `clrlwi r7,r10,16` == luRand &
// 0xFFFF, range [0,65535]) scaled by ~1/65536 -> table values in [0,1). The X360
// keeps the running value in a single 32-bit register; the stored sample is the
// value-before-update.
void cParticleWaveFormTable::NoiseTableBuild()
{
    f32* lpDst = mNoiseTable;
    u32  luRand = 0x12345678u;
    s32  liCount = KU_TABLE_SIZE;
    do
    {
        const u32 luSample = luRand & 0xFFFFu;   // clrlwi r7,r10,16 -> unsigned low 16 bits
        --liCount;
        luRand = 69069u * luRand + 41u;
        *lpDst++ = static_cast<f32>(luSample) * KF_NOISE_SCL;
    }
    while (liCount != 0);
}

// cParticleWaveFormTable::SawToothTableBuild (inlined in Init @0x8290E808)
//   mSawToothTable[i] = i / 511.0
void cParticleWaveFormTable::SawToothTableBuild()
{
    f32* lpDst = mSawToothTable;
    for (s32 i = 0; i < KU_TABLE_SIZE; ++i)
        *lpDst++ = static_cast<f32>(i) * KF_INV_511;
}

// cParticleWaveFormTable::SawToothInvTableBuild @ 0x82909FC0
//   mSawToothInvTable[i] = -((i / 511.0) - 1.0)  ==  1.0 - i/511
void cParticleWaveFormTable::SawToothInvTableBuild()
{
    f32* lpDst = mSawToothInvTable;
    for (s32 i = 0; i < KU_TABLE_SIZE; ++i)
        *lpDst++ = -(static_cast<f32>(i) * KF_INV_511 - 1.0f);   // fnmsubs
}

// cParticleWaveFormTable::SinTableBuild @ 0x8290A008
//   mSinTable[i] = sin((i / 511.0) * 2pi) * 0.5 + 0.5
void cParticleWaveFormTable::SinTableBuild()
{
    f32* lpDst = mSinTable;
    for (s32 i = 0; i < KU_TABLE_SIZE; ++i)
    {
        const f32 lfArg = static_cast<f32>(i) * KF_INV_511 * KF_TWO_PI;
        *lpDst++ = static_cast<f32>(std::sin(lfArg)) * 0.5f + 0.5f;   // fmadds f0,f0,0.5,0.5
    }
}

// cParticleWaveFormTable::SquareTableBuild (inlined in Init @0x8290E868)
//   mSquareTable[i] = (i < 256) ? 1.0 : 0.0
void cParticleWaveFormTable::SquareTableBuild()
{
    f32* lpDst = mSquareTable;
    for (s32 i = 0; i < KU_TABLE_SIZE; ++i)
        *lpDst++ = (i < KU_HALF_SIZE) ? 1.0f : 0.0f;
}

// cParticleWaveFormTable::TriangleTableBuild @ 0x8290A0A8
//   i <  256: mTriangleTable[i] = i / 256.0
//   i >= 256: mTriangleTable[i] = -(((i-256) / 256.0) - 1.0) == 1 - (i-256)/256
void cParticleWaveFormTable::TriangleTableBuild()
{
    f32* lpDst = mTriangleTable;
    for (s32 i = 0; i < KU_TABLE_SIZE; ++i)
    {
        f32 lfVal;
        if (i >= KU_HALF_SIZE)
            lfVal = -(static_cast<f32>(i - KU_HALF_SIZE) * KF_INV_256 - 1.0f);  // fnmsubs
        else
            lfVal = static_cast<f32>(i) * KF_INV_256;
        *lpDst++ = lfVal;
    }
}

// cParticleWaveFormTable::Init @ 0x8290E7F0
//
// Builds all six tables. The sawtooth (0x800) and square (0x2000) fills are
// emitted inline in the X360 Init body; the other four are out-of-line calls.
// Order reproduced: Noise, (inline)SawTooth, SawToothInv, Sin, (inline)Square,
// Triangle.
void cParticleWaveFormTable::Init()
{
    NoiseTableBuild();                              // bl NoiseTableBuild

    // inline sawtooth fill: mSawToothTable[i] = i * (1/511)   (this+0x800)
    {
        f32* lpDst = mSawToothTable;
        for (s32 i = 0; i < KU_TABLE_SIZE; ++i)
            *lpDst++ = static_cast<f32>(i) * KF_INV_511;
    }

    SawToothInvTableBuild();                        // bl SawToothInvTableBuild
    SinTableBuild();                                // bl SinTableBuild

    // inline square fill: mSquareTable[i] = i<256 ? 1.0 : 0.0  (this+0x2000)
    {
        f32* lpDst = mSquareTable;
        for (s32 i = 0; i < KU_TABLE_SIZE; ++i)
            *lpDst++ = (i < KU_HALF_SIZE) ? 1.0f : 0.0f;
    }

    TriangleTableBuild();                           // bl TriangleTableBuild
}
