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
    CopyVec4(mv3SkyTopColour, KAF_ScatteringDefault0);
    CopyVec4(mv3SkyHorColour, KAF_ScatteringDefault1);
    CopyVec4(mv3SkySunColour, KAF_ScatteringDefault2);

    mfSkyHorPow      = 0.5f;
    mfSkySunPow      = 9.0f;
    mfSkyDrk         = 0.0f;
    mfSkyHorBleedScl = 13.0f;
    mfSkyHorBleedPow = 9.0f;
    mfSkySunBleedPow = 7.0f;

    CopyVec4(mv3ScattTopColour, KAF_ScatteringDefault3);
    CopyVec4(mv3ScattHorColour, KAF_ScatteringDefault4);
    CopyVec4(mv3ScattSunColour, KAF_ScatteringDefault5);

    mfScattHorPow      = 0.5f;
    mfScattSunPow      = 9.0f;
    mfScattDrk         = 0.0f;
    mfScattHorBleedScl = 13.0f;
    mfScattHorBleedPow = 9.0f;
    mfScattSunBleedPow = 14.0f;

    mafScattDist[0] = 100.0f;   // 0x98: high word of the guest's 0x42C8...0000 store
    mafScattDist[1] = 6000.0f;  // 0x9C: low word (0x...45BB8000)
    mfScattPow = 0.31f;
    mfScattCap = 0.89999998f;
}
}
}
