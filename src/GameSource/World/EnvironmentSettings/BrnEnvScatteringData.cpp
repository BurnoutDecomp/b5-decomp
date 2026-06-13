#include "BrnEnvScatteringData.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnWorld::EnvironmentSettings::ScatteringData::Construct
//
// Copies six default scattering vectors from the module's static default
// template and fills the explicit scalar fields. The guest used SIMD
// loads/stores; the source vectors are referenced here by their recovered data
// symbols (defined with the template data TU).

namespace BrnWorld
{
namespace EnvironmentSettings
{
extern const float KAF_ScatteringDefault0[4]; // 0x82FFB170
extern const float KAF_ScatteringDefault1[4]; // 0x82FFB000
extern const float KAF_ScatteringDefault2[4]; // 0x82FFB080
extern const float KAF_ScatteringDefault3[4]; // 0x82FFB020
extern const float KAF_ScatteringDefault4[4]; // 0x82FFB200
extern const float KAF_ScatteringDefault5[4]; // 0x82FFAE30

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

void ScatteringData::Construct()
{
    CopyVec4(mVec0,  KAF_ScatteringDefault0);
    CopyVec4(mVec16, KAF_ScatteringDefault1);
    CopyVec4(mVec32, KAF_ScatteringDefault2);

    mField30[0] = 0.5f;  // field_30
    mField30[1] = 9.0f;  // field_34
    mField30[2] = 0.0f;  // field_38
    mField30[3] = 13.0f; // field_3C
    mField30[4] = 9.0f;  // field_40
    mField30[5] = 7.0f;  // field_44

    CopyVec4(mVec80,  KAF_ScatteringDefault3);
    CopyVec4(mVec96,  KAF_ScatteringDefault4);
    CopyVec4(mVec112, KAF_ScatteringDefault5);

    mField80[0] = 0.5f;  // field_80
    mField80[1] = 9.0f;  // field_84
    mField80[2] = 0.0f;  // field_88
    mField80[3] = 13.0f; // field_8C
    mField80[4] = 9.0f;  // field_90
    mField80[5] = 14.0f; // field_94

    mField98 = 0x42C8000045BB8000ull; // {6000.0f, 100.0f}
    mfFieldA0 = 0.31f;
    mfFieldA4 = 0.89999998f;
}
}
}
