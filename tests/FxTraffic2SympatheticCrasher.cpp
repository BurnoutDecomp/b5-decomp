// FX-TRAFFIC2 (crash parity 2026-09-24, G58-D1): the PRODUCTION
//   TrafficEntityModule::GenerateSympatheticCrasherOutput                 @0x82715C30
//   Vehicle::IsCrashing                                                   @0x82704A70
//   Vehicle::IsSympatheticCrasher (DWARF BrnTrafficVehicle.h:387, inlined at 0x82715C80)
//   TrafficToRaceCarInterface_PreScene::SetSympatheticCrasher             @0x82710848
// extracted from the b5 sources by run_fxtraffic2_sympathetic_crasher.py and hosted on a fixture
// that has the module's real member types. Every expected value below is derived from the ARTIST
// asm (the decode is in the producer's banner in BrnTrafficEntityModule.cpp):
//   only in Showtime (lbzx +0x717DD ; beq -> return: nothing written outside it);
//   for the 400 standard slots (cmplwi r31,0x190), each frame, the slot's bit is SET iff the car
//   is alive (+5 bit 0, tested BEFORE IsCrashing, which asserts it), IsCrashing() (crash-traffic
//   byte == 0) and its latched sympathetic target is not -1 (lwz 0x40 ; cmpwi -1), else CLEARED;
//   slots 400+ are never visited.
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficToRaceCarInterface.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstddef>
#include <cstdio>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;
static const char* gpcLastAssert = "";

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int)
    {
        ++gAsserts;
        gpcLastAssert = lpcMessage;
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { unsigned long long gxMessageFilterFlags = 0; }
}

static unsigned guInterfaceFetches = 0;

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    // The lock-checked accessor (0x82710DD0) without the lock bookkeeping; counts its calls.
    OutputBuffer_PreScene::TrafficToRaceCarInterface_PreScene*
    OutputBuffer_PreScene::GetTrafficToRaceCarInterface_PreScene()
    {
        ++guInterfaceFetches;
        return &mTrafficToRaceCarInterface_PreScene;
    }
}

    // The TU-local diag helper the production body uses (BrnTrafficEntityModule.cpp).
    inline CgsDev::Log::DebugPrint* TrafficDiagStream() { return nullptr; }

    struct SympFixture
    {
        typedef TrafficEntityModule M;

        decltype(M::mbPlayingShowtimeMode) mbPlayingShowtimeMode;
        decltype(M::maVehicles)            maVehicles;

        void GenerateSympatheticCrasherOutput(const BrnTrafficIO::InputBuffer_PreScene* lpInput,
                                              BrnTrafficIO::OutputBuffer_PreScene* lpOutput);
    };
}

// The production bodies under test.
#include "sympathetic_crasher.inc"

using namespace BrnTraffic;
using namespace BrnTraffic::BrnTrafficIO;
typedef SympFixture Fixture;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

alignas(64) static unsigned char gaFixture[sizeof(Fixture)];
alignas(64) static unsigned char gaOutput[sizeof(OutputBuffer_PreScene)];

static Fixture& F()        { return *reinterpret_cast<Fixture*>(gaFixture); }
static OutputBuffer_PreScene& Out() { return *reinterpret_cast<OutputBuffer_PreScene*>(gaOutput); }
static TrafficToRaceCarInterface_PreScene& Iface() { return Out().mTrafficToRaceCarInterface_PreScene; }

static const u32 KU_NO_TARGET = 0xFFFFFFFFu;
static const u32 KU_A_TARGET  = (21u << 10) | 0x02000000u;   // a traffic entity id

// Every slot dead, no target, crash-traffic type 1 (not crashing).
static void Fresh(bool lbShowtime, bool lbPresetBits)
{
    std::memset(gaFixture, 0, sizeof(gaFixture));
    std::memset(gaOutput, 0, sizeof(gaOutput));
    Fixture& lr = F();
    lr.mbPlayingShowtimeMode = lbShowtime;
    for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
    {
        lr.maVehicles[luVehicle].mxFlags                  = 0;
        lr.maVehicles[luVehicle].muCrashTrafficType       = 1;
        lr.maVehicles[luVehicle].mSympCrashTarget.muValue = KU_NO_TARGET;
    }
    if (lbPresetBits)
    {
        Iface().mSympatheticCrashers.SetAll();
    }
    else
    {
        Iface().mSympatheticCrashers.UnSetAll();
    }
    gAsserts = 0;
    gpcLastAssert = "";
    guInterfaceFetches = 0;
}

static void Car(u32 luVehicle, bool lbAlive, u8 luCrashTrafficType, u32 luTarget)
{
    Vehicle& lrVehicle = F().maVehicles[luVehicle];
    lrVehicle.mxFlags = static_cast<u8>((lbAlive ? Vehicle::E_FLAG_ALIVE : 0) | Vehicle::E_FLAG_PHYSICAL);
    lrVehicle.muCrashTrafficType       = luCrashTrafficType;
    lrVehicle.mSympCrashTarget.muValue = luTarget;
}

static void Run()
{
    F().GenerateSympatheticCrasherOutput(nullptr, &Out());
}

static bool Bit(u32 luIndex)
{
    return Iface().mSympatheticCrashers.IsBitSet(luIndex);
}

static u32 NumBitsSet()
{
    u32 luCount = 0;
    for (u32 luIndex = 0; luIndex < KU_MAX_STANDARD_TRAFFIC; ++luIndex)
    {
        luCount += Bit(luIndex) ? 1u : 0u;
    }
    return luCount;
}

int main()
{
    // ---- Showtime: every standard slot is rewritten from the car's state ----------------------
    Fresh(true, true);                      // every bit pre-SET: a slot that is not a crasher must be cleared
    Car(0,   true,  0, KU_A_TARGET);        // alive, crashing, target -> crasher
    Car(1,   true,  0, KU_NO_TARGET);       // alive, crashing, no target (-1) -> not
    Car(2,   true,  1, KU_A_TARGET);        // alive, NOT crashing (type 1), target -> not
    Car(3,   false, 0, KU_A_TARGET);        // dead: IsCrashing must not even be asked (it asserts IsAlive)
    Car(5,   true,  0, 0x00000000u);        // target id 0 is valid: only -1 is the sentinel
    Car(399, true,  0, KU_A_TARGET);        // the last standard slot is visited
    Car(400, true,  0, KU_A_TARGET);        // the first slot past KU_MAX_STANDARD_TRAFFIC is not
    Run();
    Check(Bit(0),  "slot 0 (alive, crashing, target) is a sympathetic crasher");
    Check(!Bit(1), "slot 1 (crashing, target -1) is cleared");
    Check(!Bit(2), "slot 2 (alive with a target but not crashing) is cleared");
    Check(!Bit(3), "slot 3 (dead) is cleared");
    Check(Bit(5),  "slot 5 (target id 0) is a crasher -- the compare is against -1 only (cmpwi -1)");
    Check(Bit(399), "slot 399, the last standard slot, is visited (cmplwi 0x190)");
    Check(NumBitsSet() == 3u, "exactly the three crashers are set; every other pre-set bit is cleared");
    Check(gAsserts == 0, "no assert: IsAlive gates IsCrashing, and no index >= 400 reaches SetSympatheticCrasher");
    Check(guInterfaceFetches == KU_MAX_STANDARD_TRAFFIC,
          "the interface is fetched once per standard slot (0x82715C98 inside the loop): 400 writes");

    // ---- next frame: the target is dropped -> the bit is cleared again ------------------------
    F().maVehicles[0].mSympCrashTarget.muValue = KU_NO_TARGET;
    Run();
    Check(!Bit(0) && Bit(5) && Bit(399), "a car whose target is gone is cleared on the next Showtime frame");

    // ---- outside Showtime nothing is written -----------------------------------------------
    Fresh(false, true);
    Car(1, true, 0, KU_NO_TARGET);
    Run();
    Check(NumBitsSet() == KU_MAX_STANDARD_TRAFFIC && guInterfaceFetches == 0,
          "outside Showtime every pre-set bit stays set and the interface is never fetched");
    Fresh(false, false);
    Car(0, true, 0, KU_A_TARGET);
    Run();
    Check(NumBitsSet() == 0u, "outside Showtime a crasher is not published");

    std::printf("FxTraffic2SympatheticCrasher: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
