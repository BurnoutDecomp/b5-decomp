#include "BrnEnvScatteringData.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnWorld::EnvironmentSettings::ScatteringData::Construct
//
// Copies six default scattering vectors from the module's static default
// template and fills the explicit scalar fields. The guest used SIMD
// loads/stores; the source vectors are referenced here by their recovered data
// symbols (defined with the template data TU).

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
// unk_82FFB170 <- init @0x82C60D40   (mv3SkyTopColour)
const float KAF_ScatteringDefault0[4] = { 0.0f,  0.3f,  0.9f,  0.0f };
// unk_82FFB000 <- init @0x82C60D80   (mv3SkyHorColour)
const float KAF_ScatteringDefault1[4] = { 0.9f,  1.0f,  1.0f,  0.0f };
// unk_82FFB080 <- init @0x82C60DB8   (mv3SkySunColour)
const float KAF_ScatteringDefault2[4] = { 1.0f,  1.0f,  1.0f,  0.0f };
// unk_82FFB020 <- init @0x82C60E00   (mv3ScattTopColour)
const float KAF_ScatteringDefault3[4] = { 0.0f,  0.1f,  0.3f,  0.0f };
// unk_82FFB200 <- init @0x82C60E48   (mv3ScattHorColour)
const float KAF_ScatteringDefault4[4] = { 0.7f,  0.8f,  0.9f,  0.0f };
// unk_82FFAE30 <- init @0x82C60E80   (mv3ScattSunColour)
const float KAF_ScatteringDefault5[4] = { 1.0f,  1.0f,  1.0f,  0.0f };

    void CopyVec4(float* lpDst, const float* lpSrc)
    {
        for (int i = 0; i < 4; ++i)
        {
            lpDst[i] = lpSrc[i];
        }
    }

    // The 4-lane weighted sum SetToBlend runs on every colour member. The guest
    // emits it as vmulfp128 + three vmaddfp on a 16-byte register; the original
    // source wrote it with rw::math Vector3 operator* / operator+ (Feb-2007
    // SharedClasses/World/BrnEnvironmentData.h). All four lanes are written --
    // the guest's stvx128 stores the whole register, w lane included.
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

void ScatteringData::Construct()
{
    CopyVec4(mv3SkyTopColour, KAF_ScatteringDefault0);
    CopyVec4(mv3SkyHorColour, KAF_ScatteringDefault1);
    CopyVec4(mv3SkySunColour, KAF_ScatteringDefault2);

    mfSkyHorPow      = 0.5f;
    mfSkySunPow      = 9.0f;
    mfSkyDrk         = 0.0f;
    mfSkyHorBleedScl = 13.0f;
    mfSkyHorBleedPow = 9.0f;
    mfSkySunBleedPow = 7.0f;

    CopyVec4(mv3ScattTopColour, KAF_ScatteringDefault3);
    CopyVec4(mv3ScattHorColour, KAF_ScatteringDefault4);
    CopyVec4(mv3ScattSunColour, KAF_ScatteringDefault5);

    mfScattHorPow      = 0.5f;
    mfScattSunPow      = 9.0f;
    mfScattDrk         = 0.0f;
    mfScattHorBleedScl = 13.0f;
    mfScattHorBleedPow = 9.0f;
    mfScattSunBleedPow = 14.0f;

    mafScattDist[0] = 100.0f;   // 0x98: high word of the guest's 0x42C8...0000 store
    mafScattDist[1] = 6000.0f;  // 0x9C: low word (0x...45BB8000)
    mfScattPow = 0.31f;
    mfScattCap = 0.89999998f;
}

// ---------------------------------------------------------------------------
// SetToBlend (4-way) @ 0x827AF468
//
// *this = A0*wA0 + A1*wA1 + B0*wB0 + B1*wB1, applied ELEMENT-WISE over the whole
// ScatteringData layout EXCEPT the 8-byte alignment gap at +0x48 (mPad48).
//
// The guest's stores, read off the disassembly, are exactly:
//   stvx128 +0x00 / +0x10 / +0x20            (mv3Sky{Top,Hor,Sun}Colour, 4 lanes each)
//   stfs    +0x30 +0x34 +0x38 +0x3C +0x40 +0x44
//   stvx128 +0x50 / +0x60 / +0x70            (mv3Scatt{Top,Hor,Sun}Colour)
//   stfs    +0x80 +0x84 +0x88 +0x8C +0x90 +0x94 +0x98 +0x9C +0xA0 +0xA4
// -- +0x48 and +0x4C are NEVER written: 0x827AF670 goes straight from the +0x44
// scalar group to the +0x50 vector. That hole is the Vector3 alignment pad
// between mfSkySunBleedPow and mv3ScattTopColour; the parser's field-descriptor
// table at 0x820A4690 (22 records, offsets 0x00..0xA4) has no record there
// either, so it is genuinely not a field. DO NOT "helpfully" blend it.
//
// Every arithmetic step has the same shape. The guest broadcasts each weight into
// a VMX lane (stfs -> lvx128 -> vspltw) and runs vmulfp128 + three vmaddfp for the
// vector members, and fmuls + three fmadds for the scalars, e.g. @0x827AF670:
//   f0 = A1.f*wA1 ; f0 = A0.f*wA0 + f0 ; f0 = B0.f*wB0 + f0 ; f0 = B1.f*wB1 + f0
// == A0*wA0 + A1*wA1 + B0*wB0 + B1*wB1 (only the first addition's operand order is
// reassociated, and IEEE addition is commutative, so the result is bit-identical).
//
// PARAMETER ORDER COMES FROM THE ASM, not the pseudocode: a float argument skips
// its GPR slot on PPC, so r3=this, r4=A0 with f1=wA0, r6=A1/f2, r8=B0/f3,
// r10=B1/f4 -- the interleaved (value, weight) list that the DWARF
// (SharedClasses/World/BrnEnvironmentData.h, ScatteringData::SetToBlend 4-way),
// the Feb-2007 header and the committed CloudsData 4-way all declare.
// ---------------------------------------------------------------------------
void ScatteringData::SetToBlend( const ScatteringData& lValueA0, float lfWeightA0,
                                 const ScatteringData& lValueA1, float lfWeightA1,
                                 const ScatteringData& lValueB0, float lfWeightB0,
                                 const ScatteringData& lValueB1, float lfWeightB1 )
{
    // +0x00 / +0x10 / +0x20 -- the three sky colours (stvx128, all four lanes).
    BlendVec4( mv3SkyTopColour,
               lValueA0.mv3SkyTopColour, lfWeightA0, lValueA1.mv3SkyTopColour, lfWeightA1,
               lValueB0.mv3SkyTopColour, lfWeightB0, lValueB1.mv3SkyTopColour, lfWeightB1 );
    BlendVec4( mv3SkyHorColour,
               lValueA0.mv3SkyHorColour, lfWeightA0, lValueA1.mv3SkyHorColour, lfWeightA1,
               lValueB0.mv3SkyHorColour, lfWeightB0, lValueB1.mv3SkyHorColour, lfWeightB1 );
    BlendVec4( mv3SkySunColour,
               lValueA0.mv3SkySunColour, lfWeightA0, lValueA1.mv3SkySunColour, lfWeightA1,
               lValueB0.mv3SkySunColour, lfWeightB0, lValueB1.mv3SkySunColour, lfWeightB1 );

    // +0x30 .. +0x44 -- the six sky shape scalars.
    mfSkyHorPow      = lValueA0.mfSkyHorPow * lfWeightA0 + lValueA1.mfSkyHorPow * lfWeightA1
                     + lValueB0.mfSkyHorPow * lfWeightB0 + lValueB1.mfSkyHorPow * lfWeightB1;
    mfSkySunPow      = lValueA0.mfSkySunPow * lfWeightA0 + lValueA1.mfSkySunPow * lfWeightA1
                     + lValueB0.mfSkySunPow * lfWeightB0 + lValueB1.mfSkySunPow * lfWeightB1;
    mfSkyDrk         = lValueA0.mfSkyDrk * lfWeightA0 + lValueA1.mfSkyDrk * lfWeightA1
                     + lValueB0.mfSkyDrk * lfWeightB0 + lValueB1.mfSkyDrk * lfWeightB1;
    mfSkyHorBleedScl = lValueA0.mfSkyHorBleedScl * lfWeightA0 + lValueA1.mfSkyHorBleedScl * lfWeightA1
                     + lValueB0.mfSkyHorBleedScl * lfWeightB0 + lValueB1.mfSkyHorBleedScl * lfWeightB1;
    mfSkyHorBleedPow = lValueA0.mfSkyHorBleedPow * lfWeightA0 + lValueA1.mfSkyHorBleedPow * lfWeightA1
                     + lValueB0.mfSkyHorBleedPow * lfWeightB0 + lValueB1.mfSkyHorBleedPow * lfWeightB1;
    mfSkySunBleedPow = lValueA0.mfSkySunBleedPow * lfWeightA0 + lValueA1.mfSkySunBleedPow * lfWeightA1
                     + lValueB0.mfSkySunBleedPow * lfWeightB0 + lValueB1.mfSkySunBleedPow * lfWeightB1;

    // mPad48 (+0x48..+0x4F) is deliberately left alone -- the guest never writes it.

    // +0x50 / +0x60 / +0x70 -- the three scattering colours.
    BlendVec4( mv3ScattTopColour,
               lValueA0.mv3ScattTopColour, lfWeightA0, lValueA1.mv3ScattTopColour, lfWeightA1,
               lValueB0.mv3ScattTopColour, lfWeightB0, lValueB1.mv3ScattTopColour, lfWeightB1 );
    BlendVec4( mv3ScattHorColour,
               lValueA0.mv3ScattHorColour, lfWeightA0, lValueA1.mv3ScattHorColour, lfWeightA1,
               lValueB0.mv3ScattHorColour, lfWeightB0, lValueB1.mv3ScattHorColour, lfWeightB1 );
    BlendVec4( mv3ScattSunColour,
               lValueA0.mv3ScattSunColour, lfWeightA0, lValueA1.mv3ScattSunColour, lfWeightA1,
               lValueB0.mv3ScattSunColour, lfWeightB0, lValueB1.mv3ScattSunColour, lfWeightB1 );

    // +0x80 .. +0xA4 -- the ten scattering scalars.
    mfScattHorPow      = lValueA0.mfScattHorPow * lfWeightA0 + lValueA1.mfScattHorPow * lfWeightA1
                       + lValueB0.mfScattHorPow * lfWeightB0 + lValueB1.mfScattHorPow * lfWeightB1;
    mfScattSunPow      = lValueA0.mfScattSunPow * lfWeightA0 + lValueA1.mfScattSunPow * lfWeightA1
                       + lValueB0.mfScattSunPow * lfWeightB0 + lValueB1.mfScattSunPow * lfWeightB1;
    mfScattDrk         = lValueA0.mfScattDrk * lfWeightA0 + lValueA1.mfScattDrk * lfWeightA1
                       + lValueB0.mfScattDrk * lfWeightB0 + lValueB1.mfScattDrk * lfWeightB1;
    mfScattHorBleedScl = lValueA0.mfScattHorBleedScl * lfWeightA0 + lValueA1.mfScattHorBleedScl * lfWeightA1
                       + lValueB0.mfScattHorBleedScl * lfWeightB0 + lValueB1.mfScattHorBleedScl * lfWeightB1;
    mfScattHorBleedPow = lValueA0.mfScattHorBleedPow * lfWeightA0 + lValueA1.mfScattHorBleedPow * lfWeightA1
                       + lValueB0.mfScattHorBleedPow * lfWeightB0 + lValueB1.mfScattHorBleedPow * lfWeightB1;
    mfScattSunBleedPow = lValueA0.mfScattSunBleedPow * lfWeightA0 + lValueA1.mfScattSunBleedPow * lfWeightA1
                       + lValueB0.mfScattSunBleedPow * lfWeightB0 + lValueB1.mfScattSunBleedPow * lfWeightB1;
    mafScattDist[0]    = lValueA0.mafScattDist[0] * lfWeightA0 + lValueA1.mafScattDist[0] * lfWeightA1
                       + lValueB0.mafScattDist[0] * lfWeightB0 + lValueB1.mafScattDist[0] * lfWeightB1;
    mafScattDist[1]    = lValueA0.mafScattDist[1] * lfWeightA0 + lValueA1.mafScattDist[1] * lfWeightA1
                       + lValueB0.mafScattDist[1] * lfWeightB0 + lValueB1.mafScattDist[1] * lfWeightB1;
    mfScattPow         = lValueA0.mfScattPow * lfWeightA0 + lValueA1.mfScattPow * lfWeightA1
                       + lValueB0.mfScattPow * lfWeightB0 + lValueB1.mfScattPow * lfWeightB1;
    mfScattCap         = lValueA0.mfScattCap * lfWeightA0 + lValueA1.mfScattCap * lfWeightA1
                       + lValueB0.mfScattCap * lfWeightB0 + lValueB1.mfScattCap * lfWeightB1;
}
}
}
