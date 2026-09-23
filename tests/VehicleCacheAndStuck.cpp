// Harness for run_vehicle_cache_and_stuck.py: the production statements are pasted in through
// extracted.inc; this file only feeds them inputs and checks the console's numbers.
#include "types.hpp"
#include "BrnCommonTypes.h"                                // Vector3 (rw::math::vpu::Vector3)
#include "SharedClasses/World/BrnCollisionTag.h"          // BrnWorld::KU_COLLISION_FLAG_DRIVEABLE
#include "rw/math/vpu/vector3_operation.h"              // rw::math::vpu::MagnitudeSquared
#include <cmath>
#include <cstdio>
#include <cstring>

#include "extracted.inc"

int main()
{
    using namespace BrnPhysics::Vehicle;
    int liChecks = 0, liFailures = 0;
    auto Check = [&](bool lbPass, const char* lpcName) {
        ++liChecks;
        if (!lbPass) { ++liFailures; std::printf("FAIL: %s\n", lpcName); }
    };
    auto Near = [](f32 a, f32 b) { return std::fabs(a - b) < 1e-4f; };

    // G43-D1: |he + (1,1,1)| over xyz, 0 through the lensq==0 guard.
    Car lCavalry{ { 0.95f, 0.68f, 2.40f, 0.0f } };
    Check(Near(VehicleRadius(lCavalry), 4.26438f), "race car radius = |halfExtent + pad| (0x82615D70 vaddfp)");
    Check(Near(TrafficRadius(&lCavalry), 4.26438f), "traffic radius = |halfExtent + pad| (0x825EE7E8 vaddfp)");
    Car lZero{ { 0.0f, 0.0f, 0.0f, 0.0f } };
    Check(Near(VehicleRadius(lZero), 1.7320508f), "zero extent still gets the 1.0 padding");
    Car lNeg{ { -1.0f, -1.0f, -1.0f, 0.0f } };
    Check(VehicleRadius(lNeg) == 0.0f && TrafficRadius(&lNeg) == 0.0f, "lensq == 0 selects radius 0 (vsel 0x82615DCC)");

    // G40-D1: the DRIVEABLE bit lives in the HIGH (material) halfword.
    Check(!Occluded({ true, 0x20000000u }), "material DRIVEABLE (high bit 13) -> road, not occluded");
    Check(Occluded({ true, 0x00002000u }), "group bit 13 set but material not driveable -> occluded (wall)");
    Check(!Occluded({ false, 0x00000000u }), "no hit -> not occluded");

    // G40-D3: the image word.
    u32 luBits = 0;
    std::memcpy(&luBits, &KF_CUTOFF, sizeof(luBits));
    Check(luBits == 0x3CECBFB2u, "KF_STUCK_LINETEST_ANGULARCUTOFF_SQ == flt_82093CE4 (0x3CECBFB2)");

    std::printf("VehicleCacheAndStuck: %d checks, %d failures\n", liChecks, liFailures);
    return liFailures ? 1 : 0;
}
