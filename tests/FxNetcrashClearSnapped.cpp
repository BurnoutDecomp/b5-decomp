// FX-NETCRASH (crash parity 2026-09-24), G45-D1 / G34-D2: replay the production
// PhysicalTrafficManager::ClearSnappedNetworkTrafficContacts (ARTIST 0x825F37F0, 220 insns) with the
// production GetPhysicsEntityId (0x825B4980) over a real BitArray<20> of used traffic slots and real
// VehicleDriver records, against a recording stand-in for the deformation manager:
//   * a USED slot whose driver SNAPPED this frame (VehicleDriver +0xD5) gets its deformable object's
//     stored contacts cleared, found through FindModelIndexByEntityID(GetPhysicsEntityId(slot))
//     (physics id == (slot << 10) | 0x02000000), in bit order (0x825F3910..0x825F3984);
//   * a used slot that did not snap, or a snapped slot that is not used, is not touched;
//   * a snapped slot with no model trips "liModelIndex != -1" (BrnDeformationManager.h:878);
//   * the function never clears mbSnappedThisFrame (UpdateTrafficPhysicsPostSimulation does).
// run_fxnetcrash_clear_snapped.py extracts both bodies verbatim (--pre-fix <rev> replays a missing
// body EMPTY) and checks that VehicleManager::ClearSnappedNetworkCarContacts tail-calls it on the
// traffic manager after its race-car walk (0x8261AC1C..0x8261AC28).
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnVehicleDriver.h"
#undef protected
#undef private
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static unsigned assertions = 0;
static const char* lastAssertion = "";
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* message, const char*, int) { ++assertions; lastAssertion = message; std::fprintf(stderr, "ASSERT: %s\n", message); return 0; }
void* EndAssert() { return nullptr; }
} namespace Log {
DebugPrint* gpDebugPrint = nullptr;   // the [netcrash] witness stays silent
StrStreamBase& DebugPrint::operator<<(const char*) { return *this; }
void WriteToLog(const char*) {} }
namespace Message { u64 gxMessageFilterFlags = 0; } }

namespace BrnPhysics { namespace Vehicle {

// Records every lookup and every ClearStoredContacts, by model index.
struct FakeDeformableObject { s32 miCleared = 0; void ClearStoredContacts() { ++miCleared; } };
struct FakeDeformationManager
{
    static const s32 KI_MODELS = 8;
    FakeDeformableObject maObjects[KI_MODELS];
    u32 mauModelEntity[KI_MODELS];
    u32 mauLookups[32];
    s32 miLookups = 0;
    FakeDeformationManager() { for (s32 i = 0; i < KI_MODELS; ++i) mauModelEntity[i] = 0xFFFFFFFFu; }
    s32 FindModelIndexByEntityID(EntityId lEntityId) const
    {
        FakeDeformationManager* self = const_cast<FakeDeformationManager*>(this);
        if (self->miLookups < 32) self->mauLookups[self->miLookups++] = lEntityId.muValue;
        for (s32 i = 0; i < KI_MODELS; ++i)
            if (mauModelEntity[i] == lEntityId.muValue) return i;
        return -1;
    }
    FakeDeformableObject* GetDeformableObject(s32 liIndex) { return &maObjects[liIndex]; }
};

struct TrafficFixture
{
    typedef PhysicalTrafficManager::TotalPhysicalTrafficBitArray TotalPhysicalTrafficBitArray;
    TotalPhysicalTrafficBitArray mUsedTrafficVehicles;
    VehicleDriver*               mpaTrafficDrivers = nullptr;
    EntityId GetPhysicsEntityId(s32 liTrafficIndex) const;
    void ClearSnappedNetworkTrafficContacts(FakeDeformationManager* lpDeformationManager);
};

} }
#include "fxnetcrash_clear_snapped_methods.inc"

using namespace BrnPhysics::Vehicle;

int main()
{
    unsigned checks = 0, failures = 0;
    auto Check = [&](bool lbPass, const char* lpcName)
    {
        ++checks;
        if (!lbPass) { ++failures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
    };
    auto Physics = [](u32 luSlot) { return (luSlot << 10) | 0x02000000u; };

    static VehicleDriver saDrivers[KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC];
    std::memset(saDrivers, 0, sizeof(saDrivers));
    TrafficFixture m;
    m.mpaTrafficDrivers = saDrivers;
    m.mUsedTrafficVehicles.UnSetAll();

    // Used slots 2, 5, 7, 12; snapped drivers 2, 7, 9, 12. Models: slot 2 -> 3, slot 7 -> 0,
    // slot 12 -> 5, slot 9 -> 6 (a model that must NOT be cleared: slot 9 is not in use).
    const u32 kaUsed[] = { 2, 5, 7, 12 };
    for (u32 luSlot : kaUsed) m.mUsedTrafficVehicles.SetBit(luSlot);
    const u32 kaSnapped[] = { 2, 7, 9, 12 };
    for (u32 luSlot : kaSnapped) saDrivers[luSlot].mbSnappedThisFrame = true;
    FakeDeformationManager lManager;
    lManager.mauModelEntity[3] = Physics(2);
    lManager.mauModelEntity[0] = Physics(7);
    lManager.mauModelEntity[5] = Physics(12);
    lManager.mauModelEntity[6] = Physics(9);

    m.ClearSnappedNetworkTrafficContacts(&lManager);

    Check(lManager.maObjects[3].miCleared == 1, "snapped used slot 2 clears model 3 once");
    Check(lManager.maObjects[0].miCleared == 1, "snapped used slot 7 clears model 0 once");
    Check(lManager.maObjects[5].miCleared == 1, "snapped used slot 12 clears model 5 once");
    Check(lManager.maObjects[6].miCleared == 0, "snapped slot 9 is not in use: model 6 untouched");
    Check(lManager.maObjects[1].miCleared + lManager.maObjects[2].miCleared + lManager.maObjects[4].miCleared
              + lManager.maObjects[7].miCleared == 0, "no other model is cleared");
    Check(lManager.miLookups == 3 && lManager.mauLookups[0] == Physics(2) && lManager.mauLookups[1] == Physics(7)
              && lManager.mauLookups[2] == Physics(12),
          "the lookups are GetPhysicsEntityId of the snapped used slots, in bit order (2, 7, 12)");
    Check(saDrivers[2].mbSnappedThisFrame && saDrivers[7].mbSnappedThisFrame && saDrivers[12].mbSnappedThisFrame,
          "mbSnappedThisFrame is left set (UpdateTrafficPhysicsPostSimulation clears it)");
    Check(assertions == 0, "no tripwire on a consistent frame");

    // A snapped used slot with no deformation model: the console's :878 tripwire.
    m.mUsedTrafficVehicles.SetBit(15);
    saDrivers[15].mbSnappedThisFrame = true;
    m.ClearSnappedNetworkTrafficContacts(&lManager);
    Check(assertions == 1 && std::strcmp(lastAssertion, "liModelIndex != -1") == 0,
          "a snapped slot with no model trips \"liModelIndex != -1\" (BrnDeformationManager.h:878)");
    Check(lManager.maObjects[3].miCleared == 2 && lManager.maObjects[0].miCleared == 2 && lManager.maObjects[5].miCleared == 2,
          "a second frame clears the still-snapped slots again");

    std::printf("FxNetcrashClearSnapped: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
