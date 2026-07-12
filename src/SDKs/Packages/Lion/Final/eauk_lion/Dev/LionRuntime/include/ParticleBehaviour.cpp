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
// HONEST PLACEHOLDERS: the two Lion token tables the Delocate/endian path walks.
//   off_82F36A40 -- the cParticleWaveForm member token table (per wave-form).
//   off_82F36A38 -- the cParticleBehaviour member token table (the node itself).
// These are external cLionTokenTable instances owned by the Lion token-table
// registration unit (not yet homed in the project). Declared extern here so the
// EndianTwiddle calls reconstruct faithfully; flagged for proper homing.
// ----------------------------------------------------------------------------
extern const cLionTokenTable gLionParticleWaveFormTokenTable;  // off_82F36A40
extern const cLionTokenTable gLionParticleBehaviourTokenTable; // off_82F36A38

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
    mpWaveFormAlpha = 0;
    mDragFactorVel = 0.0f;
    mpWaveFormRGB = 0;
    mDragFactorRot = 0.0f;
    mpWaveFormX = 0;
    mDragFactorScale = 0.0f;
    mpWaveFormY = 0;
    mDragFactor = 0.0f;
    mpWaveFormZ = 0;
    mMass = 0.0f;
    mpNext = 0;
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
// COLOUR DERIVATION (asm 0x8290B02C..0x8290B160, VMX): mRGBA0 and mRGBA1 are
// expanded to per-channel floats and scaled by a constant colour-normalisation
// vector (X360 unk_82FAC100) and a re-pack vector (unk_82FAC220). The asm then
// computes, per channel: min(c0,c1) -> mRGBABase, (max-min) -> mRGBAVar (both
// re-packed to u8 via vctuxs), and (c1-c0)*repack -> mRGBADiff (kept as a float
// vector). The two VMX constant pools are not in the exports, so the exact scale
// factors are unresolved; the per-channel min / range / signed-diff semantics are
// reconstructed faithfully by named channel. FLAGGED: resolve unk_82FAC100 /
// unk_82FAC220 when the Lion colour constant pool is homed.
// ----------------------------------------------------------------------------
void cParticleBehaviour::Build()
{
    CGS_ASSERT((mFlags & E_DO_WAVEALPHA) == 0,
               "( mFlags & cParticleBehaviour::eDO_WAVEALPHA ) == 0");

    const cColour8 lC0 = mRGBA0;
    const cColour8 lC1 = mRGBA1;

    // Per-channel min (base), range (variance) and signed difference (diff). The
    // X360 scales through the colour-normalisation / re-pack constant vectors
    // before re-quantising; without the pooled constants the scaling factor is
    // unresolved, so the channel-wise relationship is reconstructed directly.
    mRGBABase.r = (lC0.r < lC1.r) ? lC0.r : lC1.r;
    mRGBABase.g = (lC0.g < lC1.g) ? lC0.g : lC1.g;
    mRGBABase.b = (lC0.b < lC1.b) ? lC0.b : lC1.b;
    mRGBABase.a = (lC0.a < lC1.a) ? lC0.a : lC1.a;

    const u8 lMaxR = (lC0.r > lC1.r) ? lC0.r : lC1.r;
    const u8 lMaxG = (lC0.g > lC1.g) ? lC0.g : lC1.g;
    const u8 lMaxB = (lC0.b > lC1.b) ? lC0.b : lC1.b;
    const u8 lMaxA = (lC0.a > lC1.a) ? lC0.a : lC1.a;
    mRGBAVar.r = static_cast<u8>(lMaxR - mRGBABase.r);
    mRGBAVar.g = static_cast<u8>(lMaxG - mRGBABase.g);
    mRGBAVar.b = static_cast<u8>(lMaxB - mRGBABase.b);
    mRGBAVar.a = static_cast<u8>(lMaxA - mRGBABase.a);

    mRGBADiff.x = static_cast<f32>(lC1.r) - static_cast<f32>(lC0.r);
    mRGBADiff.y = static_cast<f32>(lC1.g) - static_cast<f32>(lC0.g);
    mRGBADiff.z = static_cast<f32>(lC1.b) - static_cast<f32>(lC0.b);
    mRGBADiff.w = static_cast<f32>(lC1.a) - static_cast<f32>(lC0.a);

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

    if (lbTwiddle && mpWaveFormX != 0)
        gLionParticleWaveFormTokenTable.EndianTwiddle(mpWaveFormX);
    if (lbTwiddle && mpWaveFormY != 0)
        gLionParticleWaveFormTokenTable.EndianTwiddle(mpWaveFormY);
    if (lbTwiddle && mpWaveFormZ != 0)
        gLionParticleWaveFormTokenTable.EndianTwiddle(mpWaveFormZ);
    if (lbTwiddle && mpWaveFormAlpha != 0)
        gLionParticleWaveFormTokenTable.EndianTwiddle(mpWaveFormAlpha);
    if (lbTwiddle && mpWaveFormRGB != 0)
        gLionParticleWaveFormTokenTable.EndianTwiddle(mpWaveFormRGB);

    // Convert each present pointer to a base-relative byte offset from `this`.
    u8* lpBase = reinterpret_cast<u8*>(this);
    if (mpWaveFormX != 0)
        mpWaveFormX = reinterpret_cast<cParticleWaveForm*>(reinterpret_cast<u8*>(mpWaveFormX) - lpBase);
    if (mpWaveFormY != 0)
        mpWaveFormY = reinterpret_cast<cParticleWaveForm*>(reinterpret_cast<u8*>(mpWaveFormY) - lpBase);
    if (mpWaveFormZ != 0)
        mpWaveFormZ = reinterpret_cast<cParticleWaveForm*>(reinterpret_cast<u8*>(mpWaveFormZ) - lpBase);
    if (mpWaveFormAlpha != 0)
        mpWaveFormAlpha = reinterpret_cast<cParticleWaveForm*>(reinterpret_cast<u8*>(mpWaveFormAlpha) - lpBase);
    if (mpWaveFormRGB != 0)
        mpWaveFormRGB = reinterpret_cast<cParticleWaveForm*>(reinterpret_cast<u8*>(mpWaveFormRGB) - lpBase);
    if (mpNext != 0)
        mpNext = reinterpret_cast<cParticleBehaviour*>(reinterpret_cast<u8*>(mpNext) - lpBase);

    if (lbTwiddle)
    {
        // Twiddle the behaviour's own member image, then byte-swap the six now-
        // offset pointer words in place (big->little, byte-reversed).
        gLionParticleBehaviourTokenTable.EndianTwiddle(this);

        u8* lp = reinterpret_cast<u8*>(&mpWaveFormX);
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
    u8* lpBase = reinterpret_cast<u8*>(this);
    if (mpWaveFormX != 0)
        mpWaveFormX = reinterpret_cast<cParticleWaveForm*>(lpBase + reinterpret_cast<uintptr_t>(mpWaveFormX));
    if (mpWaveFormY != 0)
        mpWaveFormY = reinterpret_cast<cParticleWaveForm*>(lpBase + reinterpret_cast<uintptr_t>(mpWaveFormY));
    if (mpWaveFormZ != 0)
        mpWaveFormZ = reinterpret_cast<cParticleWaveForm*>(lpBase + reinterpret_cast<uintptr_t>(mpWaveFormZ));
    if (mpWaveFormAlpha != 0)
        mpWaveFormAlpha = reinterpret_cast<cParticleWaveForm*>(lpBase + reinterpret_cast<uintptr_t>(mpWaveFormAlpha));
    if (mpWaveFormRGB != 0)
        mpWaveFormRGB = reinterpret_cast<cParticleWaveForm*>(lpBase + reinterpret_cast<uintptr_t>(mpWaveFormRGB));
    if (mpNext != 0)
        mpNext = reinterpret_cast<cParticleBehaviour*>(lpBase + reinterpret_cast<uintptr_t>(mpNext));
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
    if (mpWaveFormX != 0)
        aSer.mDataSize += 64;
    if (mpWaveFormY != 0)
        aSer.mDataSize += 64;
    if (mpWaveFormZ != 0)
        aSer.mDataSize += 64;
    if (mpWaveFormAlpha != 0)
        aSer.mDataSize += 64;
    if (mpWaveFormRGB != 0)
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
    if (mpWaveFormX != 0)
        lpStoredX = reinterpret_cast<cParticleWaveForm*>(aSer.DataStore(mpWaveFormX, 56));
    lpCopy->mpWaveFormX = lpStoredX;

    cParticleWaveForm* lpStoredY = 0;
    if (mpWaveFormY != 0)
        lpStoredY = reinterpret_cast<cParticleWaveForm*>(aSer.DataStore(mpWaveFormY, 56));
    lpCopy->mpWaveFormY = lpStoredY;

    cParticleWaveForm* lpStoredZ = 0;
    if (mpWaveFormZ != 0)
        lpStoredZ = reinterpret_cast<cParticleWaveForm*>(aSer.DataStore(mpWaveFormZ, 56));
    lpCopy->mpWaveFormZ = lpStoredZ;

    cParticleWaveForm* lpStoredAlpha = 0;
    if (mpWaveFormAlpha != 0)
        lpStoredAlpha = reinterpret_cast<cParticleWaveForm*>(aSer.DataStore(mpWaveFormAlpha, 56));
    lpCopy->mpWaveFormAlpha = lpStoredAlpha;

    cParticleWaveForm* lpStoredRGB = 0;
    if (mpWaveFormRGB != 0)
        lpStoredRGB = reinterpret_cast<cParticleWaveForm*>(aSer.DataStore(mpWaveFormRGB, 56));
    lpCopy->mpWaveFormRGB = lpStoredRGB;

    return lpCopy;
}
