// Link the complete production Frustum.cpp and exercise the member API.
#include "vendor/renderware/collision/Frustum.hpp"
#include <cstdio>
#include <limits>

int main()
{
    rw::collision::Frustum frustum;
    const auto SetPlanes = [&](float offset)
    {
        for (u32 i = 0; i < 6; ++i)
            frustum.SetPlane(i, rw::collision::Plane(
                rw::math::vpu::Vector4{1,0,0,offset}));
    };
    rw::math::vpu::Vector3 corners[8];
    const auto SetCorners = [&](float x)
    {
        for (auto& corner : corners) corner = {x,0,0,0};
    };
    int checks = 0, failures = 0;
    const auto Check = [&](bool passed, const char* name)
    {
        ++checks;
        if (!passed) { ++failures; std::printf("FAIL %s\n", name); }
    };

    SetPlanes(2);
    SetCorners(3);
    Check(frustum.IsBoxInFrustum(corners) == 1, "finite interior corners overlap");
    SetCorners(1);
    Check(frustum.IsBoxInFrustum(corners) == 0, "fully separated finite corners cull");
    SetCorners(2);
    Check(frustum.IsBoxInFrustum(corners) == 0, "equal plane boundary remains culled");
    corners[7].x = 3;
    Check(frustum.IsBoxInFrustum(corners) == 1, "one interior corner prevents separation");
    SetCorners(std::numeric_limits<float>::quiet_NaN());
    Check(frustum.IsBoxInFrustum(corners) == 0, "unordered dot stays on original culled branch");
    SetPlanes(std::numeric_limits<float>::quiet_NaN());
    SetCorners(3);
    Check(frustum.IsBoxInFrustum(corners) == 0, "unordered plane offset stays on culled branch");
    std::printf("CollisionFrustumPlanes: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}
