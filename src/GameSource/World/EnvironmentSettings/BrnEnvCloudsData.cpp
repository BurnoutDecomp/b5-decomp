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
// 0x82FFADF0 — the 0x40-byte default colour block: lite[2] then dark[2].
extern const float KAF_CloudsDefaultColours[16];

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
