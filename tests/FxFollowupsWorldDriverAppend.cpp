// FX-FOLLOWUPS (crash parity 2026-09-25, item 3): UpdateInputBuffer::AppendVehicleDriverInputInterface
// @0x823DB6C0, the world input buffer's merge of another vehicle-driver-input interface.
//
// The console body is the lock assert and then ONE call:
//   0x823DB6D4..0x823DB758  lbz/extrwi the write-lock bit -> "Not locked for writing" (:263)
//   0x823DB75C..0x823DB768  addis r3,r28,2 ; addi r3,r3,0x2BC0 ; bl 0x823DB640
// i.e. VehicleDriverInputInterface::Append @0x823DB640 on the +0x22BC0 member: the queue merge, the
// :164 "at most one side carries a target-assist list" assert, and the adoption of the source's list
// when the member has none (GetTargetAssistParams into its own arrays). The pre-fix PC body merged the
// update-driver queue only, so the target-assist half of the call was lost.
//
// run_fxfollowups_world_driver_append.py pastes the revision's production body into the fixture
// UpdateInputBuffer below (fxfu_wdappend.inc) and links the REAL BrnVehicleDriverInputInterface.cpp.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverInputInterface.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static std::vector<std::string> gaAsserts;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { gaAsserts.push_back(lpcMessage ? lpcMessage : ""); return 0; }
void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; void WriteToLog(const char*) {} }
namespace Message { u64 gxMessageFilterFlags = 0; }
}

namespace BrnWorldIO
{
    typedef BrnPhysics::Vehicle::VehicleDriverInputInterface VehicleDriverInputInterface;

    // The production UpdateInputBuffer reduced to what the body touches: the write-lock query and the
    // +142272 (0x22BC0) member.
    struct UpdateInputBuffer
    {
        bool mbLockedForWriting = true;
        bool IsBufferLockedForWriting() const { return mbLockedForWriting; }

        VehicleDriverInputInterface mVehicleDriverInputInterface;

        void AppendVehicleDriverInputInterface(const VehicleDriverInputInterface* lpInterface);
    };

#include "fxfu_wdappend.inc"
}

namespace
{
    int giChecks = 0;
    int giFailures = 0;

    void Check(bool lbPass, const char* lpcName, const std::string& lrDetail = std::string())
    {
        ++giChecks;
        if (!lbPass)
            ++giFailures;
        std::printf("%s  %s%s%s\n", lbPass ? "PASS" : "FAIL", lpcName, lrDetail.empty() ? "" : " -- ",
                    lrDetail.c_str());
    }

    const char* const KAC_ONE_LIST_ASSERT =
        "miTargetAssistCount==0 || lpInterfaceToAppend->GetTargetAssistCount()==0";

    int CountAsserts(const char* lpcText)
    {
        int liCount = 0;
        for (const std::string& lrMessage : gaAsserts)
            if (lrMessage.find(lpcText) != std::string::npos)
                ++liCount;
        return liCount;
    }

    Vector3 Position(f32 lf)
    {
        Vector3 lv;
        lv.x = lf; lv.y = lf + 1.0f; lv.z = lf + 2.0f; lv.w = 0.0f;
        return lv;
    }

    EntityId Id(u32 lu)
    {
        EntityId lId;
        lId.muValue = lu;
        return lId;
    }

    struct TestEvent : CgsModule::Event
    {
        u32 muPayload;
    };

    // Static storage: each interface carries a 5 KB queue.
    BrnWorldIO::UpdateInputBuffer                      gBuffer;
    BrnPhysics::Vehicle::VehicleDriverInputInterface   gSource;

    void Reset()
    {
        gaAsserts.clear();
        gBuffer.mbLockedForWriting = true;
        gBuffer.mVehicleDriverInputInterface.Construct();
        gSource.Construct();
    }
}

int main()
{
    // A. the member holds no list, the source holds two -> the member adopts them.
    Reset();
    gSource.AddTargetAssist(Position(10.0f), Id(101u));
    gSource.AddTargetAssist(Position(20.0f), Id(202u));
    gBuffer.AppendVehicleDriverInputInterface(&gSource);
    {
        Vector3  laPositions[8];
        EntityId laIds[8];
        s32      liCount = -1;
        gBuffer.mVehicleDriverInputInterface.GetTargetAssistParams(laPositions, laIds, &liCount);
        Check(liCount == 2, "A1 an empty member adopts the source's two target assists (0x823DB6B0)",
              "count " + std::to_string(liCount));
        Check(liCount == 2 && laIds[0].muValue == 101u && laIds[1].muValue == 202u,
              "A2 the adopted ids are the source's, in order");
        Check(liCount == 2 && laPositions[0].x == 10.0f && laPositions[0].z == 12.0f && laPositions[1].x == 20.0f
                  && laPositions[1].y == 21.0f,
              "A3 the adopted positions are the source's, in order");
        Check(gaAsserts.empty(), "A4 no assert when only the source carries a list",
              gaAsserts.empty() ? std::string() : gaAsserts.front());
    }

    // B. both sides carry a list -> the console's :164 assert; the member keeps its own list.
    Reset();
    gBuffer.mVehicleDriverInputInterface.AddTargetAssist(Position(1.0f), Id(7u));
    gSource.AddTargetAssist(Position(10.0f), Id(101u));
    gSource.AddTargetAssist(Position(20.0f), Id(202u));
    gBuffer.AppendVehicleDriverInputInterface(&gSource);
    {
        Check(CountAsserts(KAC_ONE_LIST_ASSERT) == 1,
              "B1 two lists fire the :164 one-list assert once (0x823DB674..0x823DB690)",
              std::to_string(CountAsserts(KAC_ONE_LIST_ASSERT)) + " fired");
        Vector3  laPositions[8];
        EntityId laIds[8];
        s32      liCount = -1;
        gBuffer.mVehicleDriverInputInterface.GetTargetAssistParams(laPositions, laIds, &liCount);
        Check(liCount == 1 && laIds[0].muValue == 7u, "B2 the member keeps its own list (no adoption when count != 0)",
              "count " + std::to_string(liCount));
    }

    // C. the member carries a list, the source none -> no assert, nothing changes.
    Reset();
    gBuffer.mVehicleDriverInputInterface.AddTargetAssist(Position(1.0f), Id(7u));
    gBuffer.AppendVehicleDriverInputInterface(&gSource);
    {
        Check(gaAsserts.empty(), "C1 no assert when only the member carries a list");
        s32 liCount = gBuffer.mVehicleDriverInputInterface.GetTargetAssistCount();
        Check(liCount == 1, "C2 the member's list is untouched", "count " + std::to_string(liCount));
    }

    // D. the update-driver queue is still merged.
    Reset();
    {
        TestEvent lEvent;
        lEvent.muPayload = 0xC0FFEEu;
        gSource.GetUpdateDriverQueue()->AddEvent(&lEvent, 3, static_cast<s32>(sizeof(TestEvent)));
        gSource.GetUpdateDriverQueue()->AddEvent(&lEvent, 4, static_cast<s32>(sizeof(TestEvent)));
        gBuffer.AppendVehicleDriverInputInterface(&gSource);
        const s32 liLength = gBuffer.mVehicleDriverInputInterface.GetUpdateDriverQueue()->GetLength();
        Check(liLength == 2, "D1 the source's two queued driver events are merged into the member",
              "length " + std::to_string(liLength));
    }

    // E. the write-lock assert.
    Reset();
    gBuffer.mbLockedForWriting = false;
    gBuffer.AppendVehicleDriverInputInterface(&gSource);
    Check(CountAsserts("Not locked for writing") == 1, "E1 an unlocked buffer fires \"Not locked for writing\" (:263)");

    std::printf("FxFollowupsWorldDriverAppend: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures == 0 ? 0 : 1;
}
