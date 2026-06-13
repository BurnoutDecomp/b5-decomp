#ifndef BRN_ENV_LIGHTING_DATA_H
#define BRN_ENV_LIGHTING_DATA_H

#include "types.hpp"

namespace BrnWorld
{
namespace EnvironmentSettings
{
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x????????.
// Default lighting keyframe: eight 4-float vectors copied from the module's
// static default template, plus one scalar override.
class LightingData
{
public:
    void Construct();

private:
    float mVec[8][4];  // guest 0..128, seeded from the static template
    float mfField80;   // guest +128
};
}
}

#endif
