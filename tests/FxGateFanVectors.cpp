// FX-GATE (crash parity 2026-09-25): SteeringFan::GenerateFanVectors' loop body, EXTRACTED from the production source
// by run_fxgate_fan_vectors.py (fxgate_fan_vectors.inc), against the console's own words 0x82779474..0x8277972C run
// on emu64 (FxGateFanVectorsData.h): the fmadds angle (0x8277948C), the inlined XMVectorSinCos (0x8277955C..
// 0x827796BC) and the two vmaddfp target stores (0x827796F4 / 0x82779724).
#include "types.hpp"
#include "SDKs/XboxMath/XMVectorSinCos.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#define CGS_ASSERT(condition, message) ((void)0)

namespace
{
    const s32 KI_FAN_STEPS = 17;
    struct Vector2 { f32 x, y, z, w; };

#include "fxgate_fan_vectors.inc"   // struct FanFixture { members ; void Body(f32 lfT, f32 lfBaseAngle, s32 liStep) }
}

#include "FxGateFanVectorsData.h"

namespace
{
    unsigned Bits(float lf) { unsigned lu; std::memcpy(&lu, &lf, 4); return lu; }
    float Float(unsigned lu) { float lf; std::memcpy(&lf, &lu, 4); return lf; }
}

int main()
{
    unsigned luChecks = 0, luFailures = 0, luDiffering = 0;
    static const char* const kapcNames[6] = { "unit.x", "unit.y", "target.x", "target.y", "hng.x", "hng.y" };
    for (const FanVectorsRow& lrRow : kaFanVectorsRows)
    {
        FanFixture lFan;
        std::memset(&lFan, 0, sizeof(lFan));
        lFan.mfFanAngle = Float(lrRow.mauIn[1]);
        lFan.mFanOrigin2D.x = Float(lrRow.mauIn[3]);
        lFan.mFanOrigin2D.y = Float(lrRow.mauIn[4]);
        lFan.mfLookAheadRadius = Float(lrRow.mauIn[5]);
        lFan.mfLookAheadHNGRadius = Float(lrRow.mauIn[6]);
        lFan.Body(Float(lrRow.mauIn[0]), Float(lrRow.mauIn[2]), lrRow.miStep);

        const s32 liStep = lrRow.miStep;
        const float lafActual[6] = { lFan.mUnitDirection[liStep].x, lFan.mUnitDirection[liStep].y,
                                     lFan.mTarget[liStep].x, lFan.mTarget[liStep].y,
                                     lFan.mHNGTarget[liStep].x, lFan.mHNGTarget[liStep].y };
        ++luChecks;
        luDiffering += lrRow.mbDiffers ? 1u : 0u;
        bool lbOk = true;
        for (int liOut = 0; liOut < 6; ++liOut)
        {
            if (Bits(lafActual[liOut]) != lrRow.mauOut[liOut])
            {
                if (lbOk && luFailures < 6)
                    std::printf("FAIL  row lfT %g step %d: %s = 0x%08X (%.9g), console 0x%08X (%.9g)\n",
                                Float(lrRow.mauIn[0]), liStep, kapcNames[liOut], Bits(lafActual[liOut]),
                                lafActual[liOut], lrRow.mauOut[liOut], Float(lrRow.mauOut[liOut]));
                lbOk = false;
            }
        }
        if (!lbOk)
            ++luFailures;
    }
    std::printf("%u rows, %u where the pre-fix spelling differs\n", luChecks, luDiffering);
    std::printf("FxGateFanVectors: %u checks, %u failures\n", luChecks, luFailures);
    return luFailures == 0 ? 0 : 1;
}
