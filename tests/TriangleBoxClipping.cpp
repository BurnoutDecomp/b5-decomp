// Whole production CgsTriangleBox.cpp is linked; no clipping body is copied here.
#include "GameShared/GameClasses/Geometric/Intersection/CgsTriangleBox.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Test boundary: an unexpected engine assertion fails the process immediately.
namespace CgsDev { namespace Assert
{
    int BeginAssert() { std::abort(); }
    int FireAssert(const char*, const char*, int) { std::abort(); }
    void* EndAssert() { std::abort(); }
} }

namespace
{
    using rw::math::vpu::Vector3;
    using rw::math::vpu::MaskScalar;
    int checks = 0;
    int failures = 0;

    void Check(bool passed, const char* name)
    {
        ++checks;
        if (!passed) { ++failures; std::printf("FAIL %s\n", name); }
    }

    bool Valid(const MaskScalar& mask)
    {
        u32 bits[4];
        std::memcpy(bits, &mask, sizeof(bits));
        return bits[0] == 0xFFFFFFFFu && bits[1] == bits[0]
            && bits[2] == bits[0] && bits[3] == bits[0];
    }

    float FloatBits(u32 bits)
    {
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
}

int main()
{
    const CgsGeometric::AxisAlignedBox box{{-1,-1,-1,0},{1,1,1,0}};
    Vector3 points[18]{};
    MaskScalar masks[18]{};
    CgsGeometric::ClipTriWithAABB(points, masks, box,
        Vector3{-4,-4,0.25f,0}, Vector3{4,-4,0.25f,0}, Vector3{0,4,0.25f,0});
    int count = 0;
    for (int i = 0; i < 12; ++i) count += Valid(masks[i]);
    Check(count == 4, "triangle plane crosses all four vertical box edges");
    bool crossings = true;
    const int crossingIndexes[] = {4,5,8,9};
    for (int i : crossingIndexes)
        crossings = crossings && Valid(masks[i]) && points[i].z == 0.25f
            && std::fabs(points[i].x) == 1 && std::fabs(points[i].y) == 1;
    Check(crossings, "positive box-edge crossing parameters publish the plane points");

    CgsGeometric::ClipTriEdgeAgainstBox(points, masks, box,
        Vector3{0,2,0,0}, Vector3{2,0,0,4});
    Check(Valid(masks[0]) && Valid(masks[1]) && points[0].x == 1
          && points[0].y == 1 && points[1].x == 1 && points[1].y == 1,
          "corner contact retains two equal points from different faces");

    CgsGeometric::ClipTriEdgeAgainstBox(points, masks, box,
        Vector3{-2,-0.5f,0,0}, Vector3{2,3.5f,0,4});
    Check(Valid(masks[0]) && Valid(masks[1]) && points[0].x == -1
          && points[0].y == 0.5f && points[1].x == -0.5f && points[1].y == 1,
          "min-X candidate precedes max-Y candidate");

    // Exact input words produce a rounded own-face x=1+1ulp. ARTIST tests
    // only the other two slab lanes, so the candidate remains valid.
    CgsGeometric::ClipTriEdgeAgainstBox(points, masks, box,
        Vector3{FloatBits(0xC10AB85Bu),0,0,0},
        Vector3{FloatBits(0x3F843541u),0,0,0});
    Check(Valid(masks[0]) && points[0].x > 1,
          "own-face rounding is not subjected to an extra slab test");

    CgsGeometric::ClipTriEdgeAgainstBox(points, masks, box,
        Vector3{-0.5f,0,0,0}, Vector3{0.5f,0,0,0});
    Check(!Valid(masks[0]) && !Valid(masks[1]) && points[0].x == 0
          && points[1].x == 0, "wholly contained segment emits no boundary contacts");

    CgsGeometric::ClipTriEdgeAgainstBox(points, masks, box,
        Vector3{2,-2,0,0}, Vector3{2,2,0,0});
    Check(!Valid(masks[0]) && !Valid(masks[1]),
          "parallel segment outside another slab emits no contacts");

    CgsGeometric::ClipTriEdgeAgainstBox(points, masks, box,
        Vector3{-2,0,0,0}, Vector3{2,0,0,4});
    Check(Valid(masks[0]) && Valid(masks[1]) && points[0].x == 1
          && points[1].x == -1 && points[0].w == 3 && points[1].w == 1,
          "axial crossing keeps original maximum-first order and W lane");

    std::printf("TriangleBoxClipping: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}
