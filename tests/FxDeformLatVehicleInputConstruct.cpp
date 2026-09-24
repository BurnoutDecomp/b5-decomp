// Harness for run_fxdeformlat_vehicle_input_construct.py (crash parity G27-D1; prepared by
// FX-DEFORM-LAT, landed by FX-XLANE).
//
// VehicleInputInterface::Construct @0x822E66A0, extracted from the real source, run on 0xCD-poisoned
// storage of the REAL VehicleInputInterface (PC IO buffers are default-initialised, not zero-filled:
// CgsIOBufferStack::CreateIOBuffer does `new (lpMem) T;` and memsets only under BRN_IOBUF_ZERO=1).
//   0x822E6740 lis r11,1 ; 0x822E6744 li r30,0 ; 0x822E6748 ori r11,r11,0xF410 ;
//   0x822E6754 stwx r30,r31,r11 -> this+0x1F410 == mTriangleCacheInterface.mpTriangleCacheManager = 0
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"
#include <cstdio>
#include <cstring>
#include <new>

static int giAsserts = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcExpr, const char*, int) { ++giAsserts; std::printf("  assert: %s\n", lpcExpr); return 0; }
void* EndAssert() { return nullptr; }
} }

#include "methods.inc"

using namespace BrnPhysics::Vehicle;
alignas(16) static unsigned char gStorage[sizeof(VehicleInputInterface)];

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL: %s\n", lpcName); }
}

int main()
{
    std::memset(gStorage, 0xCD, sizeof(gStorage));
    VehicleInputInterface* lpInterface = new (gStorage) VehicleInputInterface;   // default-init, as CreateIOBuffer
    lpInterface->Construct();

    Check(lpInterface->GetTriangleCacheInterface()->mpTriangleCacheManager == nullptr,
          "mTriangleCacheInterface.mpTriangleCacheManager = NULL (0x822E6754 stwx 0 -> this+0x1F410)");
    Check(lpInterface->GetLineTestResults()->GetLength() == 0 && lpInterface->GetCreateRaceCarEventQueue()->GetLength() == 0,
          "queues constructed empty (control)");
    bool lbClear = true;
    for (u32 lu = 0; lu < 8u; ++lu) lbClear = lbClear && !lpInterface->GetRaceCarsAddedForCollision()->IsBitSet(lu);
    Check(lbClear, "added-for-collision set cleared (control)");
    Check(giAsserts == 0, "no tripwire (control)");
    std::printf("FxDeformLatVehicleInputConstruct: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
