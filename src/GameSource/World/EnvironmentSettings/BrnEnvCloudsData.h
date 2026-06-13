#ifndef BRN_ENV_CLOUDS_DATA_H
#define BRN_ENV_CLOUDS_DATA_H

#include "types.hpp"

namespace BrnWorld
{
namespace EnvironmentSettings
{
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x????????.
// Default clouds keyframe: a header block copied from the module's static
// default template, followed by explicit float-pair parameters.
class CloudsData
{
public:
    void Construct();

private:
    u64   mHead[8];     // guest 0..64, seeded from the static template
    float mDensity[2];  // guest +64  {0.6, 0.6}
    float mField72[2];  // guest +72  {0.2, 0.2}
    float mField80[2];  // guest +80  {1.0, 1.0}
    float mField88[2];  // guest +88  {30.0, 30.0}
    float mField96[2];  // guest +96  {7000.0, 7000.0}
    float mfField104;   // guest +104 {0.0}
};
}
}

#endif
