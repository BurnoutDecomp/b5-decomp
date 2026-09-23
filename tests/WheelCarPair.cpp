// Harness for run_wheel_car_pair.py (crash parity G22-D1): links the shipped
// CgsPrimitivePairListBuilder.cpp and appends one CYLINDER-vs-BOX pair through the new
// AddPrimitivePair(Cylinder*, Box*) overload @0x828149F8 -- the record DeformationManager::
// AddRaceCarWheelPair @0x82605BE8 emits for a torn-off wheel overlapping a car. Pre-fix the overload
// does not exist (the producer was a logging GATE), so this harness does not even compile: RED.
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsPrimitivePairListBuilder.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsCylinder.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsBox.h"
#include <cstdio>
#include <cstring>

namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* m, const char*, int) { std::printf("ASSERT: %s\n", m); return 0; }
void* EndAssert() { return nullptr; }
} }

int main()
{
    using namespace CgsSceneManager::CgsCollision;
    int liChecks = 0, liFailures = 0;
    auto Check = [&](bool lbPass, const char* lpcName) {
        ++liChecks;
        if (!lbPass) { ++liFailures; std::printf("FAIL: %s\n", lpcName); }
    };

    alignas(16) static u8 saStream[1024];
    PrimitivePairListBuilder lBuilder;
    std::memset(&lBuilder, 0, sizeof(lBuilder));
    lBuilder.mpaDataStream   = saStream;
    lBuilder.mu16MaxDataSize = sizeof(saStream);

    CgsGeometric::Cylinder lCylinder;
    std::memset(&lCylinder, 0, sizeof(lCylinder));
    reinterpret_cast<f32*>(&lCylinder)[16] = 0.35f;   // +0x40
    reinterpret_cast<f32*>(&lCylinder)[17] = 0.12f;   // +0x44
    CgsGeometric::Box lBox;
    std::memset(&lBox, 0, sizeof(lBox));
    reinterpret_cast<f32*>(&lBox)[16] = 1.0f;         // +0x40 dims.x

    lBuilder.AddPrimitivePair(&lCylinder, &lBox, 0.5f, 3u, 7u);

    const PrimitivePairList::CollisionHeader* lpHeader =
        reinterpret_cast<const PrimitivePairList::CollisionHeader*>(saStream);
    Check(lBuilder.mu16NumTests == 1, "one pair record appended");
    Check(lpHeader->mu8PrimTypeA == PrimitivePairList::E_VOLUME_TYPE_CYLINDER &&
          lpHeader->mu8PrimTypeB == PrimitivePairList::E_VOLUME_TYPE_BOX, "header types = CYLINDER, BOX (5, 4)");
    Check(lBuilder.mu16UsedData == sizeof(PrimitivePairList::CollisionHeader) + 0x50 + 0x50,
          "header + 80-byte cylinder + 80-byte box");
    const f32* lpCyl = reinterpret_cast<const f32*>(saStream + sizeof(PrimitivePairList::CollisionHeader));
    const f32* lpBoxOut = lpCyl + 20;
    Check(lpCyl[16] == 0.35f && lpCyl[17] == 0.12f, "cylinder scalars copied (+0x40 / +0x44)");
    Check(lpBoxOut[16] == 1.0f, "box copied");
    std::printf("WheelCarPair: %d checks, %d failures\n", liChecks, liFailures);
    return liFailures ? 1 : 0;
}
