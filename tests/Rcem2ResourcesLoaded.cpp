// FX-RCEM2 (crash-parity 2026-09-23, G61-D6): ActiveRaceCar::OnResourcesLoaded (ARTIST 0x822EB168),
// extracted VERBATIM from BrnActiveRaceCar.cpp by run_rcem2_resources_loaded.py and replayed on a
// fixture car. The console calls ResetVerletOffsets (`mr r3, r28 ; bl` @0x822EB404) after the state
// and handle stores and right BEFORE DetachedPartRenderEvent<20>::Construct (@0x822EB40C).
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstdio>
#include <string>
#include <vector>

static unsigned guAssertions = 0;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { ++guAssertions; std::fprintf(stderr, "ASSERT: %s\n", lpcMessage); return 0; }
void* EndAssert() { return nullptr; }
}
}

namespace Fixture {
namespace CgsResource { struct ResourceHandle { int miTag = 0; }; }
struct Vector3 { f32 x, y, z, w; };
static std::vector<std::string> gaCalls;

struct DetachedPartQueue { void Construct() { gaCalls.push_back("queue"); } };
struct RenderParams {
    DetachedPartQueue mQueue;
    f32 mafVerletOffsets[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
    DetachedPartQueue& GetDetachedPartQueue() { return mQueue; }
};

struct ActiveRaceCar {
    enum EState : u8 { E_STATE_INACTIVE = 0, E_STATE_ATTACHED = 1, E_STATE_WAITING = 2, E_STATE_ACTIVE = 3 };
    u8 muState = E_STATE_ATTACHED;
    CgsResource::ResourceHandle mDeformationModelHandle, mGraphicsModelHandle;
    RenderParams mRenderParams;
    u8 muStateAtVerletReset = 0xFF;
    bool IsAttached() const { return muState != E_STATE_INACTIVE; }
    bool IsActive() const { return muState == E_STATE_ACTIVE; }
    void ResetVerletOffsets() {
        CGS_ASSERT(muState != E_STATE_INACTIVE, "!IsInactive()");
        gaCalls.push_back("verlet"); muStateAtVerletReset = muState;
        for (f32& lf : mRenderParams.mafVerletOffsets) lf = 0.0f;
    }
    void OnResourcesLoaded(const CgsResource::ResourceHandle& lrDeformationModelHandle,
                           const CgsResource::ResourceHandle& lrGraphicsModelHandle,
                           const Vector3& lrInitialVelocity, u64 luCarAssetAttribKey);
};
#include "rcem2_resources_loaded.inc"
}

int main() {
    using namespace Fixture;
    int liChecks = 0, liFailures = 0;
    auto Check = [&](bool lbPass, const char* lpcName) { ++liChecks; if (!lbPass) { ++liFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); } };
    ActiveRaceCar lCar; CgsResource::ResourceHandle lDef, lGfx; lDef.miTag = 11; lGfx.miTag = 22;
    const Vector3 lVelocity = { 0.0f, 0.0f, 0.0f, 0.0f };
    lCar.OnResourcesLoaded(lDef, lGfx, lVelocity, 0x1234ull);
    Check(lCar.muState == ActiveRaceCar::E_STATE_WAITING, "state -> WAITING (+0x740 = 2)");
    Check(lCar.mDeformationModelHandle.miTag == 11 && lCar.mGraphicsModelHandle.miTag == 22, "both handles stored");
    Check(lCar.mRenderParams.mafVerletOffsets[0] == 0.0f && lCar.mRenderParams.mafVerletOffsets[3] == 0.0f,
          "G61-D6 0x822EB404: ResetVerletOffsets zeroes the offsets");
    Check(gaCalls.size() == 2 && gaCalls[0] == "verlet" && gaCalls[1] == "queue",
          "G61-D6: ResetVerletOffsets precedes the detached-part queue Construct (0x822EB404 < 0x822EB40C)");
    Check(lCar.muStateAtVerletReset == ActiveRaceCar::E_STATE_WAITING, "G61-D6: called after the WAITING store (its !IsInactive() holds)");
    Check(guAssertions == 0, "no assertions");
    std::printf("Rcem2ResourcesLoaded: %d checks, %d failures\n", liChecks, liFailures);
    return liFailures ? 1 : 0;
}
