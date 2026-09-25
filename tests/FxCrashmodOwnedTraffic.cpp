// FX-CRASHMOD (crash parity 2026-09-23), G64-D3 / G65-D3: replay the production
// CrashModule::GenerateOwnedTrafficUpdates (ARTIST 0x827C53F0, read from the asm) against a real
// physical-traffic-state queue and the real NetworkOutputInterface:
//   * a crashing traffic vehicle owned by the local player (meLocalActiveRaceCarIndex) whose state
//     arrived this frame is published with that state's mTransform, in mTrafficCrashes order;
//   * another player's wreck, a wreck with no state, or any Showtime mode publishes nothing;
//   * a crashing vehicle with no state that is neither network-crashing nor about to be recycled
//     trips the console's "Didn't receive transform" tripwire (:1410).
// run_fxcrashmod_owned_traffic.py extracts the body verbatim and checks PostPhysicsUpdate's gated
// call site (0x827D3CA8..0x827D3CC0).
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/World/CrashModule/BrnCrashModule.h"
#include "rw/math/vpu/vector3_operation.h"
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleIO.h"
#undef protected
#undef private
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/System/PC/BrnNetHarnessPC.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

static unsigned assertions = 0;
static const char* lastAssertion = "";
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* message, const char*, int) { ++assertions; lastAssertion = message; std::fprintf(stderr, "ASSERT: %s\n", message); return 0; }
void* EndAssert() { return nullptr; }
} namespace Log { DebugPrint* gpDebugPrint = nullptr; void WriteToLog(const char*) {} }
namespace Message { u64 gxMessageFilterFlags = 0; } }

// The [nettraf] crash-own witness (b5 ca4ac341, a PC harness line, not console code). The real
// declaration is included above, so this silent definition must keep its signature.
namespace BrnNetHarnessPC
{
    void WitnessTag(const char*, const char*, const char*, ...) {}
}

using namespace BrnWorld;

struct CrashFixture
{
    static constexpr u32 KU_INVALID_CRASH = 0xffffffffu;
    decltype(CrashModule::mTrafficCrashes)        mTrafficCrashes;
    decltype(CrashModule::mCrashingTraffic)       mCrashingTraffic;
    decltype(CrashModule::mCrashingNetworkTraffic) mCrashingNetworkTraffic;
    decltype(CrashModule::mRecycledTrafficQueue)  mRecycledTrafficQueue;
    EActiveRaceCarIndex meLocalActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(2);
    bool mbIsOnlineGameMode   = true;
    bool mbIsShowtimeGameMode = false;
    CrashFixture()
    {
        mTrafficCrashes.Clear(); mCrashingTraffic.UnSetAll(); mCrashingNetworkTraffic.UnSetAll();
        mRecycledTrafficQueue.Construct();
    }
    void GenerateOwnedTrafficUpdates(const CrashIO::InputBuffer_PostPhysics*, CrashIO::OutputBuffer_PostPhysics*);
    bool WillTrafficVehicleBeRecycledNextFrame(u16);
};
#include "fxcrashmod_owned_traffic_methods.inc"

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

    auto input  = std::make_unique<CrashIO::InputBuffer_PostPhysics>();
    auto output = std::make_unique<CrashIO::OutputBuffer_PostPhysics>();
    auto& states = input->mVehicleOutputInterface.mTrafficStateQueue;
    input->mxStatusFlags.SetBit(CgsModule::IOBuffer::eStatusLockedForRead);

    auto Traffic = [](u32 luVehicle) { return 0x02000000u | (luVehicle << 10); };
    auto AddState = [&](u32 luVehicle, f32 lfSeed)
    {
        BrnPhysics::Vehicle::PhysicalTrafficState lState;
        std::memset(&lState, 0, sizeof(lState));
        lState.mTransform = Transform(lfSeed);
        lState.mEntityID.muValue = Traffic(luVehicle);
        // Reserve-and-fill (the producer VehicleOutputInterface::AddTrafficState does the same);
        // WheelLite::operator= has no body in the tree, so no element assignment here.
        std::memcpy(&states.AddEvent(), &lState, sizeof(lState));
    };

    CrashFixture m;
    auto AddCrash = [&](s32 liOwner, u16 luVehicle)
    {
        TrafficCrash lCrash{};
        lCrash.Construct(liOwner, luVehicle, 4.0f, false);
        m.mTrafficCrashes.Append(lCrash);
        m.mCrashingTraffic.SetBit(luVehicle);
    };
    auto Run = [&]()
    {
        output->Construct();
        output->mxStatusFlags.SetBit(CgsModule::IOBuffer::eStatusLockedForWrite);
        m.GenerateOwnedTrafficUpdates(input.get(), output.get());
        return output->mNetworkOutputInterface.GetCrashingTrafficUpdateQueue();
    };

    // Local player 2 owns 10, 11 and 13; player 3 owns 12. States arrive for 10, 12, 13 and a
    // non-crashing 20; 11 has no state but is about to be recycled (no tripwire).
    AddCrash(2, 10); AddCrash(2, 11); AddCrash(3, 12); AddCrash(2, 13);
    states.Construct();
    AddState(20, 7.0f); AddState(13, 3.0f); AddState(10, 1.0f); AddState(12, 2.0f);
    BrnPhysics::Vehicle::TrafficRemovedEvent lRemoved{};
    lRemoved.mRemovedVehicleEntityId.muValue = Traffic(11);
    m.mRecycledTrafficQueue.AddEvent(lRemoved);

    const auto* q = Run();
    Check(q->GetLength() == 2, "two owned wrecks with a state are published");
    Check(q->GetLength() == 2 && q->GetEvent(0).muVehicleId == 10 && q->GetEvent(1).muVehicleId == 13,
          "published in mTrafficCrashes order (10 then 13), not state order");
    Check(q->GetLength() == 2 && std::memcmp(&q->GetEvent(0).mTransform, &states.GetEvent(2).mTransform, sizeof(Matrix44Affine)) == 0,
          "vehicle 10 carries its PhysicalTrafficState::mTransform");
    Check(q->GetLength() == 2 && std::memcmp(&q->GetEvent(1).mTransform, &states.GetEvent(1).mTransform, sizeof(Matrix44Affine)) == 0,
          "vehicle 13 carries its PhysicalTrafficState::mTransform");
    Check(assertions == 0, "a recycled wreck without a state raises no tripwire");

    // Showtime: nothing at all (0x827C5480).
    m.mbIsShowtimeGameMode = true;
    q = Run();
    Check(q->GetLength() == 0, "no owned-traffic updates in a Showtime mode");
    m.mbIsShowtimeGameMode = false;

    // A crashing vehicle with no state, not network, not recycled: the :1410 tripwire.
    AddCrash(2, 14);
    const unsigned luBefore = assertions;
    q = Run();
    Check(assertions == luBefore + 1 &&
          std::strcmp(lastAssertion, "Didn't receive transform for crashing traffic vehicle") == 0,
          "a crashing wreck with no transform trips the console's :1410 tripwire");
    Check(q->GetLength() == 2, "the wreck without a transform is not published");

    std::printf("FxCrashmodOwnedTraffic: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
