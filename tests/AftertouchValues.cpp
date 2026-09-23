// Harness for run_aftertouch_values.py: links the shipped BrnPlayerDriverControls.cpp and checks
// GetAftertouchValues @0x825B2E88 against the console leaf (crash parity G47-D1): the Y axis
// selects on the controls' own mbIsSteeringWheel (+0x41), and the trailing bool is never read.
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"
#include <cmath>
#include <cstdio>
#include <cstring>

int main()
{
    using BrnPhysics::Vehicle::BrnPlayerDriverControls;
    int liChecks = 0, liFailures = 0;
    auto Check = [&](bool lbPass, const char* lpcName) {
        ++liChecks;
        if (!lbPass) { ++liFailures; std::printf("FAIL: %s\n", lpcName); }
    };
    auto Near = [](float a, float b) { return std::fabs(a - b) < 1e-6f; };

    alignas(16) unsigned char laStorage[sizeof(BrnPlayerDriverControls)];
    std::memset(laStorage, 0, sizeof(laStorage));
    BrnPlayerDriverControls& lrControls = *reinterpret_cast<BrnPlayerDriverControls*>(laStorage);
    lrControls.mfSteering        = 0.8f;
    lrControls.mfForwardSteering = 0.6f;
    lrControls.mfRequestedGas    = 1.0f;
    lrControls.mfBrake           = 0.2f;

    for (int liArg = 0; liArg < 2; ++liArg)
    {
        float x = -9.0f, y = -9.0f, z = -9.0f;
        lrControls.mbIsSteeringWheel = false;
        lrControls.GetAftertouchValues(x, y, z, liArg != 0);
        Check(Near(x, 0.8f * 0.25f), "X = steering * 0.25");
        Check(Near(y, 0.6f * -0.25f), "pad: Y = forward steering * -0.25, whatever the bool argument");
        Check(z == 0.0f, "Z = 0");

        lrControls.mbIsSteeringWheel = true;
        lrControls.GetAftertouchValues(x, y, z, liArg != 0);
        Check(Near(y, (1.0f - 0.2f) * -0.25f), "wheel: Y = (requested gas - brake) * -0.25, whatever the bool argument");
    }
    std::printf("AftertouchValues: %d checks, %d failures\n", liChecks, liFailures);
    return liFailures ? 1 : 0;
}
