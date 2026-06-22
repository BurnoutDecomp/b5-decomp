#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleBehaviour.h
//
// cParticleBehaviour -- the Lion (eauk_lion) particle-behaviour runtime descriptor.
// A single behaviour node carries the full base/variance parameter set for one
// particle effect layer (position / velocity / acceleration / rotation / size /
// colour curves), the compiled base-variance pack used by the simulation step,
// and up to five optional wave-form sub-objects. Behaviours are chained through
// mpNext and are owned/serialised by cParticleDescriptor.
//
// LAYOUT AUTHORITY: the member set, order and types are from the DecFIGS DWARF
// (ParticleBehaviour.h:47) and every offset used by the runtime is verified
// against the X360 asm for Build / BuildColourSteps / CompileBaseVariance /
// Delocate / Relocate / GetSerialiseSize / Serialise:
//
//   mFlags                         @0x2C4 (708)
//   mRGBA0/1/Base/Var              @0x210/0x214/0x218/0x21C
//   mRGBADiff (cVector)            @0x1B0 (432)
//   mColour[4]/mColourTime[4]      @0x220 / 0x230
//   mColourStepRGBA[4]/mRGBATime[4]@0x240 / 0x250
//   mColourSteps                   @0x260 (608)
//   mRGBAVarianceMode              @0x264 (612)
//   mZero..mNegInvOneMinus...      @0x268..0x274
//   mAlphaFadeIn/Out               @0x278 / 0x27C
//   mDivisors[4]                   @0x200 (512)
//   mpWaveFormX..mpWaveFormRGB     @0x2C8..0x2D8
//   mpNext                         @0x2DC (732)
//   mBVCompiled                    @0x2E0 (736), size field @0x458 (1112)
//   sizeof(cParticleBehaviour)     == 1216 (0x4C0)  (Serialise/GetSerialiseSize)
//
// X360 pointers are 32-bit; on the host they are wider, so byte offsets differ.
// Members are accessed BY NAME, never by raw offset.
//
// HONEST PLACEHOLDERS (flagged for proper homing -- see ParticleBehaviour.cpp
// dep_flags): cVector, cColour8, cParticleWaveForm and cLionParticleEffectManager
// are vendor math / Lion types that are not yet homed anywhere in the project.
// They are modelled here only as far as this TU's runtime touches them (named
// channels / opaque forward decls), reproduced store-for-store from the asm.
// Grow these additively in their real home when it lands.
// ============================================================================

#include "types.hpp"

class cLionSerialiser;        // LionSerialiser.h (sibling home)

// ----------------------------------------------------------------------------
// HONEST PLACEHOLDER: minimal 4-float vector. The runtime here only stores whole
// vectors and reads/writes individual f32 lanes (x,y,z,w), so a named 4-lane
// struct is sufficient. Replace with the real cVector home when it is homed.
// ----------------------------------------------------------------------------
struct cVector
{
    f32 x;
    f32 y;
    f32 z;
    f32 w;
};

// ----------------------------------------------------------------------------
// HONEST PLACEHOLDER: 8-bit-per-channel packed colour (one 32-bit word). The
// X360 packs the channels little-endian (byte0 = first channel). The runtime
// reads/writes it as a whole word and per-channel; modelled by named channels.
// Replace with the real cColour8 home when it is homed.
// ----------------------------------------------------------------------------
struct cColour8
{
    u8 r;
    u8 g;
    u8 b;
    u8 a;
};

// Opaque Lion types this TU only references by pointer.
class cParticleWaveForm;            // wave-form sub-object (modulates a channel)
class cLionParticleEffectManager;   // owns wave-form allocation pool

// ParticleBehaviour.h:33 -- one compiled (base,variance) pair.
struct sParticleBehaviourBaseVariancePack
{
    f32 base;
    f32 variance;
};

// ParticleBehaviour.h:40 -- the compiled base-variance table consumed by the
// per-particle simulation. CompileBaseVariance packs a subset of the descriptor's
// base/variance vectors into aData[] (a leading run of fixed members followed by
// flag-gated blocks) and records the live pack count in `size`.
struct cParticleBehaviourBaseVarianceCompiled
{
    static const u32 KU_MAX_PACKS = 47;
    sParticleBehaviourBaseVariancePack aData[KU_MAX_PACKS];
    u32 size;
    u32 dummy;
};

// ParticleBehaviour.h:47
struct cParticleBehaviour
{
    // ParticleBehaviour.h:131
    static const u32 KU_COLOUR_STEP_LIMIT = 4;

    // --- mFlags bits used by this TU (from the X360 rlwinm masks) ---
    enum Flags
    {
        E_FLAG_COLOUR_STEP0  = 0x40000,  // BuildColourSteps step gates
        E_FLAG_COLOUR_STEP1  = 0x80000,
        E_FLAG_COLOUR_STEP2  = 0x100000,
        E_FLAG_COLOUR_STEP3  = 0x200000,
        E_DO_WAVEALPHA       = 0x2000,   // Build assert: must be clear here
        E_BV_AXIS            = 0x10,     // CompileBaseVariance axis block
        E_BV_OFFSETROT       = 0x20,     // CompileBaseVariance offset-rot block
        E_BV_ROT             = 0x1,      // CompileBaseVariance rot (full) block
        E_BV_ROTVELACC       = 0x40,     // CompileBaseVariance rot vel/acc block
        E_BV_SIZE_FULL       = 0x80,     // CompileBaseVariance size (full) block
    };

    void Build();
    void BuildColourSteps();
    void CompileBaseVariance();
    void Delocate(u32 aEndianTwiddleFlag);
    void Relocate();
    void GetSerialiseSize(cLionSerialiser& aSer) const;
    cParticleBehaviour* Serialise(cLionSerialiser& aSer) const;

    // ----- members (offsets verified against the X360 asm) -----
    cVector mAccBase;                 // 0x000
    cVector mAccVariance;             // 0x010
    cVector mAxisBase;                // 0x020
    cVector mOffsetRotXYZBase;        // 0x030
    cVector mOffsetRotXYZVariance;    // 0x040
    cVector mOffsetRotXYZVelBase;     // 0x050
    cVector mOffsetRotXYZVelVariance; // 0x060
    cVector mOffsetRotXYZAccBase;     // 0x070
    cVector mOffsetRotXYZAccVariance; // 0x080
    cVector mRotXYZBase;              // 0x090
    cVector mRotXYZVariance;          // 0x0A0
    cVector mRotXYZVelBase;           // 0x0B0
    cVector mRotXYZVelVariance;       // 0x0C0
    cVector mRotXYZAccBase;           // 0x0D0
    cVector mRotXYZAccVariance;       // 0x0E0
    cVector mPivotPoint;              // 0x0F0
    cVector mPosBase;                 // 0x100
    cVector mPosVariance;             // 0x110
    cVector mRingRadius;              // 0x120
    cVector mSizeXYZBase;             // 0x130
    cVector mSizeXYZVariance;         // 0x140
    cVector mSizeXYZVelBase;          // 0x150
    cVector mSizeXYZVelVariance;      // 0x160
    cVector mSizeXYZAccBase;          // 0x170
    cVector mSizeXYZAccVariance;      // 0x180
    cVector mVelBase;                 // 0x190
    cVector mVelVariance;             // 0x1A0
    cVector mRGBADiff;                // 0x1B0
    cVector mColourStepsRGBAv[4];     // 0x1C0
    f32     mDivisors[4];             // 0x200
    cColour8 mRGBA0;                  // 0x210
    cColour8 mRGBA1;                  // 0x214
    cColour8 mRGBABase;               // 0x218
    cColour8 mRGBAVar;                // 0x21C
    cColour8 mColour[4];              // 0x220
    f32     mColourTime[4];           // 0x230
    cColour8 mColourStepRGBA[4];      // 0x240
    f32     mRGBATime[4];             // 0x250
    u32     mColourSteps;             // 0x260
    u32     mRGBAVarianceMode;        // 0x264
    f32     mZero;                    // 0x268
    f32     mAlphaFadeOutPlusInvOneMinusAlphaFadeOut; // 0x26C
    f32     mAlphaFadeInInv;          // 0x270
    f32     mNegInvOneMinusAlphaFadeOut;              // 0x274
    f32     mAlphaFadeIn;             // 0x278
    f32     mAlphaFadeOut;            // 0x27C
    f32     mCellSize;               // 0x280
    f32     mCloneScaleInTime;       // 0x284
    f32     mDragFactor;             // 0x288
    f32     mMass;                   // 0x28C
    f32     mSizeBase;               // 0x290
    f32     mSizeVariance;           // 0x294
    f32     mSizeVelBase;            // 0x298
    f32     mSizeVelVariance;        // 0x29C
    f32     mSizeAccBase;            // 0x2A0
    f32     mSizeAccVariance;        // 0x2A4
    f32     mEmissionRateBase;       // 0x2A8
    f32     mEmissionRateVariance;   // 0x2AC
    f32     mLifeBase;               // 0x2B0
    f32     mLifeVariance;           // 0x2B4
    f32     mRadius;                 // 0x2B8
    f32     mScale;                  // 0x2BC
    u32     mEmissionCountClamp;     // 0x2C0
    u32     mFlags;                  // 0x2C4
    cParticleWaveForm* mpWaveFormX;     // 0x2C8
    cParticleWaveForm* mpWaveFormY;     // 0x2CC
    cParticleWaveForm* mpWaveFormZ;     // 0x2D0
    cParticleWaveForm* mpWaveFormAlpha; // 0x2D4
    cParticleWaveForm* mpWaveFormRGB;   // 0x2D8
    cParticleBehaviour* mpNext;         // 0x2DC
    cParticleBehaviourBaseVarianceCompiled mBVCompiled; // 0x2E0
    bool    mEmissionRateHasBeenScaled; // 0x460
    u32     mEmissionCountClampVariance;
    f32     mEndOnAlphaFade;
    f32     mEndOnScale;
    f32     mEndOnStartAngle;
    f32     mEndOnEndAngle;
    f32     mTimeScale;
    f32     mTimeScaleVariance;
    u32     mRibbonParticleCount;
    f32     mDragFactorVel;
    f32     mDragFactorRot;
    f32     mDragFactorScale;
    f32     mEmitterStartWeight;
    f32     mEmitterEndWeight;
    f32     mEmitterVelWeight;
    cVector mAABBMin;
    cVector mAABBMax;
};
