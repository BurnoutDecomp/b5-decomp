// FX-GATE item 6, the physics sites: the clamp statements EXTRACTED from each crash / drift physics site (between the
// Dot line and the XMVectorACos call; run_fxgate_physics_acos.py) against the console's clamp, and the angle each
// clamped value then gets from XboxMath::XMVectorACos (X360 0x821F0980).
//   vmx:  vmaxfp against -1.0, then vminfp against +1.0 -- a NaN operand comes back, so a NaN dot stays NaN.
//   fsel: rw::math::fpu::Clamp (0x825D3614..0x825D3624, Max = fsel on -1 - dot, Min = fsel on 1 - v) -- a NaN
//         dot comes out as 1.0.
#include "types.hpp"
#include "rw/math/fpu/scalar_operation.h"
#include "SDKs/XboxMath/XMVectorACos.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace
{
    typedef float (*ClampFn)(float);
    struct ClampSite { const char* mpcName; ClampFn mpFn; bool mbFsel; };
}

#include "fxgate_physics_acos.inc"

namespace
{
    unsigned gChecks = 0, gFailures = 0;

    unsigned int Bits(float lf) { unsigned int lu; std::memcpy(&lu, &lf, 4); return lu; }

    float ConsoleClamp(float lfDot, bool lbFsel)
    {
        if (std::isnan(lfDot))
            return lbFsel ? 1.0f : lfDot;
        if (lfDot < -1.0f) return -1.0f;
        if (lfDot > 1.0f) return 1.0f;
        return lfDot;
    }

    bool Same(float lfA, float lfB)
    {
        return (std::isnan(lfA) && std::isnan(lfB)) || Bits(lfA) == Bits(lfB)
            || (lfA == 0.0f && lfB == 0.0f);   // the sign of a zero is lost in acos anyway
    }
}

int main()
{
    const float kfNaN = std::numeric_limits<float>::quiet_NaN();
    const float kfInf = std::numeric_limits<float>::infinity();
    const float kaInputs[] = { kfNaN, -kfNaN, kfInf, -kfInf, 2.0f, -2.0f, 1.00000012f, -1.00000012f,
                               1.0f, -1.0f, 0.99999994f, 0.5f, -0.5f, 0.0f };

    for (const ClampSite& lrSite : kaSites)
    {
        for (float lfIn : kaInputs)
        {
            const float lfActual = lrSite.mpFn(lfIn);
            const float lfWant = ConsoleClamp(lfIn, lrSite.mbFsel);
            ++gChecks;
            if (!Same(lfActual, lfWant))
            {
                ++gFailures;
                std::printf("FAIL  %s clamp(0x%08X) = 0x%08X (%.9g), console 0x%08X (%.9g)\n", lrSite.mpcName,
                            Bits(lfIn), Bits(lfActual), lfActual, Bits(lfWant), lfWant);
            }
        }

        // The angle a NaN dot ends with: NaN through vmaxfp / vminfp, 4.8e-7 (XMVectorACos(1)) through the fsel pair.
        const float lfAngle = XboxMath::XMVectorACos(lrSite.mpFn(kfNaN));
        ++gChecks;
        const bool lbOk = lrSite.mbFsel ? (Bits(lfAngle) == 0x35000000u) : std::isnan(lfAngle);
        if (!lbOk)
        {
            ++gFailures;
            std::printf("FAIL  %s: a NaN dot gives the angle 0x%08X; console %s\n", lrSite.mpcName, Bits(lfAngle),
                        lrSite.mbFsel ? "0x35000000 (XMVectorACos(1.0))" : "NaN");
        }
    }

    std::printf("FxGatePhysicsACos: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
