// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleBehaviour.cpp
//
// cParticleBehaviour runtime: the build/compile + serialise/relocate path for one
// Lion (eauk_lion) particle-behaviour node. Sibling to the committed Lion homes
// LionSerialiser.* and LionTokeniser.* (whose DataStore / EndianTwiddle this TU
// calls into by name).
//
// Reconstructed store-for-store from the X360 asm for:
//   cParticleBehaviour::Build              @ 0x8290AFE8
//   cParticleBehaviour::BuildColourSteps   @ 0x829094F0
//   cParticleBehaviour::CompileBaseVariance@ 0x82909170
//   cParticleBehaviour::Delocate           @ 0x8290C9E0
//   cParticleBehaviour::GetSerialiseSize   @ 0x8290CCD0
//   cParticleBehaviour::Relocate           @ 0x8290CC48
//   cParticleBehaviour::Serialise          @ 0x8290F098
// ============================================================================

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleBehaviour.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionSerialiser.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionTokeniser.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// ----------------------------------------------------------------------------
// The two Lion token tables the Delocate/endian path walks:
//   off_82F36A40 -- the cParticleWaveForm member token table (per wave-form).
//   off_82F36A38 -- the cParticleBehaviour member token table (the node itself).
// HOMED 2026-09-03: both are now real data in LionParticleParser.cpp, transcribed
// from the X360 .rdata, under the names the DecFIGS DWARF gives them
// (LionParticleParser.cpp:132-141). They used to be `extern` under INVENTED names
// with no definition anywhere in the tree, so this TU could never link.
// ----------------------------------------------------------------------------
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionParticleParser.h"

// ----------------------------------------------------------------------------
// small store-for-store helpers (a cVector lane-set is one aligned vector store on
// X360; reproduced here by named lane so no raw-offset access is needed).
// ----------------------------------------------------------------------------
namespace
{
    inline void SetVec(cVector& arV, f32 aX, f32 aY, f32 aZ, f32 aW)
    {
        arV.x = aX;
        arV.y = aY;
        arV.z = aZ;
        arV.w = aW;
    }

    inline void ZeroVec(cVector& arV)
    {
        SetVec(arV, 0.0f, 0.0f, 0.0f, 0.0f);
    }

    inline void SetColour(cColour8& arC, u8 aValue)
    {
        arC.r = aValue;
        arC.g = aValue;
        arC.b = aValue;
        arC.a = aValue;
    }
}

// ----------------------------------------------------------------------------
// cParticleBehaviour::Init  @ 0x82908EA0
//
// Resets a behaviour node to its authoring defaults (called by cParticleEmitter::Init
// via the descriptor). Every store is reproduced from the X360 asm by named member;
// the non-zero defaults (from the pseudocode's literal constants and the resolved
// rodata: flt_82001CC0=0.0, flt_82001C98=1.0, flt_82001DA0=0.5, flt_820054D0=7.0):
//   mPivotPoint            = (0.5, 0.5, 0.5, 0)
//   mSizeXYZBase           = (0.5, 0.5, 0,   0)
//   mVelBase               = (0,   1,   0,   0)
//   mRGBA0 / mRGBA1        = 0x80808080 (mid-grey, all channels 0x80)
//   mColour[]/mColourStepRGBA[] = 0xFFFFFFFF (all channels 0xFF)
//   mEmissionRateBase = 7.0; mLifeBase / mScale / mCellSize / mSizeBase = 1.0
//   mAlphaFadeOut / mTimeScale / mEmitterEndWeight / mEmitterVelWeight = 1.0
//   mEndOnAlphaFade / mEndOnEndAngle = 1.0
// The colour steps loop (asm 0x82909084..0x829090A0) fills all four mColour[] /
// mColourStepRGBA[] with -1 and all four mColourTime[] / mRGBATime[] with 0.
//
// Init does NOT touch mColourStepsRGBAv[], mDivisors[], the alpha-fade reciprocals,
// mBVCompiled, mRadius, mAABBMin/Max or mEmissionRateHasBeenScaled -- those are left
// as-is (Build() / CompileBaseVariance() derive them later), matching the asm.
// ----------------------------------------------------------------------------
void cParticleBehaviour::Init()
{
    ZeroVec(mAccBase);
    ZeroVec(mAccVariance);
    ZeroVec(mAxisBase);
    ZeroVec(mOffsetRotXYZBase);
    ZeroVec(mOffsetRotXYZVariance);
    ZeroVec(mOffsetRotXYZVelBase);
    ZeroVec(mOffsetRotXYZVelVariance);
    ZeroVec(mOffsetRotXYZAccBase);
    ZeroVec(mOffsetRotXYZAccVariance);
    ZeroVec(mRotXYZBase);
    ZeroVec(mRotXYZVariance);
    ZeroVec(mRotXYZVelBase);
    ZeroVec(mRotXYZVelVariance);
    ZeroVec(mRotXYZAccBase);
    ZeroVec(mRotXYZAccVariance);
    SetVec(mPivotPoint, 0.5f, 0.5f, 0.5f, 0.0f);
    ZeroVec(mPosBase);
    ZeroVec(mPosVariance);
    ZeroVec(mRingRadius);
    SetVec(mSizeXYZBase, 0.5f, 0.5f, 0.0f, 0.0f);
    ZeroVec(mSizeXYZVariance);
    ZeroVec(mSizeXYZVelBase);
    ZeroVec(mSizeXYZVelVariance);
    ZeroVec(mSizeXYZAccBase);
    ZeroVec(mSizeXYZAccVariance);
    SetVec(mVelBase, 0.0f, 1.0f, 0.0f, 0.0f);
    ZeroVec(mVelVariance);
    ZeroVec(mRGBADiff);

    // Colour / colour-step tables: four each, colours -> 0xFFFFFFFF, times -> 0.
    for (u32 luIndex = 0; luIndex < KU_COLOUR_STEP_LIMIT; ++luIndex)
    {
        SetColour(mColour[luIndex], 0xFF);
        mColourTime[luIndex] = 0.0f;
        SetColour(mColourStepRGBA[luIndex], 0xFF);
        mRGBATime[luIndex] = 0.0f;
    }
    mColourSteps = 0;

    SetColour(mRGBA0, 0x80);
    SetColour(mRGBA1, 0x80);
    SetColour(mRGBABase, 0x00);
    SetColour(mRGBAVar, 0x00);

    mRibbonParticleCount = 0;
    mEmissionCountClamp = 0;
    mAlphaFadeIn = 0.0f;
    mEmissionCountClampVariance = 0;
    mAlphaFadeOut = 1.0f;
    mRGBAVarianceMode = 0;
    mCellSize = 1.0f;
    mFlags = 0;
    mCloneScaleInTime = 0.0f;
    mpWaveFormAlpha.SetRaw(0);
    mDragFactorVel = 0.0f;
    mpWaveFormRGB.SetRaw(0);
    mDragFactorRot = 0.0f;
    mpWaveFormX.SetRaw(0);
    mDragFactorScale = 0.0f;
    mpWaveFormY.SetRaw(0);
    mDragFactor = 0.0f;
    mpWaveFormZ.SetRaw(0);
    mMass = 0.0f;
    mpNext.SetRaw(0);
    mLifeBase = 1.0f;
    mLifeVariance = 0.0f;
    mEmissionRateBase = 7.0f;
    mEmissionRateVariance = 0.0f;
    mEmitterStartWeight = 0.0f;
    mEmitterEndWeight = 1.0f;
    mEmitterVelWeight = 1.0f;
    mScale = 1.0f;
    mTimeScale = 1.0f;
    mTimeScaleVariance = 0.0f;
    mSizeBase = 1.0f;
    mSizeVariance = 0.0f;
    mSizeVelBase = 0.0f;
    mSizeVelVariance = 0.0f;
    mSizeAccBase = 0.0f;
    mSizeAccVariance = 0.0f;

    mEndOnScale = 0.0f;
    mEndOnStartAngle = 0.0f;
    mEndOnAlphaFade = 1.0f;
    mEndOnEndAngle = 1.0f;
}

// ----------------------------------------------------------------------------
// cParticleBehaviour::BuildColourSteps  @ 0x829094F0
//
// Packs up to four colour "steps" into mColourStepRGBA[] / mColourTime[] from the
// per-channel mColour[]/mColourTime[] entries, gated by the four colour-step
// flags. mColourSteps counts how many steps were emitted.
//
// asm note: the cColour8 step store uses word (mColourSteps + 0x90) = &mColourStepRGBA[]
// (mColourStepRGBA @ word 0x90=144 / 0x240); the f32 time store uses word
// (mColourSteps + 0x94) = &mRGBATime[] (@ word 0x94=148 / 0x250) -- the packed
// destination, NOT the source mColourTime[] (@ word 140 / 0x230). Reconstructed by name.
// ----------------------------------------------------------------------------
void cParticleBehaviour::BuildColourSteps()
{
    mColourSteps = 0;

    if ((mFlags & E_FLAG_COLOUR_STEP0) != 0)
    {
        mColourStepRGBA[mColourSteps] = mColour[0];
        mRGBATime[mColourSteps]       = mColourTime[0];
        ++mColourSteps;
    }
    if ((mFlags & E_FLAG_COLOUR_STEP1) != 0)
    {
        mColourStepRGBA[mColourSteps] = mColour[1];
        mRGBATime[mColourSteps]       = mColourTime[1];
        ++mColourSteps;
    }
    if ((mFlags & E_FLAG_COLOUR_STEP2) != 0)
    {
        mColourStepRGBA[mColourSteps] = mColour[2];
        mRGBATime[mColourSteps]       = mColourTime[2];
        ++mColourSteps;
    }
    if ((mFlags & E_FLAG_COLOUR_STEP3) != 0)
    {
        mColourStepRGBA[mColourSteps] = mColour[3];
        mRGBATime[mColourSteps]       = mColourTime[3];
        ++mColourSteps;
    }
}

// ----------------------------------------------------------------------------
// cParticleBehaviour::CompileBaseVariance  @ 0x82909170
//
// Packs a flag-selected sequence of (base, variance) f32 pairs into mBVCompiled.
// The leading run is unconditional; subsequent blocks are gated by the BV flags.
// Each pair is written as {base, variance} interleaved from the corresponding
// member vectors' x/y/z lanes. mBVCompiled.size is the number of emitted pairs
// (computed from the running write pointer, asm: (p - base) >> 3).
//
// The asm walks a running f32* (r11); reconstructed here through a named cursor
// over mBVCompiled.aData[] so behaviour and pair count match store-for-store.
// ----------------------------------------------------------------------------
void cParticleBehaviour::CompileBaseVariance()
{
    sParticleBehaviourBaseVariancePack* lpPack = mBVCompiled.aData;

    // Unconditional leading run (10 pairs): mass/life, pos, vel, acc.
    lpPack[0].base     = mLifeBase;            // *(this+688)
    lpPack[0].variance = mLifeVariance;        // *(this+692)
    lpPack[1].base     = mPosBase.x;           // *(this+256)
    lpPack[1].variance = mPosVariance.x;       // *(this+272)
    lpPack[2].base     = mPosBase.y;           // *(this+260)
    lpPack[2].variance = mPosVariance.y;       // *(this+276)
    lpPack[3].base     = mPosBase.z;           // *(this+264)
    lpPack[3].variance = mPosVariance.z;       // *(this+280)
    lpPack[4].base     = mVelBase.x;           // *(this+400)
    lpPack[4].variance = mVelVariance.x;       // *(this+416)
    lpPack[5].base     = mVelBase.y;           // *(this+404)
    lpPack[5].variance = mVelVariance.y;       // *(this+420)
    lpPack[6].base     = mVelBase.z;           // *(this+408)
    lpPack[6].variance = mVelVariance.z;       // *(this+424)
    lpPack[7].base     = mAccBase.x;           // *(this+0)
    lpPack[7].variance = mAccVariance.x;       // *(this+16)
    lpPack[8].base     = mAccBase.y;           // *(this+4)
    lpPack[8].variance = mAccVariance.y;       // *(this+20)
    lpPack[9].base     = mAccBase.z;           // *(this+8)
    lpPack[9].variance = mAccVariance.z;       // *(this+24)
    lpPack += 10;

    // E_BV_AXIS (0x10): three fixed (-1, 2) pairs.
    if ((mFlags & E_BV_AXIS) != 0)
    {
        lpPack[0].base = -1.0f;
        lpPack[0].variance = 2.0f;
        lpPack[1].base = -1.0f;
        lpPack[1].variance = 2.0f;
        lpPack[2].base = -1.0f;
        lpPack[2].variance = 2.0f;
        lpPack += 3;
    }

    // E_BV_OFFSETROT (0x20): offset-rotation base/vel/acc (9 pairs).
    if ((mFlags & E_BV_OFFSETROT) != 0)
    {
        lpPack[0].base     = mOffsetRotXYZBase.x;        // *(this+48)
        lpPack[0].variance = mOffsetRotXYZVariance.x;    // *(this+64)
        lpPack[1].base     = mOffsetRotXYZBase.y;        // *(this+52)
        lpPack[1].variance = mOffsetRotXYZVariance.y;    // *(this+68)
        lpPack[2].base     = mOffsetRotXYZBase.z;        // *(this+56)
        lpPack[2].variance = mOffsetRotXYZVariance.z;    // *(this+72)
        lpPack[3].base     = mOffsetRotXYZVelBase.x;     // *(this+80)
        lpPack[3].variance = mOffsetRotXYZVelVariance.x; // *(this+96)
        lpPack[4].base     = mOffsetRotXYZVelBase.y;     // *(this+84)
        lpPack[4].variance = mOffsetRotXYZVelVariance.y; // *(this+100)
        lpPack[5].base     = mOffsetRotXYZVelBase.z;     // *(this+88)
        lpPack[5].variance = mOffsetRotXYZVelVariance.z; // *(this+104)
        lpPack[6].base     = mOffsetRotXYZAccBase.x;     // *(this+112)
        lpPack[6].variance = mOffsetRotXYZAccVariance.x; // *(this+128)
        lpPack[7].base     = mOffsetRotXYZAccBase.y;     // *(this+116)
        lpPack[7].variance = mOffsetRotXYZAccVariance.y; // *(this+132)
        lpPack[8].base     = mOffsetRotXYZAccBase.z;     // *(this+120)
        lpPack[8].variance = mOffsetRotXYZAccVariance.z; // *(this+136)
        lpPack += 9;
    }

    // Rotation block: E_BV_ROT (0x1) -> the Z-column of rot base/vel/acc only
    // (3 pairs: base.z, velbase.z, accbase.z with their variances), else
    // E_BV_ROTVELACC (0x40) -> the full XYZ rot base/vel/acc (9 pairs).
    // (asm: 0x1 branch reads 0x98/0xA8, 0xB8/0xC8, 0xD8/0xE8 = the .z lanes.)
    const u32 luFlags1 = mFlags;
    if ((luFlags1 & E_BV_ROT) != 0)
    {
        lpPack[0].base     = mRotXYZBase.z;       // *(this+0x98)
        lpPack[0].variance = mRotXYZVariance.z;   // *(this+0xA8)
        lpPack[1].base     = mRotXYZVelBase.z;    // *(this+0xB8)
        lpPack[1].variance = mRotXYZVelVariance.z;// *(this+0xC8)
        lpPack[2].base     = mRotXYZAccBase.z;    // *(this+0xD8)
        lpPack[2].variance = mRotXYZAccVariance.z;// *(this+0xE8)
        lpPack += 3;
    }
    else if ((luFlags1 & E_BV_ROTVELACC) != 0)
    {
        lpPack[0].base     = mRotXYZBase.x;       // *(this+0x90)
        lpPack[0].variance = mRotXYZVariance.x;   // *(this+0xA0)
        lpPack[1].base     = mRotXYZBase.y;       // *(this+0x94)
        lpPack[1].variance = mRotXYZVariance.y;   // *(this+0xA4)
        lpPack[2].base     = mRotXYZBase.z;       // *(this+0x98)
        lpPack[2].variance = mRotXYZVariance.z;   // *(this+0xA8)
        lpPack[3].base     = mRotXYZVelBase.x;    // *(this+0xB0)
        lpPack[3].variance = mRotXYZVelVariance.x;// *(this+0xC0)
        lpPack[4].base     = mRotXYZVelBase.y;    // *(this+0xB4)
        lpPack[4].variance = mRotXYZVelVariance.y;// *(this+0xC4)
        lpPack[5].base     = mRotXYZVelBase.z;    // *(this+0xB8)
        lpPack[5].variance = mRotXYZVelVariance.z;// *(this+0xC8)
        lpPack[6].base     = mRotXYZAccBase.x;    // *(this+0xD0)
        lpPack[6].variance = mRotXYZAccVariance.x;// *(this+0xE0)
        lpPack[7].base     = mRotXYZAccBase.y;    // *(this+0xD4)
        lpPack[7].variance = mRotXYZAccVariance.y;// *(this+0xE4)
        lpPack[8].base     = mRotXYZAccBase.z;    // *(this+0xD8)
        lpPack[8].variance = mRotXYZAccVariance.z;// *(this+0xE8)
        lpPack += 9;
    }

    // Size block: first pair (x base/var) always; E_BV_SIZE_FULL (0x80) -> the
    // full size base/vel/acc (9 pairs), else the size-xyz base diagonal (3 pairs,
    // x already written, then y and z bases).
    const u32 luFlags2 = mFlags;
    lpPack[0].base     = mSizeXYZBase.x;          // *(this+304)
    lpPack[0].variance = mSizeXYZVariance.x;      // *(this+320)
    if ((luFlags2 & E_BV_SIZE_FULL) != 0)
    {
        lpPack[1].base     = mSizeXYZBase.y;       // *(this+308)
        lpPack[1].variance = mSizeXYZVariance.y;   // *(this+324)
        lpPack[2].base     = mSizeXYZBase.z;       // *(this+312)
        lpPack[2].variance = mSizeXYZVariance.z;   // *(this+328)
        lpPack[3].base     = mSizeXYZVelBase.x;    // *(this+336)
        lpPack[3].variance = mSizeXYZVelVariance.x;// *(this+352)
        lpPack[4].base     = mSizeXYZVelBase.y;    // *(this+340)
        lpPack[4].variance = mSizeXYZVelVariance.y;// *(this+356)
        lpPack[5].base     = mSizeXYZVelBase.z;    // *(this+344)
        lpPack[5].variance = mSizeXYZVelVariance.z;// *(this+360)
        lpPack[6].base     = mSizeXYZAccBase.x;    // *(this+368)
        lpPack[6].variance = mSizeXYZAccVariance.x;// *(this+384)
        lpPack[7].base     = mSizeXYZAccBase.y;    // *(this+372)
        lpPack[7].variance = mSizeXYZAccVariance.y;// *(this+388)
        lpPack[8].base     = mSizeXYZAccBase.z;    // *(this+376)
        lpPack[8].variance = mSizeXYZAccVariance.z;// *(this+392)
        lpPack += 9;
    }
    else
    {
        lpPack[1].base     = mSizeXYZVelBase.x;    // *(this+336)
        lpPack[1].variance = mSizeXYZVelVariance.x;// *(this+352)
        lpPack[2].base     = mSizeXYZAccBase.x;    // *(this+368)
        lpPack[2].variance = mSizeXYZAccVariance.x;// *(this+384)
        lpPack += 3;
    }

    // RGBA-variance-mode 2 appends a fixed (0, 1) pair.
    if (mRGBAVarianceMode == 2)
    {
        lpPack[0].base = 0.0f;
        lpPack[0].variance = 1.0f;
        lpPack += 1;
    }

    // size = number of emitted pairs = (cursor - aData) (each pack is 2 f32 = 8
    // bytes; asm computes (bytePtr - this - 736) >> 3).
    mBVCompiled.size = static_cast<u32>(lpPack - mBVCompiled.aData);
}

// ----------------------------------------------------------------------------
// cParticleBehaviour::Build  @ 0x8290AFE8
//
// Finalises a behaviour after load: asserts the wave-alpha flag is clear, derives
// the packed colour base/variance/diff from mRGBA0/mRGBA1, builds the colour steps
// and the compiled base-variance table, and precomputes the alpha-fade reciprocals
// and the (near-zero) divisor guard values.
//
// COLOUR DERIVATION (asm 0x8290B02C..0x8290B160, VMX). ⭐ THE TWO SCALE VECTORS THIS
// COMMENT USED TO CALL "unresolved" ARE RECOVERED (2026-09-03), AND RESOLVING THEM TURNED
// UP A REAL DIVERGENCE IN THIS FUNCTION. Both are dynamically-initialised .bss, so a
// literal scan of the image finds only readers; tools/re/findinit.py finds the CRT thunk:
//
//   unk_82FAC100  <- 0x82C4A110: lfs flt_82010C1C ; vspltw ; stvx128
//                    flt_82010C1C == 0x3B808081 == 0.003921568859368563 == 1/255
//   unk_82FAC220  <- 0x82C4A0B0: lfs flt_82010C20 ; vspltw ; stvx128
//                    flt_82010C20 == 0x437F0000 == 255.0
//
// So the console normalises both colours to 0..1, does the min / max / difference there,
// and multiplies back by 255 before re-quantising with `vctuxs` -- which TRUNCATES toward
// zero, after a `vminfp` clamp at 255. That round trip is not the identity, and it is not
// uniformly harmless either. Measured over every input, in exact float arithmetic:
//
//   mRGBABase = trunc(min(c0,c1)/255 * 255) ... 0 of 256 values differ from min(c0,c1).
//               The identity holds, so `min` is a PROVEN-exact reconstruction.
//   mRGBAVar  = trunc((max/255 - min/255) * 255) ... 16,612 of 65,536 channel pairs
//               differ from (max - min), always low by one count -- c0=3, c1=4 gives
//               0.99999994, which truncates to 0, not 1.
//   mRGBADiff = (c1/255 - c0/255) * 255 ... 49,398 of 65,536 pairs differ from (c1 - c0),
//               by about a ulp (c0=0, c1=3 gives 3.000000238).
//
// The previous reconstruction wrote (max - min) and (c1 - c0) directly, so it was right
// for mRGBABase and wrong for the other two. Both are now written as the console's own
// arithmetic. The variance one is behavioural, not cosmetic: it is the per-particle colour
// spread, and a quarter of all channel pairs were getting one extra count of it.
// ----------------------------------------------------------------------------
namespace
{
// unk_82FAC100 / unk_82FAC220, splat across all four lanes. Written as the image's exact
// floats rather than as 1.0f/255.0f, because the truncation above is sensitive to the last
// bit: 0x3B808081 is slightly ABOVE the true 1/255, which is what makes the mRGBABase
// round trip exact.
const f32 KF_COLOUR_U8_TO_UNIT = 0.003921568859368563f;   // 0x3B808081
const f32 KF_COLOUR_UNIT_TO_U8 = 255.0f;                  // 0x437F0000

// `vmulfp128 <255>` + `vminfp <255>` + `vctuxs ...,0`: scale back up, clamp, truncate.
inline u8 PackUnitChannel(f32 afUnit)
{
    f32 lfScaled = afUnit * KF_COLOUR_UNIT_TO_U8;
    if (lfScaled > KF_COLOUR_UNIT_TO_U8)
    {
        lfScaled = KF_COLOUR_UNIT_TO_U8;
    }
    return static_cast<u8>(lfScaled);      // vctuxs rounds toward zero
}
}  // namespace

void cParticleBehaviour::Build()
{
    CGS_ASSERT((mFlags & E_DO_WAVEALPHA) == 0,
               "( mFlags & cParticleBehaviour::eDO_WAVEALPHA ) == 0");

    const cColour8 lC0 = mRGBA0;
    const cColour8 lC1 = mRGBA1;

    // asm 0x8290B0A0..0x8290B0B4 -- both colours expanded to floats and normalised.
    const f32 lfC0[4] = { static_cast<f32>(lC0.r) * KF_COLOUR_U8_TO_UNIT,
                          static_cast<f32>(lC0.g) * KF_COLOUR_U8_TO_UNIT,
                          static_cast<f32>(lC0.b) * KF_COLOUR_U8_TO_UNIT,
                          static_cast<f32>(lC0.a) * KF_COLOUR_U8_TO_UNIT };
    const f32 lfC1[4] = { static_cast<f32>(lC1.r) * KF_COLOUR_U8_TO_UNIT,
                          static_cast<f32>(lC1.g) * KF_COLOUR_U8_TO_UNIT,
                          static_cast<f32>(lC1.b) * KF_COLOUR_U8_TO_UNIT,
                          static_cast<f32>(lC1.a) * KF_COLOUR_U8_TO_UNIT };

    // asm 0x8290B0B8..0x8290B0CC -- vminfp / vmaxfp / vsubfp, all in unit space.
    f32 lfMin[4];
    f32 lfRange[4];
    f32 lfDiff[4];
    for (u32 luChannel = 0; luChannel < 4; ++luChannel)
    {
        const f32 lfA = lfC0[luChannel];
        const f32 lfB = lfC1[luChannel];
        lfMin[luChannel]   = (lfA < lfB) ? lfA : lfB;
        lfRange[luChannel] = ((lfA > lfB) ? lfA : lfB) - lfMin[luChannel];
        lfDiff[luChannel]  = lfB - lfA;
    }

    // asm 0x8290B0C4..0x8290B108 -- min re-packed into mRGBABase.
    mRGBABase.r = PackUnitChannel(lfMin[0]);
    mRGBABase.g = PackUnitChannel(lfMin[1]);
    mRGBABase.b = PackUnitChannel(lfMin[2]);
    mRGBABase.a = PackUnitChannel(lfMin[3]);

    // asm 0x8290B10C..0x8290B154 -- range re-packed into mRGBAVar.
    mRGBAVar.r = PackUnitChannel(lfRange[0]);
    mRGBAVar.g = PackUnitChannel(lfRange[1]);
    mRGBAVar.b = PackUnitChannel(lfRange[2]);
    mRGBAVar.a = PackUnitChannel(lfRange[3]);

    // asm 0x8290B158..0x8290B160 -- the signed difference, scaled back to 0..255 units and
    // left as a float vector (no re-quantisation).
    mRGBADiff.x = lfDiff[0] * KF_COLOUR_UNIT_TO_U8;
    mRGBADiff.y = lfDiff[1] * KF_COLOUR_UNIT_TO_U8;
    mRGBADiff.z = lfDiff[2] * KF_COLOUR_UNIT_TO_U8;
    mRGBADiff.w = lfDiff[3] * KF_COLOUR_UNIT_TO_U8;

    BuildColourSteps();

    // Alpha-fade reciprocals (asm 0x8290B168..0x8290B1C4).
    const f32 lAlphaFadeIn = mAlphaFadeIn;       // *(this+632)
    mZero = 0.0f;                                // *(this+616)
    mAlphaFadeInInv = (lAlphaFadeIn == 0.0f) ? 1.0f : (1.0f / lAlphaFadeIn);

    const f32 lAlphaFadeOut = mAlphaFadeOut;     // *(this+636)
    const f32 lOneMinusFadeOut = 1.0f - lAlphaFadeOut;
    mAlphaFadeOutPlusInvOneMinusAlphaFadeOut = lAlphaFadeOut + lOneMinusFadeOut;
    f32 lInvOneMinusFadeOut = 1.0f;
    if (lOneMinusFadeOut != 0.0f)
        lInvOneMinusFadeOut = 1.0f / lOneMinusFadeOut;
    mNegInvOneMinusAlphaFadeOut = -lInvOneMinusFadeOut;

    // Near-zero divisor guards (asm 0x8290B1C8..0x8290B1D8 store 1e-30 to all four).
    mDivisors[3] = 1.0e-30f;
    mDivisors[2] = 1.0e-30f;
    mDivisors[1] = 1.0e-30f;
    mDivisors[0] = 1.0e-30f;

    CompileBaseVariance();
}

// ----------------------------------------------------------------------------
// cParticleBehaviour::Delocate  @ 0x8290C9E0
//
// Prepares the behaviour for serialisation to a different-endian target. When the
// endian-twiddle flag (aEndianTwiddleFlag) is set, each present wave-form's member
// fields are byte-swapped through the Lion wave-form token table; the wave-form
// and mpNext pointers are then converted from absolute pointers to base-relative
// offsets (delocated); finally, when twiddling, the six pointer-words themselves
// (mpWaveFormX..mpNext) are byte-swapped in place and the behaviour's own member
// fields are twiddled through the behaviour token table.
//
// asm note: the in-place pointer-word byte-swaps at 0x2C8..0x2DC and the behaviour
// twiddle run after the pointer->offset conversion; the behaviour-token twiddle is
// emitted by the EndianTwiddle(&off_82F36A38, this) call. Reconstructed by name.
// ----------------------------------------------------------------------------
void cParticleBehaviour::Delocate(u32 aEndianTwiddleFlag)
{
    const bool lbTwiddle = (aEndianTwiddleFlag != 0);

    if (lbTwiddle && mpWaveFormX)
        gLionParticleParserWaveTokenTable.EndianTwiddle(mpWaveFormX.Get());
    if (lbTwiddle && mpWaveFormY)
        gLionParticleParserWaveTokenTable.EndianTwiddle(mpWaveFormY.Get());
    if (lbTwiddle && mpWaveFormZ)
        gLionParticleParserWaveTokenTable.EndianTwiddle(mpWaveFormZ.Get());
    if (lbTwiddle && mpWaveFormAlpha)
        gLionParticleParserWaveTokenTable.EndianTwiddle(mpWaveFormAlpha.Get());
    if (lbTwiddle && mpWaveFormRGB)
        gLionParticleParserWaveTokenTable.EndianTwiddle(mpWaveFormRGB.Get());

    // Convert each present link to a base-relative byte offset from `this`.
    mpWaveFormX.Delocate(this);
    mpWaveFormY.Delocate(this);
    mpWaveFormZ.Delocate(this);
    mpWaveFormAlpha.Delocate(this);
    mpWaveFormRGB.Delocate(this);
    mpNext.Delocate(this);

    if (lbTwiddle)
    {
        // Twiddle the behaviour's own member image, then byte-swap the six now-
        // offset pointer words in place (big->little, byte-reversed).
        gLionParticleParserBehTokenTable.EndianTwiddle(this);

        u8* lp = reinterpret_cast<u8*>(mpWaveFormX.RawAddress());
        for (u32 luWord = 0; luWord < 6; ++luWord)
        {
            const u8 b0 = lp[0];
            const u8 b1 = lp[1];
            const u8 b2 = lp[2];
            const u8 b3 = lp[3];
            const u32 luSwapped =
                ((((((static_cast<u32>(b0) << 8) | b1) << 8) | b2) << 8) | b3);
            *reinterpret_cast<u32*>(lp) = luSwapped;
            lp += 4;
        }
    }
}

// ----------------------------------------------------------------------------
// cParticleBehaviour::Relocate  @ 0x8290CC48
//
// Inverse of the pointer->offset conversion in Delocate: re-bases each present
// wave-form / next offset back into an absolute pointer relative to `this`.
// ----------------------------------------------------------------------------
void cParticleBehaviour::Relocate()
{
    // asm words 178..183 -- each a 32-bit slot re-based against `this`.
    mpWaveFormX.Relocate(this);
    mpWaveFormY.Relocate(this);
    mpWaveFormZ.Relocate(this);
    mpWaveFormAlpha.Relocate(this);
    mpWaveFormRGB.Relocate(this);
    mpNext.Relocate(this);
}

// ----------------------------------------------------------------------------
// cParticleBehaviour::GetSerialiseSize  @ 0x8290CCD0
//
// Adds this behaviour's serialised size to the serialiser's running data size:
// sizeof(cParticleBehaviour) (1216 bytes / 0x4C0) plus 64 bytes for each present
// wave-form (X/Y/Z/Alpha/RGB). The running total lives in the serialiser member
// reached at +0x14 on X360 (mDataSize).
//
// asm note: `*(a2 + 20)` is the serialiser's mDataSize accumulator. The const-ref
// signature is honoured at the call site; the accumulator is mutated, matching the
// X360 (the parameter is non-const data through the +0x14 store).
// ----------------------------------------------------------------------------
void cParticleBehaviour::GetSerialiseSize(cLionSerialiser& aSer) const
{
    aSer.mDataSize += 1216;
    if (mpWaveFormX)
        aSer.mDataSize += 64;
    if (mpWaveFormY)
        aSer.mDataSize += 64;
    if (mpWaveFormZ)
        aSer.mDataSize += 64;
    if (mpWaveFormAlpha)
        aSer.mDataSize += 64;
    if (mpWaveFormRGB)
        aSer.mDataSize += 64;
}

// ----------------------------------------------------------------------------
// cParticleBehaviour::Serialise  @ 0x8290F098
//
// Copies the behaviour (1216 bytes) into the serialiser's data area via DataStore,
// then copies each present wave-form (56 bytes) and stores the relocated wave-form
// pointer in the just-written copy. Returns the destination copy, or null if this
// is null. mpRGB / mpNext are handled by the descriptor-level Serialise; this only
// rewrites the five wave-form pointers (mpWaveFormX..mpWaveFormRGB) in the copy.
// ----------------------------------------------------------------------------
cParticleBehaviour* cParticleBehaviour::Serialise(cLionSerialiser& aSer) const
{
    cParticleBehaviour* lpCopy =
        reinterpret_cast<cParticleBehaviour*>(aSer.DataStore(this, 1216));

    cParticleWaveForm* lpStoredX = 0;
    if (mpWaveFormX)
        lpStoredX = reinterpret_cast<cParticleWaveForm*>(aSer.DataStore(mpWaveFormX.Get(), 56));
    lpCopy->mpWaveFormX.Set(lpStoredX);

    cParticleWaveForm* lpStoredY = 0;
    if (mpWaveFormY)
        lpStoredY = reinterpret_cast<cParticleWaveForm*>(aSer.DataStore(mpWaveFormY.Get(), 56));
    lpCopy->mpWaveFormY.Set(lpStoredY);

    cParticleWaveForm* lpStoredZ = 0;
    if (mpWaveFormZ)
        lpStoredZ = reinterpret_cast<cParticleWaveForm*>(aSer.DataStore(mpWaveFormZ.Get(), 56));
    lpCopy->mpWaveFormZ.Set(lpStoredZ);

    cParticleWaveForm* lpStoredAlpha = 0;
    if (mpWaveFormAlpha)
        lpStoredAlpha = reinterpret_cast<cParticleWaveForm*>(aSer.DataStore(mpWaveFormAlpha.Get(), 56));
    lpCopy->mpWaveFormAlpha.Set(lpStoredAlpha);

    cParticleWaveForm* lpStoredRGB = 0;
    if (mpWaveFormRGB)
        lpStoredRGB = reinterpret_cast<cParticleWaveForm*>(aSer.DataStore(mpWaveFormRGB.Get(), 56));
    lpCopy->mpWaveFormRGB.Set(lpStoredRGB);

    return lpCopy;
}
