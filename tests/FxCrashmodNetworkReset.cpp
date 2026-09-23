// FX-CRASHMOD (crash parity 2026-09-23), G64-D4 / G65-D1: replay the production
// CrashModule::ResetCrashedNetworkRaceCars (ARTIST 0x827CE6E0) and
// CrashModule::OnContactFromNetworkPlayer (0x827C62E8) against a real driver-update queue.
//   * an E_DRIVER_TYPE_NETWORK (2) record whose mbCrash (+0xB9) is clear, for a car whose
//     mCrashingRaceCars bit is set, ends that car's crash: ResetRaceCarFromCrashIndex(out, crash,
//     false) posts CRASH COMPLETE (not removed), then OnContactFromNetworkPlayer(car) refreshes the
//     car's confirmed-network traffic wrecks to KF_NETWORK_CRASH_TIMEOUT (flt_820CA5A8 == 20.0f);
//   * mbCrash set, bit clear, or a non-network record: nothing happens.
// The bodies are extracted verbatim from BrnCrashModule_RaceCarCrashes.cpp by
// run_fxcrashmod_network_reset.py, which also checks PreSceneUpdate's call site (0x827D3B74).
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/World/CrashModule/BrnCrashModule.h"
#include "rw/math/vpu/vector3_operation.h"
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleIO.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"
#undef protected
#undef private
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstdlib>
#include <memory>

static unsigned assertions = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* message, const char*, int) { ++assertions; std::fprintf(stderr, "ASSERT: %s\n", message); return 0; }
void* EndAssert() { return nullptr; }
} namespace Log { DebugPrint* gpDebugPrint = nullptr; void WriteToLog(const char*) {} }
namespace Message { u64 gxMessageFilterFlags = 0; } }

using namespace BrnWorld;
using BrnPhysics::Vehicle::VehicleDriverInputInterface;

struct CrashFixture
{
    static constexpr u32 KU_INVALID_CRASH = 0xffffffffu;
    decltype(CrashModule::mRaceCarCrashes)   mRaceCarCrashes;
    decltype(CrashModule::mTrafficCrashes)   mTrafficCrashes;
    decltype(CrashModule::mCrashingRaceCars) mCrashingRaceCars;
    bool mbIsOnlineGameMode = true;
    CrashFixture() { mRaceCarCrashes.Clear(); mTrafficCrashes.Clear(); mCrashingRaceCars.UnSetAll(); }
    void ResetCrashedNetworkRaceCars(const CrashIO::InputBuffer_PreScene*, CrashIO::OutputBuffer_PreScene*);
    void OnContactFromNetworkPlayer(EActiveRaceCarIndex);
    void ResetRaceCarFromCrashIndex(CrashIO::OutputBuffer_PreScene*, u32, bool);
    u32  FindCrashForRaceCar(EActiveRaceCarIndex) const;
};
#include "fxcrashmod_network_reset_methods.inc"

namespace
{
    u64 RaceCarVolume(u32 luSlot) { return static_cast<u64>(0x01000000u | (luSlot << 10)) << 32; }
}

int main()
{
    unsigned checks = 0, failures = 0;
    auto Check = [&](bool lbPass, const char* lpcName)
    {
        ++checks;
        if (!lbPass) { ++failures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
    };

    auto input  = std::make_unique<CrashIO::InputBuffer_PreScene>();
    auto output = std::make_unique<CrashIO::OutputBuffer_PreScene>();
    auto* complete = output->mRaceCarOutputInterface.GetRaceCarCrashCompleteEventQueue();
    complete->Construct();
    output->mxStatusFlags.SetBit(CgsModule::IOBuffer::eStatusLockedForWrite);

    CrashFixture m;
    auto AddRace = [&](u32 luSlot, f32 lfSeconds)
    {
        RaceCarCrash lCrash{};
        lCrash.mRaceCarVolumeInstanceId.muId = RaceCarVolume(luSlot);
        lCrash.mfSecondsBeforeCleanup = lfSeconds;
        m.mRaceCarCrashes.Append(lCrash);
        m.mCrashingRaceCars.SetBit(luSlot);
    };
    auto AddTraffic = [&](s32 liOwner, u16 luVehicle, u8 lxFlags)
    {
        TrafficCrash lCrash{};
        lCrash.Construct(liOwner, luVehicle, 2.0f, false);
        lCrash.mxFlags = lxFlags;
        m.mTrafficCrashes.Append(lCrash);
    };
    auto Seconds = [&](u32 luSlot) -> f32
    {
        for (u32 i = 0; i < m.mRaceCarCrashes.GetLength(); ++i)
            if (m.mRaceCarCrashes.GetItem(i).GetOwner() == static_cast<s32>(luSlot))
                return m.mRaceCarCrashes.GetItem(i).mfSecondsBeforeCleanup;
        return -999.0f;
    };
    auto TrafficTime = [&](u16 luVehicle) -> f32
    {
        for (u32 i = 0; i < m.mTrafficCrashes.GetLength(); ++i)
            if (m.mTrafficCrashes.GetItem(i).GetVehicleIndex() == luVehicle)
                return m.mTrafficCrashes.GetItem(i).mfTimeTillClearup;
        return -999.0f;
    };

    // Crashes: cars 3, 4 and 5 are crashing; car 3 owns a confirmed (bit 0x4) and an unconfirmed
    // (bit 0x2) network traffic wreck; car 6 is not crashing.
    AddRace(3, 1.0f); AddRace(4, 1.5f); AddRace(5, 3.0f);
    AddTraffic(3, 101, 4); AddTraffic(3, 102, 2); AddTraffic(5, 103, 4);

    // The driver queue: a network record for car 3 (no longer crashing), car 4 (still crashing),
    // car 6 (never crashed) and a non-network (AI, type 1) record for car 5 with mbCrash clear.
    auto lpDrivers = std::make_unique<VehicleDriverInputInterface>();
    lpDrivers->Construct();
    auto AddDriver = [&](s32 liType, s32 liCar, bool lbCrash)
    {
        BrnPhysics::Vehicle::BrnNetworkDriverControls lDriver;
        lDriver.miVehicleID = liCar;
        lDriver.mbCrash = lbCrash;
        lpDrivers->GetUpdateDriverQueue()->AddEvent(&lDriver, liType, sizeof(lDriver));
    };
    AddDriver(BrnPhysics::Vehicle::E_DRIVER_TYPE_NETWORK, 3, false);
    AddDriver(BrnPhysics::Vehicle::E_DRIVER_TYPE_NETWORK, 4, true);
    AddDriver(BrnPhysics::Vehicle::E_DRIVER_TYPE_NETWORK, 6, false);
    AddDriver(BrnPhysics::Vehicle::E_DRIVER_TYPE_AI, 5, false);

    input->mxStatusFlags.SetBit(CgsModule::IOBuffer::eStatusLockedForWrite);
    input->SetVehicleDriverInterface(lpDrivers.get());
    input->mxStatusFlags.UnSetBit(CgsModule::IOBuffer::eStatusLockedForWrite);
    input->mxStatusFlags.SetBit(CgsModule::IOBuffer::eStatusLockedForRead);

    m.ResetCrashedNetworkRaceCars(input.get(), output.get());

    Check(m.FindCrashForRaceCar(static_cast<EActiveRaceCarIndex>(3)) == CrashFixture::KU_INVALID_CRASH,
          "network car that stopped crashing: its crash record is ended");
    Check(!m.mCrashingRaceCars.IsBitSet(3), "network car that stopped crashing: crashing bit cleared");
    Check(complete->GetLength() == 1 && complete->GetEvent(0).mRaceCarVolumeInstanceId.muId == RaceCarVolume(3),
          "network car that stopped crashing: exactly one CRASH COMPLETE, for car 3");
    Check(complete->GetLength() == 1 && !complete->GetEvent(0).mbRemoveRaceCar,
          "network crash end does not remove the car (li r6, 0)");
    Check(TrafficTime(101) == 20.0f, "OnContactFromNetworkPlayer: car 3's confirmed traffic wreck -> 20.0 s");
    Check(TrafficTime(102) == 2.0f,  "unconfirmed network traffic wreck keeps its timer");
    Check(TrafficTime(103) == 2.0f,  "another player's confirmed traffic wreck keeps its timer");
    Check(Seconds(4) == 1.5f && m.mCrashingRaceCars.IsBitSet(4), "network car still crashing (mbCrash) is left alone");
    Check(Seconds(5) == 3.0f && m.mCrashingRaceCars.IsBitSet(5), "a non-network driver record is ignored");
    Check(m.mRaceCarCrashes.GetLength() == 2, "only the one crash was ended");

    // OnContactFromNetworkPlayer on its own: car 4's race-car record and its confirmed traffic.
    AddTraffic(4, 104, 4);
    m.OnContactFromNetworkPlayer(static_cast<EActiveRaceCarIndex>(4));
    Check(Seconds(4) == 20.0f, "OnContactFromNetworkPlayer: the player's race-car record -> 20.0 s");
    Check(TrafficTime(104) == 20.0f, "OnContactFromNetworkPlayer: the player's confirmed traffic -> 20.0 s");
    Check(Seconds(5) == 3.0f && TrafficTime(103) == 2.0f, "OnContactFromNetworkPlayer leaves other owners alone");

    Check(assertions == 0, "valid records raise no assertion");
    std::printf("FxCrashmodNetworkReset: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
