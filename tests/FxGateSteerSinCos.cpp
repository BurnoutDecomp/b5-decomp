// FX-GATE (crash parity 2026-09-25): the steering angle's sine / cosine in VehiclePhysics::SetWheelVelocities and
// VehiclePhysics::UpdateWheels, EXTRACTED from the production source by run_fxgate_steer_sincos.py
// (fxgate_steer_sincos.inc), against the console's own inlined XMVectorSinCos words run on emu64
// (FxGateSteerSinCosData.h: SetWheelVelocities 0x825FD2F4..0x825FD4B4, UpdateWheels 0x8261E524..0x8261E714).
#include "types.hpp"
#include "SDKs/XboxMath/XMVectorSinCos.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
    struct SteeringVector { f32 x, y, z, w; };

#include "fxgate_steer_sincos.inc"   // K_SetWheelVelocities / K_UpdateWheels (f32 angle, f32* sin, f32* cos)
}

#include "FxGateSteerSinCosData.h"

namespace
{
    unsigned Bits(float lf) { unsigned lu; std::memcpy(&lu, &lf, 4); return lu; }
    float Float(unsigned lu) { float lf; std::memcpy(&lf, &lu, 4); return lf; }
}

int main()
{
    typedef void (*Kernel)(f32, f32*, f32*);
    const Kernel kaKernels[2] = { &K_SetWheelVelocities, &K_UpdateWheels };
    const char* const kapcNames[2] = { "SetWheelVelocities", "UpdateWheels" };
    unsigned luChecks = 0, luFailures = 0, luDiffering = 0;
    for (int liKernel = 0; liKernel < 2; ++liKernel)
    {
        unsigned luPrinted = 0;
        for (const SteerSinCosRow& lrRow : kaSteerSinCosRows)
        {
            f32 lfSin = 0.0f, lfCos = 0.0f;
            kaKernels[liKernel](Float(lrRow.muAngle), &lfSin, &lfCos);
            ++luChecks;
            if (liKernel == 0 && lrRow.mbDiffers)
                ++luDiffering;
            if (Bits(lfSin) != lrRow.muSin || Bits(lfCos) != lrRow.muCos)
            {
                ++luFailures;
                if (luPrinted++ < 4)
                    std::printf("FAIL  %s angle 0x%08X (%.9g): sin/cos 0x%08X 0x%08X, console 0x%08X 0x%08X\n",
                                kapcNames[liKernel], lrRow.muAngle, Float(lrRow.muAngle), Bits(lfSin), Bits(lfCos),
                                lrRow.muSin, lrRow.muCos);
            }
        }
    }
    std::printf("%u rows where the correctly rounded sin / cos differ from the console's\n", luDiffering);
    std::printf("FxGateSteerSinCos: %u checks, %u failures\n", luChecks, luFailures);
    return luFailures == 0 ? 0 : 1;
}
