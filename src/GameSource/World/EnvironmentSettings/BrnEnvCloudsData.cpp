#include "BrnEnvCloudsData.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnWorld::EnvironmentSettings::CloudsData::Construct
//
// Copies the two default per-layer cloud colour arrays from the module's static
// default template, then fills the explicit cloud parameter pairs (density,
// feathering, opacity, speed, scale) and the trailing direction angle. The guest
// walked the template through embedded pointers; that walk reduces to a
// contiguous copy of the recovered default colour block, referenced by its data
// symbol (defined with the template data TU).

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
//
// CORRECTION (same round): the previous comment called this "the 0x40-byte block at
// 0x82FFADF0". It is TWO separate 0x20-byte blocks. CloudsData::Construct @0x82675190
// does eight `ld` pairs: 0x82FFADF0/ADF8/AE00/AE08 -> this+0x00..0x1F (the two LITE
// colours) and 0x82FFB060/B068/B070/B078 -> this+0x20..0x3F (the two DARK colours).
// The indices this file already uses (0..7 lite, 8..15 dark) are therefore right; only
// the address in the comment was wrong.
// ---------------------------------------------------------------------------
namespace
{
const float KAF_CloudsDefaultColours[16] =
{
    // lite[0] : unk_82FFADF0 <- init @0x82C610F0
    0.8f, 0.8f, 0.8f, 0.0f,
    // lite[1] : unk_82FFAE00 <- same initialiser, stvx128 at +0x10 (@0x82C610FC)
    0.8f, 0.8f, 0.8f, 0.0f,
    // dark[0] : unk_82FFB060 <- init @0x82C61150
    0.6f, 0.6f, 0.6f, 0.0f,
    // dark[1] : unk_82FFB070 <- same initialiser, stvx128 at +0x10 (@0x82C6115C)
    0.6f, 0.6f, 0.6f, 0.0f,
};
}

void CloudsData::Construct()
{
    for (int i = 0; i < 4; ++i)
    {
        mav3LayerLiteColour[0][i] = KAF_CloudsDefaultColours[i];
        mav3LayerLiteColour[1][i] = KAF_CloudsDefaultColours[4 + i];
        mav3LayerDarkColour[0][i] = KAF_CloudsDefaultColours[8 + i];
        mav3LayerDarkColour[1][i] = KAF_CloudsDefaultColours[12 + i];
    }

    mafLayerDensity[0]    = 0.6f;    mafLayerDensity[1]    = 0.6f;
    mafLayerFeathering[0] = 0.2f;    mafLayerFeathering[1] = 0.2f;
    mafLayerOpacity[0]    = 1.0f;    mafLayerOpacity[1]    = 1.0f;
    mafLayerSpeed[0]      = 30.0f;   mafLayerSpeed[1]      = 30.0f;
    mafLayerScale[0]      = 7000.0f; mafLayerScale[1]      = 7000.0f;
    mfDirectionAngle      = 0.0f;
}

// ---------------------------------------------------------------------------
// SetToBlend (4-way) @ 0x82675FC0
//
// Sets this keyframe to the weighted sum of four source keyframes:
//     *this = lValueA0 * lfWeightA0 + lValueA1 * lfWeightA1
//           + lValueB0 * lfWeightB0 + lValueB1 * lfWeightB1
// applied element-wise across every float field of CloudsData (the two colour
// arrays and every per-layer parameter, including mfDirectionAngle). The guest
// blends the 0x40-byte colour block with a two-iteration SIMD unroll and the
// trailing scalar pairs with paired FPU multiply-adds; the net effect is this
// uniform element-wise weighted sum over the 0x00..0x68 float layout.
void CloudsData::SetToBlend( const CloudsData& lValueA0, float lfWeightA0,
                            const CloudsData& lValueA1, float lfWeightA1,
                            const CloudsData& lValueB0, float lfWeightB0,
                            const CloudsData& lValueB1, float lfWeightB1 )
{
    float*       lpfDst = reinterpret_cast<float*>( this );
    const float* lpfA0  = reinterpret_cast<const float*>( &lValueA0 );
    const float* lpfA1  = reinterpret_cast<const float*>( &lValueA1 );
    const float* lpfB0  = reinterpret_cast<const float*>( &lValueB0 );
    const float* lpfB1  = reinterpret_cast<const float*>( &lValueB1 );

    // 0x6C bytes / 4 == 27 floats: colour arrays (0x00..0x3F) + parameter
    // pairs (0x40..0x67) + mfDirectionAngle (0x68).
    for ( int i = 0; i < 27; ++i )
    {
        lpfDst[i] = lpfA0[i] * lfWeightA0 + lpfA1[i] * lfWeightA1
                  + lpfB0[i] * lfWeightB0 + lpfB1[i] * lfWeightB1;
    }
}
}
}
