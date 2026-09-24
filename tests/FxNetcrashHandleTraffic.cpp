// FX-NETCRASH (crash parity 2026-09-24), G64-D2 / G65-D2 part 2: replay the production
// CrashModule::HandleNetworkCrashingTraffic (ARTIST 0x827CB788, 1268 insns, read from the asm)
// against the real crash-side NetworkInputInterface, a real OutputBuffer_PreScene (its
// VehicleInputInterface::mUpdateNetworkTrafficEventQueue and TrafficOutputInterface::
// mStartCrashingNetworkTrafficQueue) and the real TrafficCrash / Set<u16,160> / FastBitArray
// containers. One remote player (car 1) sends one frame of crashing-traffic updates while the
// local player is car 0:
//   * vehicle 10 -- not crashing here: a NEW network wreck. UpdateNetworkTraffic is posted, a
//     TrafficCrash {owner 1, vehicle 10} is opened CONFIRMED (flags 4) with KF_NETWORK_CRASH_TIMEOUT
//     (flt_820CA5A8 20.0f), mCrashingTraffic + mCrashingNetworkTraffic are set, 10 joins car 1's set
//     and one StartNetworkTrafficVehicleCrashing {10} event is posted (0x827CC06C..0x827CC414);
//   * vehicle 20 -- crashing here as an UNCONFIRMED network wreck of car 2: posted; ownership moves
//     to car 1 (erased from car 2's set, inserted into car 1's), confirmed (flags 2 -> 4)
//     (0x827CBF78..0x827CC064);
//   * vehicle 30 -- our own (car 0) wreck, not network: CONTENTIOUS, nothing posted
//     (0x827CBC88..0x827CBCA8);
//   * vehicle 40 -- slammed by car 3: CONTENTIOUS (maiSlammedTrafficOwners, 0x827CBCC0..0x827CBCDC);
//   * vehicle 50 -- car 1's confirmed wreck that already wants clearing up: CONTENTIOUS (flag bit 0,
//     0x827CBCAC), so it is in car 1's old set but not in this frame's -> cleared up;
//   * vehicle 60 -- car 1's confirmed wreck it stopped sending: SetNetworkVehicleClearedUp (flags |= 1)
//     (0x827CCAA0..0x827CCAE0);
//   * vehicle 70 -- car 1's UNCONFIRMED wreck it stopped sending: skipped (0x827CC618);
//   * then OnContactFromNetworkPlayer(1) (0x827CCAF8) refreshes EVERY confirmed wreck car 1 owns to
//     20.0 -- including 50 and 60, whose -1.0 from SetNetworkVehicleClearedUp it overwrites (the
//     console's order: the cleared-up loop runs first);
//   * car 2 is not marked for update, so nothing of its is touched except the ownership move of 20.
// run_fxnetcrash_handle_traffic.py extracts every body verbatim (--pre-fix <rev> replays a missing
// body EMPTY, as the pre-fix PC ran nothing) and checks PreSceneUpdate's call site
// (0x827D3B4C..0x827D3B78).
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/World/CrashModule/BrnCrashModule.h"
#include "rw/math/vpu/vector3_operation.h"
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleIO.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"
#undef protected
#undef private
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

static unsigned assertions = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* message, const char*, int) { ++assertions; std::fprintf(stderr, "ASSERT: %s\n", message); return 0; }
void* EndAssert() { return nullptr; }
} namespace Log {
DebugPrint* gpDebugPrint = nullptr;   // the [netcrash] witness stays silent
StrStreamBase& DebugPrint::operator<<(const char*) { return *this; }
void WriteToLog(const char*) {} }
namespace Message { u64 gxMessageFilterFlags = 0; } }

using namespace BrnWorld;

struct CrashFixture
{
    static constexpr u32 KU_INVALID_CRASH = 0xffffffffu;
    decltype(CrashModule::mRaceCarCrashes)             mRaceCarCrashes;
    decltype(CrashModule::mTrafficCrashes)             mTrafficCrashes;
    decltype(CrashModule::mCrashingTraffic)            mCrashingTraffic;
    decltype(CrashModule::mCrashingNetworkTraffic)     mCrashingNetworkTraffic;
    decltype(CrashModule::maiSlammedTrafficOwners)     maiSlammedTrafficOwners;
    decltype(CrashModule::maCrashingTrafficForPlayers) maCrashingTrafficForPlayers;
    EActiveRaceCarIndex meLocalActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
    bool mbIsOnlineGameMode = true;
    CrashFixture()
    {
        mRaceCarCrashes.Clear(); mTrafficCrashes.Clear();
        mCrashingTraffic.UnSetAll(); mCrashingNetworkTraffic.UnSetAll();
        std::memset(maiSlammedTrafficOwners, 0xFF, sizeof(maiSlammedTrafficOwners));
        for (u32 i = 0; i < 8; ++i) maCrashingTrafficForPlayers[i].Clear();
    }
    void HandleNetworkCrashingTraffic(const CrashIO::InputBuffer_PreScene*, CrashIO::OutputBuffer_PreScene*);
    void OnContactFromNetworkPlayer(EActiveRaceCarIndex);
    u32  FindCrashForTrafficVehicle(u32) const;
};
#include "fxnetcrash_handle_traffic_methods.inc"

namespace
{
    Matrix44Affine Transform(f32 lfSeed)
    {
        Matrix44Affine lTransform;
        lTransform.xAxis = Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
        lTransform.yAxis = Vector3{ 0.0f, 1.0f, 0.0f, 0.0f };
        lTransform.zAxis = Vector3{ 0.0f, 0.0f, 1.0f, 0.0f };
        lTransform.wAxis = Vector3{ lfSeed, 2.0f * lfSeed, -lfSeed, 1.0f };
        return lTransform;
    }
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
    input->mNetworkInputInterface.Construct();
    input->mxStatusFlags.SetBit(CgsModule::IOBuffer::eStatusLockedForRead);
    auto& updates  = output->mVehicleInputInterface.mUpdateNetworkTrafficEventQueue;
    auto& starting = output->mTrafficOutputInterface.mStartCrashingNetworkTrafficQueue;
    updates.Construct();
    starting.Construct();
    output->mxStatusFlags.SetBit(CgsModule::IOBuffer::eStatusLockedForWrite);

    CrashFixture m;
    auto AddCrash = [&](s32 liOwner, u16 luVehicle, u8 lxFlags, f32 lfTime, bool lbNetworkBit, bool lbInOwnerSet)
    {
        TrafficCrash lCrash{};
        lCrash.Construct(liOwner, luVehicle, lfTime, false);
        lCrash.mxFlags = lxFlags;
        m.mTrafficCrashes.Append(lCrash);
        m.mCrashingTraffic.SetBit(luVehicle);
        if (lbNetworkBit) m.mCrashingNetworkTraffic.SetBit(luVehicle);
        if (lbInOwnerSet) m.maCrashingTrafficForPlayers[liOwner].Insert(luVehicle);
    };
    auto Crash = [&](u16 luVehicle) -> const TrafficCrash*
    {
        for (u32 i = 0; i < m.mTrafficCrashes.GetLength(); ++i)
            if (m.mTrafficCrashes.GetItem(i).GetVehicleIndex() == luVehicle)
                return &m.mTrafficCrashes.GetItem(i);
        return nullptr;
    };

    AddCrash(2, 20, 2, 5.0f, false, true);    // car 2's unconfirmed network wreck
    AddCrash(0, 30, 0, 3.5f, false, true);    // our own (local) wreck
    AddCrash(1, 50, 4 | 1, 2.0f, true, true); // car 1's confirmed wreck that wants clearing up
    AddCrash(1, 60, 4, 7.0f, true, true);     // car 1's confirmed wreck, not re-sent this frame
    AddCrash(1, 70, 2, 9.0f, false, true);    // car 1's unconfirmed wreck, not re-sent this frame
    m.maiSlammedTrafficOwners[40] = 3;        // car 3 slammed vehicle 40

    // One frame of car 1's crashing-traffic updates, through the network module's own producer.
    input->mNetworkInputInterface.MarkRaceCarForUpdate(1);
    const u16 kaVehicles[] = { 10, 20, 30, 40, 50 };
    for (u32 i = 0; i < sizeof(kaVehicles) / sizeof(kaVehicles[0]); ++i)
        input->mNetworkInputInterface.AddTrafficUpdate(kaVehicles[i], 1, Transform(1.0f + static_cast<f32>(i)));

    m.HandleNetworkCrashingTraffic(input.get(), output.get());

    // ---- the physics replay: exactly the two uncontested vehicles, in queue order ----
    Check(updates.GetLength() == 2, "two UpdateNetworkTraffic events (vehicles 10 and 20)");
    {
        const bool lbTwo = updates.GetLength() == 2;
        const Matrix44Affine t0 = Transform(1.0f), t1 = Transform(2.0f);
        Check(lbTwo && updates.GetEvent(0).mVolumeInstanceID.muId == BrnTraffic::MakeTrafficVolumeInstanceId(10).muId &&
              updates.GetEvent(0).mVolumeInstanceID.muId == (static_cast<u64>(0x02000000u | (10u << 10)) << 32),
              "event 0 names traffic vehicle 10 (owner 2, index 10)");
        Check(lbTwo && updates.GetEvent(1).mVolumeInstanceID.muId == (static_cast<u64>(0x02000000u | (20u << 10)) << 32),
              "event 1 names traffic vehicle 20");
        Check(lbTwo && std::memcmp(&updates.GetEvent(0).mTransform, &t0, sizeof(t0)) == 0,
              "vehicle 10 carries its network transform");
        Check(lbTwo && std::memcmp(&updates.GetEvent(1).mTransform, &t1, sizeof(t1)) == 0,
              "vehicle 20 carries its network transform");
    }

    // ---- vehicle 10: a new confirmed network wreck of car 1 ----
    const TrafficCrash* c10 = Crash(10);
    Check(c10 != nullptr && c10->GetOwner() == 1, "vehicle 10 gets a TrafficCrash owned by car 1");
    Check(c10 != nullptr && c10->mxFlags == 4, "vehicle 10's wreck is CONFIRMED network (flags 2 -> 4)");
    Check(c10 != nullptr && c10->mfTimeTillClearup == 20.0f, "vehicle 10's timer is KF_NETWORK_CRASH_TIMEOUT 20.0");
    Check(m.mCrashingTraffic.IsBitSet(10) && m.mCrashingNetworkTraffic.IsBitSet(10),
          "vehicle 10 is crashing and network-crashing");
    Check(m.maCrashingTrafficForPlayers[1].Contains(10), "vehicle 10 joins car 1's crashing set");
    Check(starting.GetLength() == 1 && starting.GetEvent(0).muVehicleId == 10,
          "exactly one StartNetworkTrafficVehicleCrashing, for vehicle 10");

    // ---- vehicle 20: ownership moves from car 2 to car 1 and is confirmed ----
    const TrafficCrash* c20 = Crash(20);
    Check(c20 != nullptr && c20->GetOwner() == 1 && c20->mxFlags == 4, "vehicle 20: owner 2 -> 1, flags 2 -> 4");
    Check(!m.maCrashingTrafficForPlayers[2].Contains(20) && m.maCrashingTrafficForPlayers[1].Contains(20),
          "vehicle 20 moves from car 2's set to car 1's");
    Check(m.mCrashingNetworkTraffic.IsBitSet(20), "vehicle 20 becomes network-crashing");
    Check(c20 != nullptr && c20->mfTimeTillClearup == 20.0f, "vehicle 20's timer refreshed to 20.0 by the contact");

    // ---- contentious vehicles: untouched ----
    const TrafficCrash* c30 = Crash(30);
    Check(c30 != nullptr && c30->GetOwner() == 0 && c30->mxFlags == 0 && c30->mfTimeTillClearup == 3.5f,
          "vehicle 30 (our own wreck) is untouched");
    Check(!m.mCrashingTraffic.IsBitSet(40) && Crash(40) == nullptr, "vehicle 40 (slammed by car 3) is untouched");

    // ---- cleared up: 50 and 60 flagged, then refreshed by the contact; 70 skipped ----
    const TrafficCrash* c50 = Crash(50);
    const TrafficCrash* c60 = Crash(60);
    const TrafficCrash* c70 = Crash(70);
    Check(c60 != nullptr && (c60->mxFlags & 1) != 0 && (c60->mxFlags & 4) != 0,
          "vehicle 60 (no longer sent) wants clearing up and stays confirmed");
    Check(c60 != nullptr && c60->mfTimeTillClearup == 20.0f,
          "vehicle 60's -1.0 is overwritten by the contact's 20.0 (cleared-up loop runs first)");
    Check(c50 != nullptr && c50->mxFlags == 5 && c50->mfTimeTillClearup == 20.0f, "vehicle 50 is cleared up too");
    Check(c70 != nullptr && c70->mxFlags == 2 && c70->mfTimeTillClearup == 9.0f,
          "vehicle 70 (unconfirmed) is skipped by both loops");
    Check(m.maCrashingTrafficForPlayers[1].Contains(60) && m.maCrashingTrafficForPlayers[1].Contains(70),
          "the cleared-up loop does not erase from car 1's set");
    Check(m.mTrafficCrashes.GetLength() == 6, "one record added (vehicle 10), none removed");
    Check(assertions == 0, "no tripwire fires on a consistent frame");

    // ---- a second frame with car 1 marked but an EMPTY queue: everything it owned is cleared up ----
    input->mNetworkInputInterface.Clear();
    input->mNetworkInputInterface.MarkRaceCarForUpdate(1);
    updates.Clear();
    starting.Clear();
    m.HandleNetworkCrashingTraffic(input.get(), output.get());
    c10 = Crash(10); c20 = Crash(20);
    Check(updates.GetLength() == 0 && starting.GetLength() == 0, "an empty frame posts nothing");
    Check(c10 != nullptr && (c10->mxFlags & 1) != 0 && c20 != nullptr && (c20->mxFlags & 1) != 0,
          "an empty frame clears up car 1's confirmed wrecks (10, 20)");

    // ---- a car that is not marked is not touched at all ----
    const u32 luBeforeLength = m.mTrafficCrashes.GetLength();
    input->mNetworkInputInterface.Clear();
    updates.Clear();
    m.HandleNetworkCrashingTraffic(input.get(), output.get());
    Check(m.mTrafficCrashes.GetLength() == luBeforeLength && updates.GetLength() == 0,
          "no marked car: nothing happens");
    Check(assertions == 0, "no tripwire fires on the follow-up frames");

    std::printf("FxNetcrashHandleTraffic: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
