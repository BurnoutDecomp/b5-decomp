// FX-SHOWTIME2 (crash parity 2026-09-24): the PRODUCTION case-116 arm of
// BrnPhysics::PhysicsModule::HandleGameActions @0x825A72F0, extracted from
// src/GameSource/Physics/BrnPhysicsModuleGameActions.cpp by run_fxshowtime2_physics_request.py and
// driven against the REAL BrnPhysics::Vehicle::VehicleManagerOutputInterface (its
// mTrafficTypeRequestQueue is the real EventQueue<u16,32>).
//
// Checked against the ARTIST asm, jump-table case 109 (`addi r11, r3, -7`) == action 116:
//     0x825A7844  mr   r3, r14          ; r14 == r5 on entry == the physics OutputBuffer
//     0x825A7848  bl   0x8259FFD8       ; OutputBuffer::GetVehicleManagerOutputInterface (write)
//     0x825A784C  mr   r4, r29          ; the action record itself
//     0x825A7850  addi r3, r3, 0x750    ; + mTrafficTypeRequestQueue
//     0x825A7854  bl   0x825A3148       ; EventQueue<u16,32>::AddEvent -- copies the record's u16
// i.e. every action 116 appends exactly its leading u16 (TrafficTypeRequestAction::
// muTrafficVehicleIndex, DWARF BrnGameActions.h:2324) to the request queue, in arrival order,
// and nothing else reaches the queue.
//
// The pre-fix arm was a one-shot "[s3-action] id 116 DEFERRED" print: the queue stayed empty.
#include "types.hpp"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"   // the real interface
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

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
    DebugPrint* gpDebugPrint = nullptr;   // the pre-fix arm printed through it when non-null
}
}

namespace BrnPhysics
{
    // Stand-in for PhysicsModuleIO::OutputBuffer: the ONE member the arm reaches, through the
    // console's write accessor (0x8259FFD8). Counts the accessor calls.
    struct OutputBufferFixture
    {
        Vehicle::VehicleManagerOutputInterface mVehicleManagerOutputInterface;
        s32 miAccessorCalls;

        Vehicle::VehicleManagerOutputInterface* GetVehicleManagerOutputInterface()
        {
            ++miAccessorCalls;
            return &mVehicleManagerOutputInterface;
        }
    };

    namespace
    {
        // KI_ACTION_FORWARD_TO_OUTPUT and KU_EV_LEADING_WORD, extracted from production.
#include "physics_request_constants.inc"
    }

    // The production switch reduced to the arm under test.
    void RunArm(s32 liAction, const u8* lpu8Payload, OutputBufferFixture* lpOutputBuffer)
    {
        switch (liAction)
        {
#include "physics_request_arm.inc"
        default:
            break;
        }
    }

    s32 ForwardActionId() { return KI_ACTION_FORWARD_TO_OUTPUT; }
}

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

int main()
{
    using namespace BrnPhysics;

    static OutputBufferFixture lBuffer;
    std::memset(&lBuffer, 0, sizeof(lBuffer));
    Vehicle::VehicleManagerOutputInterface::TrafficTypeRequestQueue& lrQueue =
        lBuffer.mVehicleManagerOutputInterface.mTrafficTypeRequestQueue;
    lrQueue.Construct();

    Check(ForwardActionId() == 116,
          "the arm's id is X360 116 (jump-table case 109 + 7; DWARF E_ACTION_TRAFFIC_TYPE_REQUEST 111 + 5)");

    // A 16-byte-aligned record whose first two bytes are the index and whose tail is garbage the
    // arm must not read (the console posts the record with size 2).
    alignas(16) u8 laRecord[16];
    std::memset(laRecord, 0xEE, sizeof(laRecord));
    const u16 luFirstIndex = 298;          // < 600 == KU_MAX_TOTAL_TRAFFIC
    std::memcpy(laRecord, &luFirstIndex, sizeof(luFirstIndex));

    RunArm(116, laRecord, &lBuffer);
    Check(lrQueue.GetLength() == 1, "action 116 appends ONE request to mTrafficTypeRequestQueue (+0x750)");
    Check(lrQueue.GetLength() >= 1 && lrQueue.GetEvent(0) == luFirstIndex,
          "the request is the record's leading u16 (TrafficTypeRequestAction::muTrafficVehicleIndex)");
    Check(lBuffer.miAccessorCalls == 1,
          "reached through OutputBuffer::GetVehicleManagerOutputInterface (bl 0x8259FFD8), once");

    const u16 luSecondIndex = 5;
    std::memcpy(laRecord, &luSecondIndex, sizeof(luSecondIndex));
    RunArm(116, laRecord, &lBuffer);
    Check(lrQueue.GetLength() == 2, "a second action 116 appends behind the first (arrival order)");
    Check(lrQueue.GetLength() >= 2 && lrQueue.GetEvent(0) == luFirstIndex && lrQueue.GetEvent(1) == luSecondIndex,
          "the queue holds 298 then 5");

    RunArm(115, laRecord, &lBuffer);
    RunArm(117, laRecord, &lBuffer);
    Check(lrQueue.GetLength() == 2, "ids 115 / 117 do not reach the request queue");

    Check(gAsserts == 0, "no assert fired");

    std::printf("FxShowtime2PhysicsRequest: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
