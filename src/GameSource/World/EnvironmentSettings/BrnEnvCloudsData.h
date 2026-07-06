#ifndef BRN_ENV_CLOUDS_DATA_H
#define BRN_ENV_CLOUDS_DATA_H

#include "types.hpp"

namespace BrnWorld
{
namespace EnvironmentSettings
{
// Reconstructed from BURNOUT_X360_ARTIST.XEX.
// Default clouds keyframe: two per-layer colour arrays copied from the module's
// static default template, followed by explicit per-layer float-pair parameters.
// Field names/types per burnout.wiki (Environment Keyframe ->
// BrnWorld::EnvironmentSettings::CloudsData); offsets verified against the X360
// pseudocode.
class CloudsData
{
public:
    void Construct();

    // Blend keyframes into this one (element-wise weighted sum over the 27-float layout).
    // 2-way @ 0x82675... (not this batch); 4-way @ 0x82675FC0.
    void SetToBlend( const CloudsData& lValueA, float lfWeightA,
                     const CloudsData& lValueB, float lfWeightB );
    void SetToBlend( const CloudsData& lValueA0, float lfWeightA0,
                     const CloudsData& lValueA1, float lfWeightA1,
                     const CloudsData& lValueB0, float lfWeightB0,
                     const CloudsData& lValueB1, float lfWeightB1 );

private:
    float mav3LayerLiteColour[2][4]; // 0x00 (Vector3[2])
    float mav3LayerDarkColour[2][4]; // 0x20 (Vector3[2])
    float mafLayerDensity[2];        // 0x40
    float mafLayerFeathering[2];     // 0x48
    float mafLayerOpacity[2];        // 0x50
    float mafLayerSpeed[2];          // 0x58
    float mafLayerScale[2];          // 0x60
    float mfDirectionAngle;          // 0x68
};
}
}

#endif
