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
}
}
