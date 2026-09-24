// FX-RCEM2 (crash-parity 2026-09-23, G61-D6): ActiveRaceCar::OnResourcesLoaded (ARTIST 0x822EB168),
// extracted VERBATIM from BrnActiveRaceCar.cpp by run_rcem2_resources_loaded.py and replayed on a
// fixture car. The console calls ResetVerletOffsets (`mr r3, r28 ; bl` @0x822EB404) after the state
// and handle stores and right BEFORE DetachedPartRenderEvent<20>::Construct (@0x822EB40C).
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static unsigned guAssertions = 0;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { ++guAssertions; std::fprintf(stderr, "ASSERT: %s\n", lpcMessage); return 0; }
void* EndAssert() { return nullptr; }
}
// The [res-loaded] witness's sink (PC-only diag, off unless BRN_RESLOADED_DIAG is set).
namespace Log {
struct DebugPrint { template <class T> DebugPrint& operator<<(const T&) { return *this; } };
DebugPrint* gpDebugPrint = nullptr;
}
}

namespace Fixture {
namespace CgsResource { struct ResourceHandle { int miTag = 0; }; }
struct Vector3 { f32 x, y, z, w; };
static std::vector<std::string> gaCalls;

struct DetachedPartQueue { void Construct() { gaCalls.push_back("queue"); } };

// FX-RCEM4 (CHAIN-RECOLOUR / G61-D6 colour leg, 2026-09-24): OnResourcesLoaded now also reads the car's
// authored default colour (0x822EB474..0x822EB4F0). Stand-ins so the extracted body builds; the leg
// itself is tested by run_fxrcem4_recolour.py. The queue Construct must still come first.
static bool gbQueueBuiltBeforeColour = false;
namespace Attrib {
struct Collection {};
namespace Gen {
struct burnoutcarasset {
    struct RefSpec { const Collection* GetCollection() { return nullptr; } } mRef;
    burnoutcarasset(u64, void*) { gbQueueBuiltBeforeColour = !gaCalls.empty() && gaCalls.back() == "queue"; }
    RefSpec* GetGraphicsAssetRefSpec() const { return const_cast<RefSpec*>(&mRef); }
};
struct burnoutcargraphicsasset {
    burnoutcargraphicsasset(Collection*, void*) {}
    const s32& PlayerColourIndex() const { static const s32 kiColour = 13; return kiColour; }
    const s32& PlayerColourPaletteIndex() const { static const s32 kiPalette = 2; return kiPalette; }
};
}
}
// FX-RCEM4 (reviewer B on 87d1ad23 / G62, 2026-09-24): OnResourcesLoaded now also copies the spec's
// +1552 matrix (0x822EB20C) and sets the four wheel scales (0x822EB410). Stand-ins so the extracted
// body builds; those legs are tested by run_fxrcem4_on_resources_loaded.py.
struct Matrix44Affine { Vector3 xAxis, yAxis, zAxis, wAxis; };
namespace rw { namespace math { namespace vpu { inline bool IsValid(const Matrix44Affine&) { return true; } } } }
namespace BrnPhysics { namespace Deformation {
struct WheelSpec { Vector3 mPosition, mScale; s32 liTagPointIndex; };
struct StreamedDeformationSpec {
    Matrix44Affine mCarModelSpaceToHandlingBodySpaceTransform;
    WheelSpec maWheelSpecs[4];
    const WheelSpec* GetWheelSpec(s32 liWheel) const { return &maWheelSpecs[liWheel]; }
};
} }

struct RenderParams {
    DetachedPartQueue mQueue;
    f32 mafVerletOffsets[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
    Matrix44Affine maWheelScale[6] = {};
    DetachedPartQueue& GetDetachedPartQueue() { return mQueue; }
    void SetWheelScale(u32, const Vector3&) {}
    Matrix44Affine& GetWheelScaleMatrix(u32 luWheel) { return maWheelScale[luWheel]; }
};
static const BrnPhysics::Deformation::StreamedDeformationSpec gResidentSpec = {};
const BrnPhysics::Deformation::StreamedDeformationSpec* ResolveDeformationSpec(const CgsResource::ResourceHandle&) { return &gResidentSpec; }

struct ActiveRaceCar {
    enum EState : u8 { E_STATE_INACTIVE = 0, E_STATE_ATTACHED = 1, E_STATE_WAITING = 2, E_STATE_ACTIVE = 3 };
    u8 muState = E_STATE_ATTACHED;
    s32 meActiveRaceCarIndex = 0;
    Matrix44Affine mCentreOfMassTransform = {};
    CgsResource::ResourceHandle mDeformationModelHandle, mGraphicsModelHandle;
    RenderParams mRenderParams;
    u8 muStateAtVerletReset = 0xFF;
    s32 miDefaultColourIndex = -1, miDefaultColourPalette = -1;   // +0x1C80 / +0x1C84 (FX-RCEM4)
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
    Check(gbQueueBuiltBeforeColour && lCar.miDefaultColourIndex == 13 && lCar.miDefaultColourPalette == 2,
          "FX-RCEM4: the default-colour leg (0x822EB474) follows the queue Construct and stores both words");
    Check(guAssertions == 0, "no assertions");
    std::printf("Rcem2ResourcesLoaded: %d checks, %d failures\n", liChecks, liFailures);
    return liFailures ? 1 : 0;
}
