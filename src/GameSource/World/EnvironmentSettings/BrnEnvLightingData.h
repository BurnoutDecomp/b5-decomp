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

private:
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
