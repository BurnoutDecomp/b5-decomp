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
//   mAABBMin / mAABBMax            @0x4A0 (1184) / 0x4B0 (1200)
//   sizeof(cParticleBehaviour)     == 1216 (0x4C0)  (Serialise/GetSerialiseSize)
//
// ⭐ 2026-09-03: the six links are tLionSerialisedPtr, so the console offsets above are
// ALSO the host offsets (a .lef behaviour is read verbatim) and the static_asserts at the
// bottom of this file check them. Members are accessed BY NAME, never by raw offset.
//
// ⭐⭐ AND cVector IS 16-BYTE ALIGNED -- that is where the record's last four bytes live.
// With a 4-aligned cVector the member set below sizes to 1212, not the attested 1216, and
// the temptation is to go hunting for a missing member. There is none. cParticleBehaviour::
// Lerp @0x8290B1F8 reads mAABBMin at 1184 (0x4A0) and mAABBMax at 1200 (0x4B0), while
// cParticleBehaviour::Init @0x82908EA0 writes the last scalar before them, mEmitterVelWeight,
// at 1176 -- so the compiler padded 1180 -> 1184 to give mAABBMin a 16-byte boundary, and
// 1200 + 16 == 1216 closes the record. Every other cVector in this struct already sits on a
// 16-byte boundary, which is the corroboration: they are PPC vector registers, not structs
// of four floats that happen to be 16 bytes wide.
//
// HONEST PLACEHOLDERS (flagged for proper homing -- see ParticleBehaviour.cpp
// dep_flags): cVector, cColour8, cParticleWaveForm and cLionParticleEffectManager
// are vendor math / Lion types that are not yet homed anywhere in the project.
// They are modelled here only as far as this TU's runtime touches them (named
// channels / opaque forward decls), reproduced store-for-store from the asm.
// Grow these additively in their real home when it lands.
// ============================================================================

#include "types.hpp"
#include "SDKs/Packages/Lion/Final/eauk_common/Maths/Vector.h"   // cVector -- the real eauk_common home (fork retired 2026-09-03)
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionSerialisedPtr.h"

class cLionSerialiser;        // LionSerialiser.h (sibling home)

// ----------------------------------------------------------------------------
// cVector now comes from its real home, eauk_common/Maths/Vector.h (included above);
// the private copy here is retired. Its alignas(16) IS AN ASM FACT and moved with it --
// those are the four bytes between this record's 1212 and its attested 1216 (see the
// banner). The DWARF declares the full type there as `struct cVector { float q[4]; }`
// with the PPC accessor set (GetX/SetX/GetSplatX/...); growing the home is the follow-up.
// ----------------------------------------------------------------------------

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

    void Init();
    void Build();
    void BuildColourSteps();
    void CompileBaseVariance();
    // ParticleBehaviour.h:314 (DWARF). Interpolate two behaviour layers into this one.
    // X360 @0x8290B1F8 -- 1,530 instructions. NOT RECONSTRUCTED; the LOG-ONCE stub is in
    // LionRuntimeLinkStubs.cpp, and its note says what the miss costs.
    void Lerp(const cParticleBehaviour* apLo, const cParticleBehaviour* apHi, f32 afWeight);

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
    tLionSerialisedPtr<cParticleWaveForm>  mpWaveFormX;     // 0x2C8 Relocate word 178
    tLionSerialisedPtr<cParticleWaveForm>  mpWaveFormY;     // 0x2CC Relocate word 179
    tLionSerialisedPtr<cParticleWaveForm>  mpWaveFormZ;     // 0x2D0 Relocate word 180
    tLionSerialisedPtr<cParticleWaveForm>  mpWaveFormAlpha; // 0x2D4 Relocate word 181
    tLionSerialisedPtr<cParticleWaveForm>  mpWaveFormRGB;   // 0x2D8 Relocate word 182
    tLionSerialisedPtr<cParticleBehaviour> mpNext;          // 0x2DC Relocate word 183
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
    cVector mAABBMin;                   // 0x4A0 -- Lerp @0x8290B1F8 reads 1184/1188/1192
    cVector mAABBMax;                   // 0x4B0 -- Lerp @0x8290B1F8 reads 1200/1204/1208
};

// cParticleBehaviour::Serialise @0x8290F098 does `DataStore(this, 1216)`, and
// GetSerialiseSize @0x8290CCD0 adds the same 1216 per node.
static_assert(sizeof(cParticleBehaviour) == 1216,
              "cParticleBehaviour is the 1216-byte serialised record "
              "(cParticleBehaviour::Serialise @0x8290F098 DataStore(this, 1216)) -- if this "
              "reads 1212, cVector has lost its alignas(16)");
static_assert(offsetof(cParticleBehaviour, mDivisors)       == 0x200, "CompileBaseVariance");
static_assert(offsetof(cParticleBehaviour, mFlags)          == 0x2C4, "the rlwinm masks");
static_assert(offsetof(cParticleBehaviour, mpWaveFormX)     == 0x2C8, "Relocate asm word 178");
static_assert(offsetof(cParticleBehaviour, mpNext)          == 0x2DC, "Relocate asm word 183");
static_assert(offsetof(cParticleBehaviour, mBVCompiled)     == 0x2E0, "CompileBaseVariance");
static_assert(offsetof(cParticleBehaviour, mEmitterVelWeight) == 1176, "Init @0x82908EA0");
static_assert(offsetof(cParticleBehaviour, mAABBMin)        == 1184, "Lerp @0x8290B1F8");
static_assert(offsetof(cParticleBehaviour, mAABBMax)        == 1200, "Lerp @0x8290B1F8");
