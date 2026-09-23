// Harness for run_part_prepare_frozen.py (crash parity G29-D1): the shipped flag-store block of
// PhysicalBodyPart::Prepare @0x82626700 is pasted into Part::Prep through extracted.inc. The
// console clears mbFrozen on every re-Prepare (0x82626750 stb r29,0x1E6); a pool slot freed while
// its part lay frozen must come back unfrozen.
#include "types.hpp"
#include <cstdio>

struct Part
{
    u64 mRigidBodyId = 0;
    u32 mGlobalVehicleId = 0;
    s8 mi8ActiveJointsTagPointIndex = 0;
    const void* mpDeformableObject = nullptr;
    const void* mpIKPart = nullptr;
    bool mbAddedToScene = true;
    bool mbFrozen = true;            // left frozen by the previous occupant of the slot
    bool mbJoinedToVehicle = true;
    void Prep(u64 lPartId, u32 lGlobalVehicleId, const void* lpDeformableObject, const void* lpIKPart)
    {
#include "extracted.inc"
    }
};

int main()
{
    int liChecks = 0, liFailures = 0;
    auto Check = [&](bool lbPass, const char* lpcName) {
        ++liChecks;
        if (!lbPass) { ++liFailures; std::printf("FAIL: %s\n", lpcName); }
    };
    Part p;
    p.Prep(0x1234u, 0x01000400u, &p, &p);
    Check(!p.mbFrozen, "a re-prepared slot is no longer frozen (stb 0,0x1E6)");
    Check(!p.mbAddedToScene && !p.mbJoinedToVehicle, "added/joined cleared");
    Check(p.mi8ActiveJointsTagPointIndex == -1, "active joint tag point reset");
    std::printf("PartPrepareFrozen: %d checks, %d failures\n", liChecks, liFailures);
    return liFailures ? 1 : 0;
}
