// FX-GATE (crash parity 2026-09-25): the steered wheel direction R(Up, angle) * At in VehiclePhysics::
// SetWheelVelocities and VehiclePhysics::UpdateWheels, EXTRACTED from the production source (with the file-local
// SteeredDirection builder, when present) by run_fxgate_steered_direction.py, against the console's own words run
// on emu64 (FxGateSteeredDirectionData.h: SetWheelVelocities 0x825FD2F4..0x825FD534, UpdateWheels
// 0x8261E524..0x8261E7A4).
#include "types.hpp"
#include "SDKs/XboxMath/XMVectorSinCos.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
    struct Vector3 { f32 x, y, z, w; };
    struct SteeringVector { f32 x, y, z, w; };

#include "fxgate_steered_direction.inc"   // [SteeredDirection] + K_SetWheelVelocities / K_UpdateWheels
}

#include "FxGateSteeredDirectionData.h"

namespace
{
    unsigned Bits(float lf) { unsigned lu; std::memcpy(&lu, &lf, 4); return lu; }
    float Float(unsigned lu) { float lf; std::memcpy(&lf, &lu, 4); return lf; }
}

int main()
{
    typedef void (*Kernel)(f32, const Vector3&, const Vector3&, Vector3*);
    const Kernel kaKernels[2] = { &K_SetWheelVelocities, &K_UpdateWheels };
    const char* const kapcNames[2] = { "SetWheelVelocities", "UpdateWheels" };
    unsigned luChecks = 0, luFailures = 0, luDiffering = 0;
    for (int liKernel = 0; liKernel < 2; ++liKernel)
    {
        unsigned luPrinted = 0;
        for (const SteeredDirectionRow& lrRow : kaSteeredDirectionRows)
        {
            const Vector3 lvUp = { Float(lrRow.mauIn[1]), Float(lrRow.mauIn[2]), Float(lrRow.mauIn[3]), 0.0f };
            const Vector3 lvAt = { Float(lrRow.mauIn[4]), Float(lrRow.mauIn[5]), Float(lrRow.mauIn[6]), 0.0f };
            Vector3 lvOut = { 0.0f, 0.0f, 0.0f, 0.0f };
            kaKernels[liKernel](Float(lrRow.mauIn[0]), lvUp, lvAt, &lvOut);
            ++luChecks;
            if (liKernel == 0 && lrRow.mbDiffers)
                ++luDiffering;
            const unsigned kauActual[3] = { Bits(lvOut.x), Bits(lvOut.y), Bits(lvOut.z) };
            if (kauActual[0] != lrRow.mauOut[0] || kauActual[1] != lrRow.mauOut[1] || kauActual[2] != lrRow.mauOut[2])
            {
                ++luFailures;
                if (luPrinted++ < 4)
                    std::printf("FAIL  %s angle %.9g: (0x%08X 0x%08X 0x%08X), console (0x%08X 0x%08X 0x%08X)\n",
                                kapcNames[liKernel], Float(lrRow.mauIn[0]), kauActual[0], kauActual[1], kauActual[2],
                                lrRow.mauOut[0], lrRow.mauOut[1], lrRow.mauOut[2]);
            }
        }
    }
    std::printf("%u rows where the pre-fix spelling differs from the console\n", luDiffering);
    std::printf("FxGateSteeredDirection: %u checks, %u failures\n", luChecks, luFailures);
    return luFailures == 0 ? 0 : 1;
}
