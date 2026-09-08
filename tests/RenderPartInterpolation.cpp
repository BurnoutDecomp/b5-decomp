// Run with run_render_part_interpolation.py. No game assets or window required.
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int)
{
    std::fprintf(stderr, "%s\n", lpcMessage);
    std::abort();
}
void* EndAssert() { return nullptr; }
} }

static void Check(float lfActual, float lfExpected)
{
    if (std::fabs(lfActual - lfExpected) > 0.0001f)
    {
        std::fprintf(stderr, "Expected %f, got %f\n", lfExpected, lfActual);
        std::abort();
    }
}

static BrnWorld::DetachedPartRenderEvent Part(s32 liIndex, float lfX, bool lbAttached)
{
    BrnWorld::DetachedPartRenderEvent lPart = {};
    lPart.mTransform.xAxis.x = 1;
    lPart.mTransform.yAxis.y = 1;
    lPart.mTransform.zAxis.z = 1;
    lPart.mTransform.wAxis.x = lfX;
    lPart.miPartIndex = liIndex;
    lPart.mbIsAttached = lbAttached;
    return lPart;
}

int main()
{
    static BrnWorld::ActiveRaceCar lCar;
    lCar.ResetRenderPoseInterpolation();
    auto* lpParams = lCar.GetRenderParams();
    lpParams->SetBodyTransform(Part(0, 0, true).mTransform);
    for (u32 luWheel = 0; luWheel < 6; ++luWheel)
        lpParams->GetWheelTransform(luWheel) = Part(0, 0, true).mTransform;
    auto& lrParts = lpParams->GetDetachedPartQueue();
    lrParts.Construct();
    lrParts.AddEvent(Part(7, 10, true));
    lrParts.AddEvent(Part(95, 100, false));
    lCar.LatchTickRenderPose();
    lCar.ApplyRenderPoseInterpolation(0.5f);
    Check(lrParts.GetEvent(0).mTransform.wAxis.x, 10);

    // Queue order changes; an attached panel becomes detached without changing identity.
    lCar.RestoreTickRenderPose();
    lrParts.Clear();
    lrParts.AddEvent(Part(95, 120, false));
    lrParts.AddEvent(Part(7, 20, false));
    lCar.LatchTickRenderPose();
    for (float lfAlpha : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f})
    {
        lCar.ApplyRenderPoseInterpolation(lfAlpha);
        Check(lrParts.GetEvent(0).mTransform.wAxis.x, 100 + 20 * lfAlpha);
        Check(lrParts.GetEvent(1).mTransform.wAxis.x, 10 + 10 * lfAlpha);
        lCar.ApplyRenderPoseInterpolation(lfAlpha); // repeated render pass is idempotent
        Check(lrParts.GetEvent(1).mTransform.wAxis.x, 10 + 10 * lfAlpha);
    }

    // An unwritten/paused tick restores the real endpoint before latching.
    lCar.ApplyRenderPoseInterpolation(0.25f);
    lCar.RestoreTickRenderPose();
    Check(lrParts.GetEvent(1).mTransform.wAxis.x, 20);
    lCar.LatchTickRenderPose();
    lCar.ApplyRenderPoseInterpolation(0.25f);
    Check(lrParts.GetEvent(1).mTransform.wAxis.x, 20);

    // Removal and later reuse must not interpolate from the old panel.
    lCar.RestoreTickRenderPose();
    lrParts.Clear();
    lCar.LatchTickRenderPose();
    lrParts.AddEvent(Part(7, 500, true));
    lCar.LatchTickRenderPose();
    lCar.ApplyRenderPoseInterpolation(0.5f);
    Check(lrParts.GetEvent(0).mTransform.wAxis.x, 500);

    lCar.ResetRenderPoseInterpolation(); // teleport/car-slot reuse
    lrParts.GetEvent(0).mTransform.wAxis.x = 1000;
    lCar.LatchTickRenderPose();
    lCar.ApplyRenderPoseInterpolation(0.5f);
    Check(lrParts.GetEvent(0).mTransform.wAxis.x, 1000);
    std::puts("Render part interpolation: PASS");
}
