// ARTIST Clear @0x8227D550 resets model/rival IDs and deformation handles.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#undef private
#include <cstdio>
#include <cstdlib>

namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int)
{ std::fprintf(stderr, "ASSERT: %s\n", lpcMessage); std::abort(); }
void* EndAssert() { return nullptr; }
} }
namespace BrnWorld { namespace RaceCarEntityModuleIO {
#include "active_output_clear_methods.inc"
} }

static u32 suChecks = 0, suFailures = 0;
static void Check(bool lbCondition, const char* lpcMessage)
{
    ++suChecks;
    if (!lbCondition)
    {
        ++suFailures;
        if (suFailures < 5) std::fprintf(stderr, "FAIL: %s\n", lpcMessage);
    }
}

int main()
{
    using namespace BrnWorld;
    using namespace RaceCarEntityModuleIO;
    static RCEntityActiveRaceCarOutputInterface lOutput;
    static u32 luResource;
    CgsID laPreviousModels[8];
    for (u32 luSlot = 0; luSlot < 8; ++luSlot)
    {
        lOutput.maRivalIds[luSlot] = 0xDECAFBAD00000000ull + luSlot;
        laPreviousModels[luSlot] = lOutput.maCarModelIds[luSlot] = 0xC0FFEE1000000000ull + luSlot;
        lOutput.maDeformationModelResourceHandles[luSlot].mpResourceMemory = &luResource;
        lOutput.maDeformationModelResourceHandles[luSlot].mpSourceEntry =
            reinterpret_cast<CgsResource::Entry*>(&luResource); // identity only, never dereferenced
        lOutput.maCurrentInAirRotations[luSlot] = {1, 2, 3, 4};
    }
    lOutput.mbCanDriveAwayFromCrash = true; // ARTIST Clear leaves this published value alone
    lOutput.Clear();
    u32 luModelTransitions = 0;
    for (u32 luSlot = 0; luSlot < 8; ++luSlot)
    {
        const auto leSlot = static_cast<EActiveRaceCarIndex>(luSlot);
        Check(lOutput.GetCarModelId(leSlot) == 0, "cleared slot drops old model ID");
        Check(lOutput.GetRivalId(leSlot) == 0, "cleared slot drops old rival ID");
        Check(lOutput.maDeformationModelResourceHandles[luSlot].mpResourceMemory == nullptr,
              "cleared slot drops resource memory");
        Check(lOutput.maDeformationModelResourceHandles[luSlot].mpSourceEntry == nullptr,
              "cleared slot drops resource entry");
        Check(lOutput.maCurrentInAirRotations[luSlot].z == 3, "clear retains untouched pose data");
        luModelTransitions += laPreviousModels[luSlot] != lOutput.GetCarModelId(leSlot);
    }
    Check(luModelTransitions == 8, "removed slots trigger original effects model-change Reset");
    Check(lOutput.mbCanDriveAwayFromCrash, "clear retains original drive-away value");
    lOutput.Clear();
    Check(lOutput.GetCarModelId(E_ACTIVE_RACE_CAR_INDEX_0) == 0,
          "repeated empty-frame publication remains cleared");
    std::printf("Active race car output Clear: %s (%u checks, %u failures, %u model transitions)\n",
                suFailures ? "FAIL" : "PASS", suChecks, suFailures, luModelTransitions);
    return suFailures ? 1 : 0;
}
