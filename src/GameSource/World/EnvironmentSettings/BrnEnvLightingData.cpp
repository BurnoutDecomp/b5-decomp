#include "BrnEnvLightingData.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnWorld::EnvironmentSettings::LightingData::Construct
//
// Copies the eight default lighting vectors from the module's static default
// template into the keyframe, then applies the scalar override at +128. The
// guest used SIMD loads/stores from the data segment; the source vectors are
// referenced here by their recovered data symbols (defined with the template
// data TU).

namespace BrnWorld
{
namespace EnvironmentSettings
{
// ---------------------------------------------------------------------------
// The static default template vectors, RECOVERED (envfix round, 2026-08-16).
//
// They were `extern const float KAF_*[4];` with NO DEFINITION ANYWHERE IN THE
// TREE -- a latent unresolved external that `cl /c` cannot see and that would
// have broken the link the moment this TU was mounted (grep pasted in the report).
//
// The values are NOT invented. Each lives in .data (unk_82FF*, ZERO in the image)
// and is written by a CRT dynamic initialiser that lfs-loads its lanes from .rdata
// into a 16-byte stack slot and stvx128-stores it into the global. Those
// initialisers sit in an EXPORT HOLE (0x82C60AA0..0x82C61238 has no .ida-exports
// JSON), so they were read directly out of the shipped XEX image: the retail
// BURNOUT_X360_ARTIST.XEX was decrypted (XEX2, all-zero devkit key, basic
// compression block list) and the initialiser range emulated instruction by
// instruction. Oracle for the decode: the .rdata block at 0x820A4620 came out
// byte-identical to DATA_DUMP.md's independent idat dump
// (3F000000 41100000 00000000 41500000 ...). The recovered lane sources are quoted
// per array below; the w lane is 0 in every one, which is what a Vector3 stored
// through a 16-byte register looks like.
// ---------------------------------------------------------------------------
namespace
{
// unk_82FFAEE0 <- init @0x82C60EC8   (mv3KeyLightColour)
const float KAF_LightingDefault0[4] = { 1.0f,  0.9f,  0.95f, 0.0f };
// unk_82FFB090 <- init @0x82C60F10   (mv3SpecularColour)
const float KAF_LightingDefault1[4] = { 1.0f,  0.95f, 0.9f,  0.0f };
// unk_82FFB1F0 <- init @0x82C60F48   (mv3KeyFillColour)
const float KAF_LightingDefault2[4] = { 1.2f,  1.2f,  1.2f,  0.0f };
// unk_82FFB040 <- init @0x82C60F88   (mv3ShadowFillColour)
const float KAF_LightingDefault3[4] = { 0.5f,  0.5f,  0.55f, 0.0f };
// unk_82FFB050 <- init @0x82C60FD0   (mv3RightFillColour)
const float KAF_LightingDefault4[4] = { 0.96f, 1.0f,  0.9f,  0.0f };
// unk_82FFB010 <- init @0x82C61018   (mv3LeftFillColour -- byte-identical to the
//                                     right fill, i.e. a symmetric side-fill rig)
const float KAF_LightingDefault5[4] = { 0.96f, 1.0f,  0.9f,  0.0f };
// unk_82FFAE10 <- init @0x82C61060   (mv3UpFillColour)
const float KAF_LightingDefault6[4] = { 0.3f,  0.4f,  0.48f, 0.0f };
// unk_82FFAE40 <- init @0x82C610A0   (mv3DownFillColour)
const float KAF_LightingDefault7[4] = { 0.95f, 0.97f, 0.97f, 0.0f };

    void CopyVec4(float* lpDst, const float* lpSrc)
    {
        for (int i = 0; i < 4; ++i)
        {
            lpDst[i] = lpSrc[i];
        }
    }

    // See the twin helper in BrnEnvScatteringData.cpp: the de-inlined 4-lane
    // weighted sum the guest emits as vmulfp128 + three vmaddfp.
    void BlendVec4(float* lpfDst,
                   const float* lpfA0, float lfWeightA0,
                   const float* lpfA1, float lfWeightA1,
                   const float* lpfB0, float lfWeightB0,
                   const float* lpfB1, float lfWeightB1)
    {
        for (int i = 0; i < 4; ++i)
        {
            lpfDst[i] = lpfA0[i] * lfWeightA0 + lpfA1[i] * lfWeightA1
                      + lpfB0[i] * lfWeightB0 + lpfB1[i] * lfWeightB1;
        }
    }
}

void LightingData::Construct()
{
    CopyVec4(mv3KeyLightColour,   KAF_LightingDefault0);
    CopyVec4(mv3SpecularColour,   KAF_LightingDefault1);
    CopyVec4(mv3KeyFillColour,    KAF_LightingDefault2);
    CopyVec4(mv3ShadowFillColour, KAF_LightingDefault3);
    CopyVec4(mv3RightFillColour,  KAF_LightingDefault4);
    CopyVec4(mv3LeftFillColour,   KAF_LightingDefault5);
    CopyVec4(mv3UpFillColour,     KAF_LightingDefault6);

    mfAmbientIrradianceScale = 0.2f;

    CopyVec4(mv3DownFillColour,   KAF_LightingDefault7);
}

// ---------------------------------------------------------------------------
// SetToBlend (4-way) @ 0x827AFAA8
//
// *this = A0*wA0 + A1*wA1 + B0*wB0 + B1*wB1, element-wise over the WHOLE
// LightingData layout -- unlike ScatteringData there is no hole here: the guest
// writes eight 16-byte vector stores (+0x00 +0x10 +0x20 +0x30 +0x40 +0x50 +0x60
// +0x70) and one scalar store (+0x80), i.e. all 33 floats of the 0x84-byte class.
// Store list read off the disassembly:
//   0x827AFBB8 stvx128 v0, r0, r3      -> +0x00 mv3KeyLightColour
//   0x827AFCC0 stvx128 v0, r3, r9(=0x10) -> +0x10 mv3SpecularColour
//   0x827AFCE8 stvx128 v0, r3, r7(=0x20) -> +0x20 mv3KeyFillColour
//   0x827AFDE8 stvx128 v0, r3, r9(=0x30) -> +0x30 mv3ShadowFillColour
//   0x827AFE9C stvx128 v0, r3, r9(=0x40) -> +0x40 mv3RightFillColour
//   0x827AFF08 stvx128 v0, r3, r7(=0x50) -> +0x50 mv3LeftFillColour
//   0x827AFF5C stvx128 v0, r3, r9(=0x60) -> +0x60 mv3UpFillColour
//   0x827AFFEC stvx128 v0, r3, r9(=0x70) -> +0x70 mv3DownFillColour
//   0x827B0010 stfs    f0, 0x80(r3)      -> +0x80 mfAmbientIrradianceScale
//     (0x827AFFF0-0x827B000C: f0 = A1*f2; f0 = A0*f1 + f0; f0 = B0*f3 + f0;
//      f0 = B1*f4 + f0 -- the same fmuls/3x fmadds chain as ScatteringData.)
//
// PARAMETER ORDER FROM THE ASM (a float argument skips its GPR slot on PPC):
// r3=this, r4=A0/f1=wA0, r6=A1/f2=wA1, r8=B0/f3=wB0, r10=B1/f4=wB1 -- the
// interleaved (value, weight) list the DWARF and the Feb-2007 header declare.
// ---------------------------------------------------------------------------
void LightingData::SetToBlend( const LightingData& lValueA0, float lfWeightA0,
                               const LightingData& lValueA1, float lfWeightA1,
                               const LightingData& lValueB0, float lfWeightB0,
                               const LightingData& lValueB1, float lfWeightB1 )
{
    BlendVec4( mv3KeyLightColour,
               lValueA0.mv3KeyLightColour, lfWeightA0, lValueA1.mv3KeyLightColour, lfWeightA1,
               lValueB0.mv3KeyLightColour, lfWeightB0, lValueB1.mv3KeyLightColour, lfWeightB1 );
    BlendVec4( mv3SpecularColour,
               lValueA0.mv3SpecularColour, lfWeightA0, lValueA1.mv3SpecularColour, lfWeightA1,
               lValueB0.mv3SpecularColour, lfWeightB0, lValueB1.mv3SpecularColour, lfWeightB1 );
    BlendVec4( mv3KeyFillColour,
               lValueA0.mv3KeyFillColour, lfWeightA0, lValueA1.mv3KeyFillColour, lfWeightA1,
               lValueB0.mv3KeyFillColour, lfWeightB0, lValueB1.mv3KeyFillColour, lfWeightB1 );
    BlendVec4( mv3ShadowFillColour,
               lValueA0.mv3ShadowFillColour, lfWeightA0, lValueA1.mv3ShadowFillColour, lfWeightA1,
               lValueB0.mv3ShadowFillColour, lfWeightB0, lValueB1.mv3ShadowFillColour, lfWeightB1 );
    BlendVec4( mv3RightFillColour,
               lValueA0.mv3RightFillColour, lfWeightA0, lValueA1.mv3RightFillColour, lfWeightA1,
               lValueB0.mv3RightFillColour, lfWeightB0, lValueB1.mv3RightFillColour, lfWeightB1 );
    BlendVec4( mv3LeftFillColour,
               lValueA0.mv3LeftFillColour, lfWeightA0, lValueA1.mv3LeftFillColour, lfWeightA1,
               lValueB0.mv3LeftFillColour, lfWeightB0, lValueB1.mv3LeftFillColour, lfWeightB1 );
    BlendVec4( mv3UpFillColour,
               lValueA0.mv3UpFillColour, lfWeightA0, lValueA1.mv3UpFillColour, lfWeightA1,
               lValueB0.mv3UpFillColour, lfWeightB0, lValueB1.mv3UpFillColour, lfWeightB1 );
    BlendVec4( mv3DownFillColour,
               lValueA0.mv3DownFillColour, lfWeightA0, lValueA1.mv3DownFillColour, lfWeightA1,
               lValueB0.mv3DownFillColour, lfWeightB0, lValueB1.mv3DownFillColour, lfWeightB1 );

    mfAmbientIrradianceScale =
          lValueA0.mfAmbientIrradianceScale * lfWeightA0
        + lValueA1.mfAmbientIrradianceScale * lfWeightA1
        + lValueB0.mfAmbientIrradianceScale * lfWeightB0
        + lValueB1.mfAmbientIrradianceScale * lfWeightB1;
}
}
}
