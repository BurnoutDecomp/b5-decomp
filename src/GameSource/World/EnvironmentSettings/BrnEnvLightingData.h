#ifndef BRN_ENV_LIGHTING_DATA_H
#define BRN_ENV_LIGHTING_DATA_H

#include "types.hpp"

namespace BrnWorld
{
namespace EnvironmentSettings
{
// Reconstructed from BURNOUT_X360_ARTIST.XEX.
// Default lighting keyframe: eight colour vectors copied from the module's
// static default template, plus one scalar override. Field names/types per
// burnout.wiki (Environment Keyframe ->
// BrnWorld::EnvironmentSettings::LightingData); offsets verified against the
// X360 pseudocode.
class LightingData
{
public:
    void Construct();

    // 4-way per-member weighted blend @ 0x827AFAA8 (body beside Construct in
    // BrnEnvLightingData.cpp since the envfix round, 2026-08-16 -- the address in
    // the previous comment was a placeholder). Sets this keyframe to the
    // element-wise weighted sum
    //   *this = A0*wA0 + A1*wA1 + B0*wB0 + B1*wB1
    // over ALL 33 floats of the layout (no holes here). Matches the CloudsData
    // 4-way form. There is NO 2-way overload in BURNOUT_X360_ARTIST.XEX.
    void SetToBlend( const LightingData& lValueA0, float lfWeightA0,
                     const LightingData& lValueA1, float lfWeightA1,
                     const LightingData& lValueB0, float lfWeightB0,
                     const LightingData& lValueB1, float lfWeightB1 );

    // PUBLIC per the DWARF (BrnEnvironmentData.h:137-146 are struct members with no access
    // specifier); EnvironmentManager::GenerateShaderConstants @0x827D0098 reads them by name.
public:
    float mv3KeyLightColour[4];      // 0x00 (Vector3)
    float mv3SpecularColour[4];      // 0x10 (Vector3)
    float mv3KeyFillColour[4];       // 0x20 (Vector3)
    float mv3ShadowFillColour[4];    // 0x30 (Vector3)
    float mv3RightFillColour[4];     // 0x40 (Vector3)
    float mv3LeftFillColour[4];      // 0x50 (Vector3)
    float mv3UpFillColour[4];        // 0x60 (Vector3)
    float mv3DownFillColour[4];      // 0x70 (Vector3)
    float mfAmbientIrradianceScale;  // 0x80
};
}
}

#endif
