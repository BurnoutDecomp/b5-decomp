// FX-VMNET (crash parity 2026-09-23, G41-D1): the PRODUCTION body of
// VehicleManager::ProcessNetworkCarDisconnect @0x825C53F8 (BrnVehicleManagerPlayerStats.cpp) and the
// PRODUCTION VehicleDriver::ClearControls (BrnVehicleDriver.cpp) it calls, both extracted by
// run_fxvmnet_network_disconnect.py. The body is re-homed onto VmFixture, which carries the two
// members it touches with their REAL types: VehicleDriver maRaceCarDrivers[8] and
// CgsContainers::BitArray<8> mHiddenRaceCars. The payload is the real BrnGameActions.h record.
//
// The expected driver image is built HERE from the console's inlined store run
// 0x825C5464..0x825C54D0 (not from the ClearControls text), on a record pre-filled with 0xA5:
//     +0x00 s32 -1 | +0x04..+0x33 twelve 0.0f (flt_82001CC0) | +0x34 1.0f (flt_82001C98)
//     +0x38 0xFF   | +0x39 0 | +0x3A untouched (mbToggle) | +0x3B..+0x42 0
//     +0x44..+0x47 untouched (meDriverType) | +0x48 0.0f | +0x4C..+0x4E 0 | the rest untouched
// then `cmplwi r31,8` + "luIndex < NUMBITS" and `li 1; sld; ldx; andc; stdx` on mHiddenRaceCars.
// The host VehicleDriver / BrnPlayerDriverControls layouts are static_assert-pinned to those console
// offsets (BrnVehicleDriver.h, BrnVehicleDriverControls.h), so a byte comparison is meaningful.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnVehicleDriver.h"
#undef protected
#undef private
#include "GameShared/GameClasses/Containers/CgsBitArray.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstdio>
#include <cstring>

static unsigned giAsserts = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int) { ++giAsserts; std::printf("ASSERT: %s\n", lpcMessage); return 0; }
    void* EndAssert() { return nullptr; }
}
}

namespace BrnPhysics
{
namespace Vehicle
{
    // The production ClearControls (BrnVehicleDriver.cpp), pasted by the runner.
#include "clear_controls.inc"

    struct VmFixture
    {
        VehicleDriver              maRaceCarDrivers[8];
        CgsContainers::BitArray<8> mHiddenRaceCars;

        void ProcessNetworkCarDisconnect(
            const BrnGameState::GameStateModuleIO::RemotePlayerDisconnectedAction* lpPlayerDisconnectedAction);
    };

    // The production ProcessNetworkCarDisconnect (BrnVehicleManagerPlayerStats.cpp), or -- for the
    // pre-fix revision, where no body exists -- the old arm's behaviour (a one-shot log, no work).
#include "network_disconnect.inc"
}
}

using namespace BrnGameState::GameStateModuleIO;
using BrnPhysics::Vehicle::VehicleDriver;
using BrnPhysics::Vehicle::VmFixture;

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL: %s\n", lpcName); }
}

static const unsigned char KU8_FILL = 0xA5;

// The console image of one cleared driver record, built from the store list in the banner.
static void BuildExpectedDriver(unsigned char* lpu8Out)
{
    std::memset(lpu8Out, KU8_FILL, sizeof(VehicleDriver));
    const int liMinusOne = -1;
    std::memcpy(lpu8Out + 0x00, &liMinusOne, 4);              // stw r9(-1), 0x40(r11)
    const float lfZero = 0.0f, lfOne = 1.0f;
    for (int liOff = 0x04; liOff <= 0x30; liOff += 4)          // stfs f0 @0x44..0x70 (twelve)
        std::memcpy(lpu8Out + liOff, &lfZero, 4);
    std::memcpy(lpu8Out + 0x34, &lfOne, 4);                    // stfs f13, 0x74(r11)
    lpu8Out[0x38] = 0xFF;                                      // stb r9(-1), 0x78(r11)
    lpu8Out[0x39] = 0;                                         // stb r10, 0x79(r11)
    for (int liOff = 0x3B; liOff <= 0x42; ++liOff)             // stb r10 @0x7B..0x82
        lpu8Out[liOff] = 0;
    std::memcpy(lpu8Out + 0x48, &lfZero, 4);                   // stfs f0, 0x88(r11)
    for (int liOff = 0x4C; liOff <= 0x4E; ++liOff)             // stb r10 @0x8C..0x8E
        lpu8Out[liOff] = 0;
}

static VmFixture gVm;   // static storage: the Matrix44Affine members want 16-byte alignment

static void Reset(u32 luHiddenMask)
{
    std::memset(gVm.maRaceCarDrivers, KU8_FILL, sizeof(gVm.maRaceCarDrivers));
    for (u32 luBit = 0; luBit < 8; ++luBit)
    {
        if (luHiddenMask & (1u << luBit)) gVm.mHiddenRaceCars.SetBit(luBit);
        else                              gVm.mHiddenRaceCars.UnSetBit(luBit);
    }
}

static u32 HiddenMask()
{
    u32 luMask = 0;
    for (u32 luBit = 0; luBit < 8; ++luBit)
        if (gVm.mHiddenRaceCars.IsBitSet(luBit)) luMask |= 1u << luBit;
    return luMask;
}

static void Disconnect(s32 liRaceCar, s32 liPlayerId)
{
    RemotePlayerDisconnectedAction lAction;
    std::memset(&lAction, 0, sizeof(lAction));
    lAction.meActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(liRaceCar);
    lAction.mPlayerID = static_cast<BrnNetwork::NetworkPlayerID>(liPlayerId);
    gVm.ProcessNetworkCarDisconnect(&lAction);
}

static bool DriverUntouched(int liDriver)
{
    const unsigned char* lpu8 = reinterpret_cast<const unsigned char*>(&gVm.maRaceCarDrivers[liDriver]);
    for (size_t i = 0; i < sizeof(VehicleDriver); ++i)
        if (lpu8[i] != KU8_FILL) return false;
    return true;
}

int main()
{
    unsigned char lau8Expected[sizeof(VehicleDriver)];
    BuildExpectedDriver(lau8Expected);

    static const s32 KAI_CARS[] = { 0, 3, 7 };
    for (s32 liCar : KAI_CARS)
    {
        // The player id is deliberately a DIFFERENT live slot: the console reads the race-car index
        // at record +0 (`lwz r31, 0(r31)`), never the player id.
        const s32 liDecoy = (liCar + 5) % 8;
        Reset((1u << liCar) | (1u << liDecoy));
        Disconnect(liCar, liDecoy);

        char lacName[160];
        const unsigned char* lpu8 = reinterpret_cast<const unsigned char*>(&gVm.maRaceCarDrivers[liCar]);
        std::snprintf(lacName, sizeof(lacName),
                      "car %d: the driver record equals the console's inlined ClearControls image (0x825C5464..0x825C54D0)", liCar);
        Check(std::memcmp(lpu8, lau8Expected, sizeof(VehicleDriver)) == 0, lacName);

        std::snprintf(lacName, sizeof(lacName), "car %d: mbToggle (+0x3A) and meDriverType (+0x44) are NOT written", liCar);
        Check(lpu8[0x3A] == KU8_FILL && lpu8[0x44] == KU8_FILL && lpu8[0x47] == KU8_FILL, lacName);

        std::snprintf(lacName, sizeof(lacName), "car %d: mfBoostMaxSpeedScale is 1.0f (flt_82001C98), not 0", liCar);
        Check(gVm.maRaceCarDrivers[liCar].mControls.mfBoostMaxSpeedScale == 1.0f, lacName);

        bool lbOthersUntouched = true;
        for (int liOther = 0; liOther < 8; ++liOther)
            if (liOther != liCar && !DriverUntouched(liOther)) lbOthersUntouched = false;
        std::snprintf(lacName, sizeof(lacName), "car %d: the other seven drivers are untouched", liCar);
        Check(lbOthersUntouched, lacName);

        std::snprintf(lacName, sizeof(lacName), "car %d: its mHiddenRaceCars bit is cleared (andc/stdx @+44704)", liCar);
        Check(!gVm.mHiddenRaceCars.IsBitSet(static_cast<u32>(liCar)), lacName);

        std::snprintf(lacName, sizeof(lacName), "car %d: the decoy slot %d keeps its hidden bit", liCar, liDecoy);
        Check(HiddenMask() == (1u << liDecoy), lacName);
    }

    // The console clears with andc, not a toggle: a slot that was not hidden stays not hidden.
    Reset(1u << 6);
    Disconnect(2, 2);
    Check(HiddenMask() == (1u << 6), "an already-clear hidden bit stays clear (andc, not xor)");

    Check(giAsserts == 0, "no console assert fires on a valid race-car index");

    std::printf("FxVmnetNetworkDisconnect: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
