#include "BrnEnvLightingData.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnWorld::EnvironmentSettings::LightingData::Construct
//
// Copies the eight default lighting vectors from the module's static default
// template into the keyframe, then applies the scalar override at +128. The
// guest used SIMD loads/stores from the data segment; the source vectors are
// referenced here by their recovered data symbols (defined with the template
// data TU).

namespace BrnWorld
{
namespace EnvironmentSettings
{
// Static default lighting vectors recovered from the data segment.
extern const float KAF_LightingDefault0[4]; // 0x82FFAEE0
extern const float KAF_LightingDefault1[4]; // 0x82FFB090
extern const float KAF_LightingDefault2[4]; // 0x82FFB1F0
extern const float KAF_LightingDefault3[4]; // 0x82FFB040
extern const float KAF_LightingDefault4[4]; // 0x82FFB050
extern const float KAF_LightingDefault5[4]; // 0x82FFB010
extern const float KAF_LightingDefault6[4]; // 0x82FFAE10
extern const float KAF_LightingDefault7[4]; // 0x82FFAE40

namespace
{
    void CopyVec4(float* lpDst, const float* lpSrc)
    {
        for (int i = 0; i < 4; ++i)
        {
            lpDst[i] = lpSrc[i];
        }
    }
}

void LightingData::Construct()
{
    CopyVec4(mVec[0], KAF_LightingDefault0);
    CopyVec4(mVec[1], KAF_LightingDefault1);
    CopyVec4(mVec[2], KAF_LightingDefault2);
    CopyVec4(mVec[3], KAF_LightingDefault3);
    CopyVec4(mVec[4], KAF_LightingDefault4);
    CopyVec4(mVec[5], KAF_LightingDefault5);
    CopyVec4(mVec[6], KAF_LightingDefault6);

    mfField80 = 0.2f;

    CopyVec4(mVec[7], KAF_LightingDefault7);
}
}
}
