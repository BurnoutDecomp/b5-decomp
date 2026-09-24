// FX-SHOWTIME2 (crash parity 2026-09-24): the PRODUCTION VehicleManager::ProcessAftertouchEvents
// @0x82633DE8, extracted from src/GameSource/Physics/VehicleManager/BrnVehicleManager_UpdateVehiclePhysics.cpp
// by run_fxshowtime2_bounce_producer.py, run against a stand-in VehicleManager / RaceCarPhysics / driver
// whose GetRecentBounce writes a distinct value through each of its seven outputs, with the REAL
// CgsModule::VariableEventQueue<1536,16> it posts onto. The posted bytes are then read back through the
// game-state header's own JustBouncedEvent (BrnGameEvents.h) -- the record ProcessGameEvents' case 52
// consumes -- so the test pins that the producer's GetRecentBounce outputs land in the DWARF-named
// members, one for one.
//
// The numeric half passes on the pre-fold body too (the fold keeps the bytes); the runner's wiring
// half is what tells the folded producer from the local-struct fork.
//
// ARTIST asm (r31 = the car, r29 = the queue, the bounce record at r1+0x70):
//   0x82633E04  lbz 0xE50 (IsCrashing) == 0 -> return
//   0x82633E14..0x82633E58  event 76 {index @+0, out2 (sp+0x5C) @+4, out1 (sp+0x60) @+8}, r7 =
//               (meShowtimeBehaviour == 2), AddEvent(76, 12)
//   0x82633E5C..0x82633E7C  vtable +0x14 (IsPlayerVehicleActuallyInShowtime) == 0 -> return
//   0x82633E80..0x82633EC0  GetRecentBounce(r4 +0x00, r5..r8 +0x04..+0x07, r9 +0x08, r10 +0x10);
//               true -> AddEvent(0x34, 0x20)
//   0x82633EC4..0x82633EF0  lbz/stb 0 byte_82FB848A; was set -> AddEvent(0x35, 1)
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/GameState/BrnGameEvents.h"                // the real JustBouncedEvent / JustAppliedExtraSpinEvent
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static unsigned gChecks = 0, gFailures = 0;
static unsigned gAsserts = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Log
{
    DebugPrint* gpDebugPrint = nullptr;
}
namespace Message
{
    u64 gxMessageFilterFlags = 0;
}
}

typedef CgsModule::VariableEventQueue<1536, 16> GameEventQueue;

namespace BrnPhysics
{
namespace Vehicle
{
// The one byte of the showtime singleton the body touches (byte_82FB848A).
struct PlayerParametersStandIn
{
    bool mbSixaxisTiltApplied = false;
};
PlayerParametersStandIn msPlayerParams;

// What GetRecentBounce hands back this frame.
struct RecentBounceScript
{
    bool mbReturn;
    s32  miChain;
    bool mab[4];     // the second..fifth outputs, in parameter order
    s32  miEntity;
    f32  maf[4];     // the seventh output, all four lanes
};

class RaceCarPhysics
{
public:
    bool               mbCrashing   = true;
    bool               mbInShowtime = true;
    RecentBounceScript mScript      = {};
    s32                miBounceReads = 0;

    bool IsCrashing() const { return mbCrashing; }
    bool IsPlayerVehicleActuallyInShowtime() { return mbInShowtime; }

    // This tree's signature (RaceCarPhysics.h:544) -- the body under test is written against it.
    bool GetRecentBounce(s32* lpChainCount, bool* lpOverMinStress, bool* lpCarBounce, bool* lpGoodImpact,
                         bool* lpExtraFlag, s32* lpOtherEntityId, Vector3* lpBounceDirection)
    {
        ++miBounceReads;
        *lpChainCount    = mScript.miChain;
        *lpOverMinStress = mScript.mab[0];
        *lpCarBounce     = mScript.mab[1];
        *lpGoodImpact    = mScript.mab[2];
        *lpExtraFlag     = mScript.mab[3];
        *lpOtherEntityId = mScript.miEntity;
        std::memcpy(lpBounceDirection, mScript.maf, 16);
        return mScript.mbReturn;
    }
};

struct ControlsStandIn
{
    f32  mfOut1 = 0.25f;
    f32  mfOut2 = -0.75f;
    mutable bool mbLastSixaxis = false;

    void GetAftertouchValues(f32& lrfOut1, f32& lrfOut2, f32& lrfOut3, bool lbSixaxis) const
    {
        lrfOut1 = mfOut1;
        lrfOut2 = mfOut2;
        lrfOut3 = 99.0f;
        mbLastSixaxis = lbSixaxis;
    }
};

struct DriverStandIn
{
    ControlsStandIn mControls;
};

class VehicleManager
{
public:
    RaceCarPhysics maRaceCarVehicles[2];
    DriverStandIn  maRaceCarDrivers[2];
    u32            meShowtimeBehaviour = 0;

    void ProcessAftertouchEvents(s32 liRaceCarIndex, CgsModule::VariableEventQueue<1536, 16>* lpOutputQueue);
};

#include "producer_methods.inc"
}
}

using namespace BrnPhysics::Vehicle;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("  FAIL %s\n", lpcName);
    }
    else
    {
        std::printf("  ok   %s\n", lpcName);
    }
}

struct PostedEvent
{
    s32 miType;
    s32 miSize;
    alignas(16) u8 maBytes[64];
};

static std::vector<PostedEvent> Drain(GameEventQueue& lrQueue)
{
    std::vector<PostedEvent> laEvents;
    const CgsModule::Event* lpEvent = nullptr;
    s32 liSize = 0;
    s32 liType = lrQueue.GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent)
    {
        PostedEvent lPosted;
        std::memset(&lPosted, 0xCD, sizeof(lPosted));
        lPosted.miType = liType;
        lPosted.miSize = liSize;
        std::memcpy(lPosted.maBytes, lpEvent, (liSize < 64) ? liSize : 64);
        laEvents.push_back(lPosted);
        const CgsModule::Event* lpNext = nullptr;
        liType = lrQueue.GetNextEvent(lpEvent, &lpNext, &liSize);
        lpEvent = lpNext;
    }
    lrQueue.Clear();
    return laEvents;
}

static s32 Word(const PostedEvent& lr, s32 liOffset)
{
    s32 liValue = 0;
    std::memcpy(&liValue, lr.maBytes + liOffset, 4);
    return liValue;
}

static f32 Float(const PostedEvent& lr, s32 liOffset)
{
    f32 lfValue = 0.0f;
    std::memcpy(&lfValue, lr.maBytes + liOffset, 4);
    return lfValue;
}

int main()
{
    static VehicleManager lManager;
    static GameEventQueue lQueue;
    lQueue.Construct();

    RaceCarPhysics& lrCar = lManager.maRaceCarVehicles[1];
    lrCar.mScript.mbReturn = true;
    lrCar.mScript.miChain  = 3;
    lrCar.mScript.mab[0]   = true;     // -> +0x04
    lrCar.mScript.mab[1]   = false;    // -> +0x05
    lrCar.mScript.mab[2]   = true;     // -> +0x06
    lrCar.mScript.mab[3]   = false;    // -> +0x07
    lrCar.mScript.miEntity = 0x1234ABCD;
    const f32 lafVector[4] = { 1.0f, -2.5f, 3.75f, 42.0f };
    std::memcpy(lrCar.mScript.maf, lafVector, 16);
    lManager.meShowtimeBehaviour = 2;
    msPlayerParams.mbSixaxisTiltApplied = true;

    // ---- a crashing showtime car that bounced, with the tilt latch set ---------------------------
    std::printf("-- crashing, in showtime, bounced, tilt latch set\n");
    lManager.ProcessAftertouchEvents(1, &lQueue);
    std::vector<PostedEvent> laPosted = Drain(lQueue);
    Check(laPosted.size() == 3 && laPosted[0].miType == 76 && laPosted[1].miType == 52 && laPosted[2].miType == 53,
          "posts 76, then 52, then 53 (the console's order)");
    if (laPosted.size() == 3)
    {
        const PostedEvent& lr76 = laPosted[0];
        Check(lr76.miSize == 12 && Word(lr76, 0) == 1 && Float(lr76, 4) == -0.75f && Float(lr76, 8) == 0.25f,
              "event 76: 12 bytes {index, out2, out1} (stw idx sp+0x58; outs at sp+0x5C / sp+0x60)");
        Check(lManager.maRaceCarDrivers[1].mControls.mbLastSixaxis,
              "GetAftertouchValues gets r7 = (meShowtimeBehaviour == 2)");

        const PostedEvent& lr52 = laPosted[1];
        Check(lr52.miSize == 32, "event 52 is 32 bytes (li r6, 0x20 @0x82633EB0)");
        Check(Word(lr52, 0x00) == 3, "+0x00 = GetRecentBounce's first output (r4 = record+0x00)");
        Check(lr52.maBytes[0x04] == 1 && lr52.maBytes[0x05] == 0 && lr52.maBytes[0x06] == 1 && lr52.maBytes[0x07] == 0,
              "+0x04..+0x07 = the second..fifth outputs in parameter order (r5..r8)");
        Check(static_cast<u32>(Word(lr52, 0x08)) == 0x1234ABCDu, "+0x08 = the sixth output, one word (r9)");
        Check(std::memcmp(lr52.maBytes + 0x10, lafVector, 16) == 0,
              "+0x10..+0x1F = the seventh output, all four lanes (r10 = record+0x10)");

        // The same bytes through the consumer's record: the DWARF names, one for one.
        BrnGameState::GameStateModuleIO::JustBouncedEvent lRead;
        std::memcpy(&lRead, lr52.maBytes, sizeof(lRead));
        Check(lRead.miBounceChain == 3 && lRead.mbFromStationary && !lRead.mbOnCar && lRead.mbBoostedBounce
                  && !lRead.mbGoodImpact && lRead.midImpactEntityId.muValue == 0x1234ABCDu
                  && std::memcmp(&lRead.mContactPoint, lafVector, 16) == 0,
              "read back as JustBouncedEvent: lpiBounceChain / lpbFromStationary / lpbOnCar / lpbBoostedBounce / "
              "lpbGoodImpact / lpidImpactEntityId / lpContactPoint (DWARF GetRecentBounce :319) land in "
              "the same-named members");

        const PostedEvent& lr53 = laPosted[2];
        Check(lr53.miSize == 1, "event 53 is 1 byte (li r6, 1 @0x82633EE0)");
    }
    Check(!msPlayerParams.mbSixaxisTiltApplied, "the tilt latch is consumed (stb 0 @0x82633ED4)");
    Check(lrCar.miBounceReads == 1, "GetRecentBounce is read once per call");

    // ---- no bounce this frame, latch clear ------------------------------------------------------
    std::printf("-- in showtime, no bounce, latch clear\n");
    lrCar.mScript.mbReturn = false;
    lManager.meShowtimeBehaviour = 1;
    lManager.ProcessAftertouchEvents(1, &lQueue);
    laPosted = Drain(lQueue);
    Check(laPosted.size() == 1 && laPosted[0].miType == 76,
          "GetRecentBounce false and the latch clear: only event 76 (beq @0x82633EAC / 0x82633EDC)");
    Check(!lManager.maRaceCarDrivers[1].mControls.mbLastSixaxis, "r7 = 0 when the behaviour is not 2");

    // ---- not in showtime -------------------------------------------------------------------------
    std::printf("-- crashing, not in showtime\n");
    lrCar.mbInShowtime = false;
    lrCar.mScript.mbReturn = true;
    msPlayerParams.mbSixaxisTiltApplied = true;
    const s32 liReadsBefore = lrCar.miBounceReads;
    lManager.ProcessAftertouchEvents(1, &lQueue);
    laPosted = Drain(lQueue);
    Check(laPosted.size() == 1 && laPosted[0].miType == 76, "outside showtime: only event 76");
    Check(lrCar.miBounceReads == liReadsBefore && msPlayerParams.mbSixaxisTiltApplied,
          "...GetRecentBounce is not read and the latch is left set (the vtable +0x14 gate)");

    // ---- not crashing ----------------------------------------------------------------------------
    std::printf("-- not crashing\n");
    lrCar.mbCrashing = false;
    lManager.ProcessAftertouchEvents(1, &lQueue);
    laPosted = Drain(lQueue);
    Check(laPosted.empty(), "a car that is not crashing posts nothing (lbz 0xE50 gate)");

    Check(gAsserts == 0, "no assert fired");

    std::printf("FxShowtime2BounceProducer: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
