#include "BrnEnvCloudsData.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnWorld::EnvironmentSettings::CloudsData::Construct
//
// Copies the eight-quadword default header from the module's static default
// template, then fills the explicit cloud parameter pairs (density, coverage,
// scale, height, distance) and the trailing scalar. The guest walked the
// template through embedded pointers; that walk reduces to a contiguous copy of
// the recovered default block, referenced by its data symbol (defined with the
// template data TU).

namespace BrnWorld
{
namespace EnvironmentSettings
{
extern const u64 KAQ_CloudsDefaultHeader[8]; // 0x82FFADF0

void CloudsData::Construct()
{
    for (int i = 0; i < 8; ++i)
    {
        mHead[i] = KAQ_CloudsDefaultHeader[i];
    }

    mDensity[0] = 0.6f;    mDensity[1] = 0.6f;
    mField72[0] = 0.2f;    mField72[1] = 0.2f;
    mField80[0] = 1.0f;    mField80[1] = 1.0f;
    mField88[0] = 30.0f;   mField88[1] = 30.0f;
    mField96[0] = 7000.0f; mField96[1] = 7000.0f;
    mfField104  = 0.0f;
}
}
}
