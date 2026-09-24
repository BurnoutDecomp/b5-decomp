// FX-FLOW (crash parity 2026-09-24, NEW-FINISHLINE): the PRODUCTION MainDirector::ProcessInputQueue
// case-24 arm (E_ACTION_BROADCAST_MODE_FINISH_LINES), extracted from
// src/GameSource/Director/BrnMainDirector.cpp by run_fxflow_finish_line.py, fed the CONSOLE's 48-byte
// wire record (built here byte by byte, not through the header's record type), against the real
// BrnDirector::GameState -- and then the PRODUCTION DirectorResourceManager::GetEventCompletionShots,
// the reader ArbStatePostEvent::Prepare hands the stored id to (`ld r5,0x160` @0x8226E268).
//
// Checked against the ARTIST asm:
//   0x8223873C  ld   r11,0x28(r30)     the record's u64 at +0x28 ...
//   0x8223874C  stdx r11,r31,0x33940   ... -> GameState +0x160 mFinishLineID (all 64 bits)
//   0x82238750  BoxRegion::ComputeDirection(r4 = the record)  (the BoxRegion is the record's +0x00)
//   0x82238760  stvx v0,r31,0x33950    -> GameState +0x170 mFinishLineNorthmostDir
//   ComputeDirection @0x821F2CA8 is the "at" row of RotY * RotX: (sin ry, -sin rx cos ry, cos rx cos ry)
//   GetEventCompletionShots @0x821F6BB8 case 0: 0x880E9 -> +0x348 (North) ... 0x880EA -> +0x3B8
//   (NorthWest); a miss asserts "Unknown finish line" and returns the plain race group +0x318.
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"
#include "GameSource/Director/BrnDirectorResourceManager.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Log
{
    DebugPrint* gpDebugPrint = nullptr;   // the arm's BRN_FINISHLINE_DIAG witness stays silent
    StrStreamBase& DebugPrint::operator<<(const char*) { return *this; }
}
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcText, const char*, int) { ++gAsserts; std::fprintf(stderr, "assert: %s\n", lpcText); return 0; }
    void* EndAssert() { return nullptr; }
}
}

// Stands in for BrnDirector::MainDirector: the arm reads only maGameState and the record pointer.
struct FinishLineDirector
{
    BrnDirector::GameState maGameState;
    void Drain(s32 liActionType, const u8* lpacPayload);
};

// The production bodies under test (or the runner's labelled empty stand-in for the arm).
#include "finish_line.inc"

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

static bool Near(f32 lfA, f32 lfB) { return std::fabs(lfA - lfB) <= 1.0e-5f; }

// The console wire format of action 24 (PrepareForMode @0x82342F3C..0x82342F70): nine f32 of the
// finish landmark's BoxRegion (position, rotation in radians, dimensions) at +0x00..+0x23, then the
// landmark id as a u64 at +0x28. 48 bytes.
struct WireRecord
{
    alignas(16) u8 mau8Bytes[48];
};

static WireRecord Record(const f32 lafBox[9], u64 luFinishLineID)
{
    WireRecord lRecord;
    std::memset(lRecord.mau8Bytes, 0xCD, sizeof(lRecord.mau8Bytes));   // the 4 pad bytes are junk
    std::memcpy(lRecord.mau8Bytes + 0x00, lafBox, 9 * sizeof(f32));
    std::memcpy(lRecord.mau8Bytes + 0x28, &luFinishLineID, sizeof(u64));
    return lRecord;
}

alignas(16) static unsigned char gaResourceManagerStorage[sizeof(BrnDirector::DirectorResourceManager)];

int main()
{
    static FinishLineDirector lDirector;   // zero-initialised: mFinishLineID 0 == GameState::Clear's
    BrnDirector::GameState& lrGameState = lDirector.maGameState;
    const BrnDirector::DirectorResourceManager& lrResources =
        *reinterpret_cast<const BrnDirector::DirectorResourceManager*>(gaResourceManagerStorage);

    // Sentinels on neighbours the arm must not touch.
    lrGameState.meEventType            = 0;       // E_MODE_OFFLINE_RACE
    lrGameState.mbDriveThruActive      = true;
    lrGameState.meBeatenRivalRaceCarIndex = static_cast<EActiveRaceCarIndex>(3);
    lrGameState.meJunkyardState        = BrnDirector::GameState::E_JY_CAR_SELECT;

    // 0. The state every PC race finish had: the id is Clear's 0 -> the race ladder misses.
    {
        const unsigned luAssertsBefore = gAsserts;
        const Attrib::Gen::shotgroup& lrGroup = lrResources.GetEventCompletionShots(0, static_cast<s64>(lrGameState.mFinishLineID));
        Check(&lrGroup == &lrResources.mRaceFinishGroup && gAsserts == luAssertsBefore + 1,
              "reader sanity: id 0 in a race misses the ladder, asserts once, returns the race group (+0x318)");
    }

    // 1. North finish line (0x880E9), a tilted, yawed box.
    {
        const f32 lafBox[9] = { 100.0f, 5.0f, -200.0f,   0.3f, 1.1f, 0.7f,   10.0f, 4.0f, 2.0f };
        const WireRecord lRecord = Record(lafBox, 557289u);
        lDirector.Drain(24, lRecord.mau8Bytes);

        Check(lrGameState.mFinishLineID == 557289u, "0x8223874C: mFinishLineID = the record's +0x28 (North 0x880E9)");
        Check(Near(lrGameState.mFinishLineNorthmostDir.x, std::sin(1.1f)),
              "0x82238760: dir.x = sin(rotY) (ComputeDirection of the record's BoxRegion)");
        Check(Near(lrGameState.mFinishLineNorthmostDir.y, -std::sin(0.3f) * std::cos(1.1f)),
              "0x82238760: dir.y = -sin(rotX) cos(rotY)");
        Check(Near(lrGameState.mFinishLineNorthmostDir.z, std::cos(0.3f) * std::cos(1.1f)),
              "0x82238760: dir.z = cos(rotX) cos(rotY) (rotZ is not in the direction)");

        const unsigned luAssertsBefore = gAsserts;
        const Attrib::Gen::shotgroup& lrGroup = lrResources.GetEventCompletionShots(0, static_cast<s64>(lrGameState.mFinishLineID));
        Check(&lrGroup == &lrResources.mRaceFinishNorth && gAsserts == luAssertsBefore,
              "ArbStatePostEvent::Prepare's lookup now finds the North finish group (+0x348), no assert");
    }

    // 2. A second broadcast (the next event) overwrites both fields: North-West (0x880EA).
    {
        const f32 lafBox[9] = { -50.0f, 0.0f, 75.0f,   -0.4f, -2.0f, 0.0f,   6.0f, 3.0f, 1.0f };
        const WireRecord lRecord = Record(lafBox, 557290u);
        lDirector.Drain(24, lRecord.mau8Bytes);

        Check(lrGameState.mFinishLineID == 557290u, "a later broadcast overwrites mFinishLineID (North-West 0x880EA)");
        Check(Near(lrGameState.mFinishLineNorthmostDir.x, std::sin(-2.0f)) &&
              Near(lrGameState.mFinishLineNorthmostDir.y, -std::sin(-0.4f) * std::cos(-2.0f)) &&
              Near(lrGameState.mFinishLineNorthmostDir.z, std::cos(-0.4f) * std::cos(-2.0f)),
              "a later broadcast overwrites mFinishLineNorthmostDir");

        const unsigned luAssertsBefore = gAsserts;
        const Attrib::Gen::shotgroup& lrGroup = lrResources.GetEventCompletionShots(0, static_cast<s64>(lrGameState.mFinishLineID));
        Check(&lrGroup == &lrResources.mRaceFinishNorthWest && gAsserts == luAssertsBefore,
              "the lookup follows it to the North-West finish group (+0x3B8), no assert");
    }

    // 3. The id is the record's full 64-bit word (`ld`, not `lwz`): a sign-extended 32-bit id.
    {
        const f32 lafBox[9] = { 0.0f, 0.0f, 0.0f,   0.0f, 0.0f, 0.0f,   1.0f, 1.0f, 1.0f };
        const u64 luSignExtended = static_cast<u64>(static_cast<s64>(-5));
        const WireRecord lRecord = Record(lafBox, luSignExtended);
        lDirector.Drain(24, lRecord.mau8Bytes);
        Check(lrGameState.mFinishLineID == luSignExtended, "all 64 bits of +0x28 are stored (ld/stdx)");
        Check(Near(lrGameState.mFinishLineNorthmostDir.x, 0.0f) && Near(lrGameState.mFinishLineNorthmostDir.y, 0.0f) &&
              Near(lrGameState.mFinishLineNorthmostDir.z, 1.0f),
              "an unrotated box faces +Z");
    }

    // 4. Neighbours untouched; another action id leaves the finish line alone.
    Check(lrGameState.meEventType == 0 && lrGameState.mbDriveThruActive &&
          lrGameState.meBeatenRivalRaceCarIndex == static_cast<EActiveRaceCarIndex>(3) &&
          lrGameState.meJunkyardState == BrnDirector::GameState::E_JY_CAR_SELECT,
          "the arm writes only mFinishLineID and mFinishLineNorthmostDir");
    {
        const f32 lafBox[9] = { 1.0f, 2.0f, 3.0f,   0.5f, 0.5f, 0.5f,   1.0f, 1.0f, 1.0f };
        const WireRecord lRecord = Record(lafBox, 557284u);
        const u64 luBefore = lrGameState.mFinishLineID;
        lDirector.Drain(25, lRecord.mau8Bytes);
        Check(lrGameState.mFinishLineID == luBefore, "action 25 does not write the finish line");
    }

    std::printf("FxFlowFinishLine: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
