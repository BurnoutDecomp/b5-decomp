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
    if (!std::isfinite(lfActual) || std::fabs(lfActual - lfExpected) > 0.0001f)
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

static void CheckDeformationAndPanelStayTogether()
{
    static BrnWorld::ActiveRaceCar lCar;
    lCar.ResetRenderPoseInterpolation();
    auto* lpParams = lCar.GetRenderParams();
    auto* lpOffsets = lpParams->GetVerletOffsets();
    auto& lrParts = lpParams->GetDetachedPartQueue();
    lrParts.Construct();
    for (u32 luWheel = 0; luWheel < 6; ++luWheel)
        lpParams->GetWheelTransform(luWheel) = Part(0, 0, true).mTransform;
    for (u32 luPoint = 0; luPoint < BrnWorld::KU_MAX_RACE_CAR_VERLET_POINTS; ++luPoint)
        lpOffsets[luPoint] = {0.0f, 0.0f, 0.0f, 0.0f};

    // A vertex at x=2 blends skin rows 0 and 127, just as the vehicle shader does.
    // Its neighbouring hinged panel follows the same point. Both simulation
    // snapshots agree, so rendering between them must not open a seam.
    lpParams->SetBodyTransform(Part(0, 10, true).mTransform);
    lrParts.AddEvent(Part(7, 12, true));
    lCar.LatchTickRenderPose();
    lCar.ApplyRenderPoseInterpolation(0.5f); // first snapshot has no older damage
    Check(lpOffsets[127].x, 0.0f);
    lCar.RestoreTickRenderPose();
    lpParams->SetBodyTransform(Part(0, 11, true).mTransform);
    for (u32 luPoint = 1; luPoint < 127; ++luPoint)
    {
        const float lfDent = 0.001f * luPoint;
        lpOffsets[luPoint] = {lfDent, -2 * lfDent, 3 * lfDent, 4 * lfDent};
    }
    lpOffsets[0] = {0.08f, -0.06f, 0.12f, 0.2f};
    lpOffsets[127] = {0.24f, 0.02f, -0.04f, 0.6f};
    lrParts.GetEvent(0) = Part(7, 13.2f, true);
    lCar.LatchTickRenderPose();
    for (float lfAlpha : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f})
    {
        lCar.ApplyRenderPoseInterpolation(lfAlpha);
        const float lfSkinnedVertexX = lpParams->GetBodyTransform().wAxis.x + 2.0f
                                    + 0.25f * lpOffsets[0].x + 0.75f * lpOffsets[127].x;
        Check(lfSkinnedVertexX, lrParts.GetEvent(0).mTransform.wAxis.x);
        lCar.ApplyRenderPoseInterpolation(lfAlpha); // shadow/main passes cannot accumulate
        Check(lpOffsets[0].x, 0.08f * lfAlpha);
        Check(lpOffsets[0].y, -0.06f * lfAlpha);
        Check(lpOffsets[0].z, 0.12f * lfAlpha);
        Check(lpOffsets[0].w, 0.2f * lfAlpha);
        Check(lpOffsets[127].x, 0.24f * lfAlpha);
        Check(lpOffsets[127].w, 0.6f * lfAlpha);
        for (u32 luPoint = 1; luPoint < 127; ++luPoint)
        {
            const float lfDent = 0.001f * luPoint * lfAlpha;
            Check(lpOffsets[luPoint].x, lfDent);
            Check(lpOffsets[luPoint].y, -2 * lfDent);
            Check(lpOffsets[luPoint].z, 3 * lfDent);
            Check(lpOffsets[luPoint].w, 4 * lfDent);
        }
    }

    // No deformation output on the next tick: retain the last real shape.
    lCar.ApplyRenderPoseInterpolation(0.25f);
    lCar.RestoreTickRenderPose();
    Check(lpOffsets[0].x, 0.08f);
    Check(lpOffsets[127].w, 0.6f);
    lCar.LatchTickRenderPose();
    lCar.ApplyRenderPoseInterpolation(0.25f);
    Check(lpOffsets[0].x, 0.08f);
    Check(lpOffsets[127].w, 0.6f);

    // A repair between render frames must clear damage immediately and must
    // survive the next restore, including when physics publishes no replacement.
    lCar.BecomeActiveForReset();
    lCar.ResetVerletOffsets();
    lCar.ApplyRenderPoseInterpolation(0.25f);
    lCar.RestoreTickRenderPose();
    lCar.LatchTickRenderPose();
    lCar.ApplyRenderPoseInterpolation(0.25f);
    for (u32 luPoint = 0; luPoint < BrnWorld::KU_MAX_RACE_CAR_VERLET_POINTS; ++luPoint)
    {
        Check(lpOffsets[luPoint].x, 0.0f);
        Check(lpOffsets[luPoint].y, 0.0f);
        Check(lpOffsets[luPoint].z, 0.0f);
        Check(lpOffsets[luPoint].w, 0.0f);
    }

    // A replaced car/teleport seeds the first supplied shape without blending
    // from a previous owner's skin, even at alpha zero.
    lCar.ResetRenderPoseInterpolation();
    lpOffsets[127] = {0.31f, -0.21f, 0.11f, 0.51f};
    lCar.LatchTickRenderPose();
    lCar.ApplyRenderPoseInterpolation(0.0f);
    Check(lpOffsets[127].x, 0.31f);
    Check(lpOffsets[127].y, -0.21f);
    Check(lpOffsets[127].z, 0.11f);
    Check(lpOffsets[127].w, 0.51f);
}

int main()
{
    CheckDeformationAndPanelStayTogether();
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
