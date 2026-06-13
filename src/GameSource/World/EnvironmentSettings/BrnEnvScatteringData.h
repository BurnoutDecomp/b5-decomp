#ifndef BRN_ENV_SCATTERING_DATA_H
#define BRN_ENV_SCATTERING_DATA_H

#include "types.hpp"

namespace BrnWorld
{
namespace EnvironmentSettings
{
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x????????.
// Default atmospheric-scattering keyframe: six 4-float vectors copied from the
// module's static default template, interleaved with explicit scalar fields.
class ScatteringData
{
public:
    void Construct();

private:
    float mVec0[4];    // guest +0
    float mVec16[4];   // guest +16
    float mVec32[4];   // guest +32
    float mField30[6]; // guest +48 (field_30..field_44)
    u8    mPad0[8];     // guest +72..+80
    float mVec80[4];   // guest +80
    float mVec96[4];   // guest +96
    float mVec112[4];  // guest +112
    float mField80[6]; // guest +128 (field_80..field_94)
    u64   mField98;    // guest +152
    float mfFieldA0;   // guest +160
    float mfFieldA4;   // guest +164
};
}
}

#endif
