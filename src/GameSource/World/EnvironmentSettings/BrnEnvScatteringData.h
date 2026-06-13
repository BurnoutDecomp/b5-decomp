#ifndef BRN_ENV_SCATTERING_DATA_H
#define BRN_ENV_SCATTERING_DATA_H

#include "types.hpp"

namespace BrnWorld
{
namespace EnvironmentSettings
{
// Reconstructed from BURNOUT_X360_ARTIST.XEX.
// Default atmospheric-scattering keyframe: six colour vectors copied from the
// module's static default template, interleaved with explicit scalar fields.
// Field names/types per burnout.wiki (Environment Keyframe ->
// BrnWorld::EnvironmentSettings::ScatteringData); offsets verified against the
// X360 pseudocode.
class ScatteringData
{
public:
    void Construct();

private:
    float mv3SkyTopColour[4];     // 0x00 (Vector3)
    float mv3SkyHorColour[4];     // 0x10 (Vector3)
    float mv3SkySunColour[4];     // 0x20 (Vector3)
    float mfSkyHorPow;            // 0x30
    float mfSkySunPow;            // 0x34
    float mfSkyDrk;               // 0x38
    float mfSkyHorBleedScl;       // 0x3C
    float mfSkyHorBleedPow;       // 0x40
    float mfSkySunBleedPow;       // 0x44
    u8    mPad48[8];              // 0x48 padding
    float mv3ScattTopColour[4];   // 0x50 (Vector3)
    float mv3ScattHorColour[4];   // 0x60 (Vector3)
    float mv3ScattSunColour[4];   // 0x70 (Vector3)
    float mfScattHorPow;          // 0x80
    float mfScattSunPow;          // 0x84
    float mfScattDrk;             // 0x88
    float mfScattHorBleedScl;     // 0x8C
    float mfScattHorBleedPow;     // 0x90
    float mfScattSunBleedPow;     // 0x94
    float mafScattDist[2];        // 0x98
    float mfScattPow;             // 0xA0
    float mfScattCap;             // 0xA4
};
}
}

#endif
