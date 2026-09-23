// Exercise the production setup body against the real streamed and output types.
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h"
#include "GameSource/Physics/DeformationManager/SharedIO/BrnVehicleLocatorData.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

static unsigned gChecks = 0, gFailures = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* message, const char*, int)
{ std::fprintf(stderr, "Unexpected assert: %s\n", message); std::abort(); }
void* EndAssert() { return nullptr; }
} }

namespace BrnPhysics { namespace Deformation {
struct LocatorSetupFixture
{
    const StreamedDeformationSpec* mpDeformationSpec;
    VehicleLocatorData mLocatorData;
    void PrepareLocators();
};
#include "boost_locator_setup.inc"
} }

static void Check(bool pass, const char* message)
{
    ++gChecks;
    if (!pass) { ++gFailures; std::fprintf(stderr, "FAIL: %s\n", message); }
}

int main()
{
    using namespace BrnPhysics::Deformation;
    LocatorPointSpec* records = nullptr;
    for (uintptr_t address = 0x10000000; !records && address < 0xF0000000; address += 0x10000)
        records = static_cast<LocatorPointSpec*>(VirtualAlloc(reinterpret_cast<void*>(address),
            65536, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!records) return 2;
    for (int i = 0; i < 40; ++i)
    {
        new (&records[i]) LocatorPointSpec{};
        records[i].mLocatorMatrix.SetIdentity();
        records[i].mLocatorMatrix.xAxis = {0, 1, 0, 0};
        records[i].mLocatorMatrix.yAxis = {-1, 0, 0, 0};
        records[i].mLocatorMatrix.wAxis = {float(i + 1), float(i + 2), float(i + 3), 0};
        records[i].meTagPointType = static_cast<ETagPointType>(i % E_TAGPOINT_COUNT);
    }
    // Nonadjacent exhaust points ensure the entire generic list is published.
    records[1].meTagPointType = E_TAGPOINT_FXBOOSTPOINT1;
    records[4].meTagPointType = E_TAGPOINT_FXBOOSTPOINT2;
    StreamedDeformationSpec spec{};
    spec.mGenericTags = {15, {static_cast<u32>(reinterpret_cast<uintptr_t>(records))}};
    spec.mLightTags = {24, {static_cast<u32>(reinterpret_cast<uintptr_t>(records + 15))}};
    spec.mCameraTags = {1, {static_cast<u32>(reinterpret_cast<uintptr_t>(records + 39))}};
    LocatorSetupFixture model{};
    model.mpDeformationSpec = &spec;
    model.PrepareLocators();
    Check(model.mLocatorData.miNumGenericLocators == 15, "generic count at capacity");
    Check(model.mLocatorData.miNumLightLocators == 24, "light count at capacity");
    Check(model.mLocatorData.miNumCameraLocators == 1, "camera count at capacity");
    for (int i = 0; i < 40; ++i)
    {
        const auto& transform = i < 15 ? model.mLocatorData.maGenericLocators[i]
            : i < 39 ? model.mLocatorData.maLightLocators[i - 15] : model.mLocatorData.maCameraLocators[0];
        const auto type = i < 15 ? model.mLocatorData.maGenericLocatorTypes[i]
            : i < 39 ? model.mLocatorData.maLightLocatorTypes[i - 15] : model.mLocatorData.maCameraLocatorTypes[0];
        Check(std::memcmp(&transform, &records[i].mLocatorMatrix, sizeof(transform)) == 0,
            "all four transform rows copied to the correct group");
        Check(type == records[i].meTagPointType, "authored tag preserved");
    }
    unsigned boostMask = 0;
    for (int i = 0; i < model.mLocatorData.miNumGenericLocators; ++i)
    {
        const int tag = model.mLocatorData.maGenericLocatorTypes[i] - E_TAGPOINT_FXBOOSTPOINT1;
        if (tag >= 0 && tag < 4) boostMask |= 1u << tag;
    }
    Check(boostMask == 3, "both exhaust nozzles discoverable by effects");
    spec.mGenericTags.muNumLocators = 1;
    spec.mLightTags.muNumLocators = spec.mCameraTags.muNumLocators = 0;
    model.PrepareLocators();
    Check(model.mLocatorData.miNumGenericLocators == 1, "reuse replaces generic count");
    Check(model.mLocatorData.miNumLightLocators == 0, "reuse clears light count");
    Check(model.mLocatorData.miNumCameraLocators == 0, "reuse clears camera count");
    spec.mGenericTags = {};
    spec.mLightTags = {};
    spec.mCameraTags = {};
    model.PrepareLocators();
    Check(model.mLocatorData.miNumGenericLocators == 0, "empty null lists reset without dereference");
    VirtualFree(records, 0, MEM_RELEASE);
    std::printf("BoostLocatorSetup: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
