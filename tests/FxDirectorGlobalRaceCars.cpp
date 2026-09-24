// FX-DIRECTOR (crash parity 2026-09-24): the director input's global race-car table.
//
// BrnGameModule::BridgeWorldToDirector step 8 (0x823E3FD8..0x823E3FEC) is
//     XMemCpy(input + 0x10, world->GetRaceCarGlobalOutputInterface() (0x823B5AC8), 0x970)
// -- the inlined DWARF InputBuffer::SetGlobalRaceCarInterface (:231) -- and MainDirector::
// ProcessInputQueue's cases 113 / 223 read it back through `addi r3, <input>, 0x10` (the inlined
// GetGlobalRaceCarInterface, :230) into RCEntityGlobalRaceCarOutputInterface::GetActiveRaceCarIndex.
// This fixture runs the PRODUCTION step-8 statement (extracted from src/GameSource/Game/
// GameBridgeWorldToX.cpp by run_fxdirector_global_race_cars.py) against the REVISION's real
// InputBuffer (shadowed header) and the REAL RCEntityGlobalRaceCarOutputInterface (its own TU linked:
// Clear / SetRaceCarData / operator= / the getters), filling the world's table the way
// RaceCarEntityModule does. A revision whose input has no typed table cannot build this test.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/BurnoutConstants.h"
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"      // the REAL InputBuffer
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstddef>
#include <cstdio>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcText, const char*, int) { ++gAsserts; std::fprintf(stderr, "assert: %s\n", lpcText); return 0; }
    void* EndAssert() { return nullptr; }
}
}

using BrnDirector::DirectorIO::InputBuffer;
using BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface;

// The world output: only the accessor step 8 reads (UpdateOutputBuffer::GetRaceCarGlobalOutputInterface).
struct WorldOutputStandIn
{
    alignas(16) unsigned char maTableStorage[sizeof(RCEntityGlobalRaceCarOutputInterface)];
    RCEntityGlobalRaceCarOutputInterface& Table() { return *reinterpret_cast<RCEntityGlobalRaceCarOutputInterface*>(maTableStorage); }
    const RCEntityGlobalRaceCarOutputInterface* GetRaceCarGlobalOutputInterface() const
    {
        return reinterpret_cast<const RCEntityGlobalRaceCarOutputInterface*>(maTableStorage);
    }
};

// Step 8, under its production parameter names.
static void Step8(InputBuffer* lpDirectorInput, const WorldOutputStandIn* lpWorldOutput)
{
    (void)lpDirectorInput;
    (void)lpWorldOutput;
#include "fxdirector_step8.inc"
}

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

alignas(16) static unsigned char gaInputStorage[sizeof(InputBuffer)];
static WorldOutputStandIn gWorld;
static InputBuffer& Input() { return *reinterpret_cast<InputBuffer*>(gaInputStorage); }

// One car into the world's table, the way RaceCarEntityModule publishes it (SetRaceCarData).
static void Publish(s32 liGlobal, s32 liActive, f32 lfSpeed, bool lbIsPlayer)
{
    Vector3 lPosition;
    lPosition.x = 100.0f + liGlobal; lPosition.y = 2.0f; lPosition.z = -50.0f; lPosition.w = 0.0f;
    Vector3 lAt;
    lAt.x = 0.0f; lAt.y = 0.0f; lAt.z = 1.0f; lAt.w = 0.0f;
    BrnWorld::WorldRegion lRegion;
    std::memset(&lRegion, 0, sizeof(lRegion));
    gWorld.Table().SetRaceCarData(lPosition, lAt, lRegion, CgsID(0x1000u + liGlobal), CgsID(0x2000u + liGlobal),
                                  lfSpeed, static_cast<u16>(liGlobal), static_cast<EGlobalRaceCarIndex>(liGlobal),
                                  static_cast<s8>(-1), static_cast<EActiveRaceCarIndex>(liActive),
                                  lbIsPlayer, !lbIsPlayer, false, true, false, true);
}

int main()
{
    // 1. Where the table lives in the input.
    Check(offsetof(InputBuffer, mGlobalRaceCarInterface) == 0x10,
          "the table is the input's first member at +0x10 (XMemCpy dst `addi r3, r26, 0x10` @0x823E3FE8)");
    Check(sizeof(RCEntityGlobalRaceCarOutputInterface) == 0x970,
          "the table is the console's 0x970 bytes (XMemCpy `li r5, 0x970` @0x823E3FE4)");
    Check(offsetof(InputBuffer, mUsedRaceCars) == 0x980, "the table ends exactly at mUsedRaceCars (+0x980)");

    // 2. The world's table for this frame: the player at global 3 -> active 0, rivals at 7 -> 1 and
    //    20 -> 4, and one global car with no active slot (12 -> -1).
    std::memset(gaInputStorage, 0xCD, sizeof(gaInputStorage));
    std::memset(gWorld.maTableStorage, 0, sizeof(gWorld.maTableStorage));
    gWorld.Table().Clear();
    Publish(3, 0, 31.5f, true);
    Publish(7, 1, 28.0f, false);
    Publish(20, 4, 12.25f, false);
    Publish(12, -1, 5.0f, false);
    gWorld.Table().SetPlayerGlobalRaceCarIndex(static_cast<EGlobalRaceCarIndex>(3));
    unsigned char lau8UsedBefore[16];
    std::memcpy(lau8UsedBefore, gaInputStorage + 0x980, sizeof(lau8UsedBefore));

    Step8(&Input(), &gWorld);

    const RCEntityGlobalRaceCarOutputInterface* lpTable = Input().GetGlobalRaceCarInterface();
    Check(reinterpret_cast<const unsigned char*>(lpTable) == gaInputStorage + 0x10,
          "GetGlobalRaceCarInterface is the address the console computes: input + 0x10");
    {
        bool lbAll = true;
        for (s32 liGlobal = 0; liGlobal < E_GLOBAL_RACE_CAR_INDEX_COUNT; ++liGlobal)
        {
            const EGlobalRaceCarIndex leGlobal = static_cast<EGlobalRaceCarIndex>(liGlobal);
            lbAll = lbAll && lpTable->GetActiveRaceCarIndex(leGlobal) == gWorld.GetRaceCarGlobalOutputInterface()->GetActiveRaceCarIndex(leGlobal);
        }
        Check(lbAll, "step 8 publishes the world's global -> active map for all 35 slots");
    }
    Check(lpTable->GetActiveRaceCarIndex(static_cast<EGlobalRaceCarIndex>(3)) == E_ACTIVE_RACE_CAR_INDEX_0 &&
          lpTable->GetActiveRaceCarIndex(static_cast<EGlobalRaceCarIndex>(7)) == static_cast<EActiveRaceCarIndex>(1) &&
          lpTable->GetActiveRaceCarIndex(static_cast<EGlobalRaceCarIndex>(20)) == static_cast<EActiveRaceCarIndex>(4) &&
          lpTable->GetActiveRaceCarIndex(static_cast<EGlobalRaceCarIndex>(12)) == E_ACTIVE_RACE_CAR_INDEX_INVALID &&
          lpTable->GetActiveRaceCarIndex(static_cast<EGlobalRaceCarIndex>(0)) == E_ACTIVE_RACE_CAR_INDEX_INVALID,
          "the conversions cases 113 / 223 make: 3 -> 0, 7 -> 1, 20 -> 4, 12 -> -1, an unpublished slot -> -1 (Clear)");
    Check(lpTable->GetPlayerGlobalRaceCarIndex() == static_cast<EGlobalRaceCarIndex>(3) &&
          lpTable->GetRaceCarSpeed(static_cast<EGlobalRaceCarIndex>(20)) == 12.25f &&
          lpTable->IsPlayer(static_cast<EGlobalRaceCarIndex>(3)) && lpTable->IsRivalAI(static_cast<EGlobalRaceCarIndex>(7)),
          "the whole table travels, not only the index map (player slot, speeds, flags)");
    Check(std::memcmp(lau8UsedBefore, gaInputStorage + 0x980, sizeof(lau8UsedBefore)) == 0,
          "the copy stops at the table: mUsedRaceCars (+0x980) is untouched");

    // 3. Next frame the world re-seats a rival: the input follows (a whole copy every frame, no merge).
    gWorld.Table().Clear();
    Publish(3, 0, 30.0f, true);
    Publish(7, 2, 27.0f, false);
    gWorld.Table().SetPlayerGlobalRaceCarIndex(static_cast<EGlobalRaceCarIndex>(3));
    Step8(&Input(), &gWorld);
    Check(lpTable->GetActiveRaceCarIndex(static_cast<EGlobalRaceCarIndex>(7)) == static_cast<EActiveRaceCarIndex>(2) &&
          lpTable->GetActiveRaceCarIndex(static_cast<EGlobalRaceCarIndex>(20)) == E_ACTIVE_RACE_CAR_INDEX_INVALID,
          "a later publish replaces the earlier table (7 -> 2 now, 20 cleared)");

    Check(gAsserts == 0, "no assert fired");

    std::printf("FxDirectorGlobalRaceCars: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
