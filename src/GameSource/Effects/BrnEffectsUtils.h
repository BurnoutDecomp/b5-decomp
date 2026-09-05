#pragma once

// =============================================================================
// BrnEffectsUtils.h  (OWNING HEADER for BrnEffects::Utils::Vector3Randomiser
//                     and BrnEffects::Utils::Vector4Randomiser)
//
// The two per-call vector randomisers. Each holds a pair of vectors
// (mVecA / mVecB, set up by the other-TU Prepare) and draws random components
// straight from a CgsNumeric::Random's internal LCG ring (reused BY NAME via
// the friend grant in CgsRandom.h).
//
// Layout (DWARF references/DecFIGS/dwarfdump/GameSource/Effects/BrnEffectsUtils.h):
//   :93  Vector3Randomiser { Vector3 mVecA(:130); Vector3 mVecB(:131); }
//   :135 Vector4Randomiser { Vector4 mVecA(:172); Vector4 mVecB(:173); }
//
// Bodied here (asm-attested, reconstructed store-for-store):
//   Vector3Randomiser::RandomiseXYZ  @ 0x82277EC8
//   Vector4Randomiser::RandomiseXYZW @ 0x82277FB8
// Prepare / RandomInterpolate are other-TU surface -> declared-only.
//
// THE DRAW (both): the randomiser advances the Random's 64-bit LCG
//   seed = seed * KU_RANDOM_MULTIPLIER + 1
// once per output component, packs the high 32 bits of each step into the
// mantissa of an IEEE-754 float in [1, 2) and writes those float-bits into the
// Random's ring at a Vector-slot chosen by ((index + 3) & 4) (a 2-slot Vector4
// double-buffer over the 8-entry f32 ring). It then RETURNS the slot that was
// primed on the PREVIOUS call (a 1-deep pipeline): the slot is loaded BEFORE the
// new bits are written, 1.0 is subtracted (giving components in [0, 1)), and the
// result is  mVecA * mVecB + (previousDraw - 1.0)  computed per lane.
// =============================================================================

#include "BrnCommonTypes.h"   // Vector3, Vector4, Matrix33, VecFloat (rw::math::vpu float lanes)
#include "GameShared/GameClasses/Numeric/CgsRandom.h"  // CgsNumeric::Random (+ LCG constants)

// The Lion particle material BuildUVData::SetupFromMaterial reads. Reference-only here (the
// full record lives in the Lion SDK header, which this general-purpose Effects header must not
// drag into every consumer) -- BrnEffectsBuildUVData.cpp includes the real one.
class cParticleMaterial;

namespace BrnEffects
{
namespace Utils
{

// =============================================================================
// The named colour-conversion constants (DWARF BrnEffectsUtils.h:43/:44/:45).
//
// All three are dynamically-initialised .bss splats in the X360 image, so a literal read of
// their addresses returns 0x00000000 BY DEFINITION; each value below was recovered through
// its CRT init thunk (tools/re/findinit.py -> tools/re/ppcdis.py -> tools/re/x360rd.py):
//
//   unk_82FAC220 <- 0x82C4A0C0 : splat4(flt_82010C20)                     == 255.0f
//   unk_82FAC210 <- 0x82C4A0D8 : (flt_82013F98 x3, flt_82010C20)          == (511,511,511,255)
//   unk_82FAC100 <- 0x82C4A110 : splat4(flt_82010C1C) == 0x3B808081       == 1/255
//
// K_VECTOR4_511_511_511_255 is the OVERBRIGHT scale: RGB gets 511 (so a 0.5 channel saturates
// to 255 and anything above it clamps), alpha gets the plain 255. That asymmetry is the whole
// content of the name ConvertVector4ToRwRgbaOverbright.
// =============================================================================
extern const VecFloat K_VECFLOAT_255;                 // BrnEffectsUtils.h:43   splat4(255.0f)
extern const Vector4  K_VECTOR4_511_511_511_255;      // BrnEffectsUtils.h:44
extern const VecFloat K_VECFLOAT_ONEOVER255;          // BrnEffectsUtils.h:45   splat4(1/255)

struct Vector3Randomiser
{
private:
    // DWARF BrnEffectsUtils.h:130 / :131.
    Vector3 mVecA;
    Vector3 mVecB;

public:
    // DWARF BrnEffectsUtils.h:100 / :109. Other-TU bodies.
    void    Prepare(Vector3 lvA, Vector3 lvB);
    Vector3 RandomInterpolate(CgsNumeric::Random &lrRandom);

    // DWARF BrnEffectsUtils.h:121 / asm @ 0x82277EC8. Defined in BrnEffectsUtils.cpp.
    Vector3 RandomiseXYZ(CgsNumeric::Random &lrRandom);
};

struct Vector4Randomiser
{
private:
    // DWARF BrnEffectsUtils.h:172 / :173.
    Vector4 mVecA;
    Vector4 mVecB;

public:
    // DWARF BrnEffectsUtils.h:142 / :151. Other-TU bodies.
    void    Prepare(Vector4 lvA, Vector4 lvB);
    Vector4 RandomInterpolate(CgsNumeric::Random &lrRandom);

    // DWARF BrnEffectsUtils.h:163 / asm @ 0x82277FB8. Defined in BrnEffectsUtils.cpp.
    Vector4 RandomiseXYZW(CgsNumeric::Random &lrRandom);
};

// =============================================================================
// BrnEffects::Utils::BuildUVData (DWARF BrnEffectsUtils.h:395) -- the per-material UV-atlas
// setup block the three LionBlendRenderer draw halves fill once per call and then hand to
// BuildUVs for every particle.
//
// ⭐ THE MEMBER NAMES ARE THE DWARF'S, NOT INFERRED. The first version of this struct lived
// inside BrnEffectsBuildUVData.cpp (so no caller could name the type at all) with guessed
// names -- "texel count", "texture width/height", "frame columns/rows". The DWARF names them
// mvfMaterialFrameCount / mvfMaterialNumXFrames / mvfMaterialOneOverNumXFrames /
// mvfMaterialOneOverNumYFrames / muMaterialFlags / muMaterialUCoordOption /
// muMaterialVCoordOption, and cParticleMaterial's own attested layout agrees field for field
// (+0x34 mFrameCount, +0x38 mXFrames, +0x39 mYFrames, +0x24 mFlags, +0x3F mUCoordOption,
// +0x40 mVCoordOption). It is a FRAME-ATLAS descriptor, not a texel-size one.
// =============================================================================
struct BuildUVData
{
    // DWARF BrnEffectsUtils.h:412-418. Every member is written by SetupFromMaterial
    // @0x822780C8 in this order (four 16-byte splats then the three trailing scalars at
    // +0x40 / +0x44 / +0x45 -- QuadDraw's `lwz r11, 0x40(r30)` reads muMaterialFlags there).
    VecFloat mvfMaterialFrameCount;         // +0x00  splat4((f32)(s32) material.mFrameCount)
    VecFloat mvfMaterialNumXFrames;         // +0x10  splat4((f32)(u8)  material.mXFrames)
    VecFloat mvfMaterialOneOverNumXFrames;  // +0x20  splat4(1 / that)
    VecFloat mvfMaterialOneOverNumYFrames;  // +0x30  splat4(1 / (f32)(u8) material.mYFrames)
    u32      muMaterialFlags;               // +0x40  material.mFlags, verbatim
    u8       muMaterialUCoordOption;        // +0x44  material.mUCoordOption
    u8       muMaterialVCoordOption;        // +0x45  material.mVCoordOption

    // The two material.mFlags bits this block's consumers test. ⭐ BOTH NAMES ARE THE GAME'S
    // OWN, read out of the X360 Lion authoring token table (LionParticleParser.cpp:190/:191,
    // offset 36 == cParticleMaterial::mFlags) -- not inferred, and not the same bit:
    //   BuildUVs @0x822781E0 tests FLAG_MULTIFRAME       with `clrlwi r11, r11, 31` (0x1)
    //            to choose the frame-atlas cell over the whole texture;
    //   QuadDraw @0x82282330 tests FLAG_INTERFRAMEBLEND  with `rlwinm r11,r11,0,30,30` (0x2)
    //            to decide whether the vertex w lane carries the fractional blend weight
    //            between the current and next frame, or a hard zero.
    static const u32 KU_MATERIAL_FLAG_MULTIFRAME       = 0x1u;
    static const u32 KU_MATERIAL_FLAG_INTERFRAMEBLEND  = 0x2u;

    // DWARF BrnEffectsUtils.h:398 / X360 @0x822780C8. Defined in BrnEffectsBuildUVData.cpp.
    void SetupFromMaterial(const cParticleMaterial& arMaterial);
};

// =============================================================================
// Namespace-scoped VMX helpers (the Hex-Rays "BrnEffects::Utils::*" free functions).
// =============================================================================

// 0x822781E0 (DWARF BrnEffectsUtils.h:453) -- build the four UV-corner vectors for one
// particle's quad from the material's frame-atlas data and the particle's current/next frame.
// RECONSTRUCTED (BrnEffectsUtils.cpp).
//
// ⚠ EACH OUTPUT IS TWO UV PAIRS, NOT ONE. The DWARF calls QuadDraw's local `laUvUv`, and the
// asm says why: every `vsldoi ..., 8` at the tail welds the CURRENT frame's (u,v) into lanes
// x/y and the NEXT frame's into lanes z/w. The vertex shader blends between them using the
// weight QuadDraw puts in the position's w lane.
//
// ⚠ THE SIGNATURE WAS WRONG until 2026-09-05: this was declared
// `BuildUVs(const Vector4* lpQuad, Vector4* lpaUVsOut)`, which loses the two VecFloat frame
// arguments entirely. The X360 call site inside QuadDraw @0x822823E4 sets r3 = &lUVData,
// r4 = &laUvUv[0], v1 = splat(aPart.Frame()) and v2 = splat(aPart.NextFrame()) -- the two
// vector registers are arguments 2 and 3, which is exactly the DWARF's declaration. An f32/
// vector argument does not consume a GPR slot here, so reading the prototype off the GPRs
// alone dropped them.
void BuildUVs(const BuildUVData& arUVData, VecFloat lvfFrame, VecFloat lvfNextFrame,
              Vector4* lpaUVsOut);

// The outlined CgsNumeric::TrigFunctions<CgsNumeric::TrigBaseFunctions5>::SinCos the DWARF names
// inside FastMatrix33FromEulerXYZ and inside both draw halves' z-only rotation path. The X360
// folds it into every caller (it has NO row in the ledger), and it is exposed here rather than
// copied into each site so the recovered polynomial has exactly one home. lfRadians in, the
// matching sine and cosine out; see the .cpp for the seven recovered coefficients.
void SinCosCycles(f32 lfRadians, f32& arSin, f32& arCos);

// 0x8227E7A8 (DWARF BrnEffectsUtils.h:259) -- the 3x3 rotation matrix Rx*Ry*Rz built from
// packed Euler XYZ angles through a minimax polynomial sin/cos. RECONSTRUCTED
// (BrnEffectsUtils.cpp).
//
// ⚠ IT RETURNS BY VALUE. The X360 passes the 48-byte result through the hidden r3 pointer
// (the three `stvx128 v0, r3, {0,0x10,0x20}` at 0x8227E964/0x8227E990/0x8227E998) and takes
// the angles in v1 -- i.e. exactly the DWARF's `Matrix33 FastMatrix33FromEulerXYZ(Vector3)`,
// not the `(Matrix33*, Vector3)` this file used to declare.
Matrix33 FastMatrix33FromEulerXYZ(Vector3 lv3EulerAngles);

} // namespace Utils
} // namespace BrnEffects
