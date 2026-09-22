// FX-RCEM (crash-parity 2026-09-22): the free-burn rival SHUTDOWN arms of
// BrnGameModule::TranslateGameActionsToGuiEvents (ARTIST 0x823E9CE0, jpt_823EA1F0), extracted
// VERBATIM from GameBridgeGameStateToX_StuntGuiEvents.cpp (with the production PushGuiEvent from
// GameBridgeGameStateToX.h) by run_rcem_shutdown_gui_events.py, and posted into a recording queue.
//   121 @0x823ED8A8 -> AddGuiEvent<GuiShutdownFinishedEvent> @0x823D8A48 -> AddEvent(q, rec, 374, 1)
//   120 @0x823ED88C -> AddGuiEvent<GuiShutdownEvent>         @0x823D8990 -> AddEvent(q, rec, 373, 8)
// 120 is gated by the runner on BrnGui::OfflineRivalShutdown having state bodies (see there).
#include "types.hpp"
#include "GameSource/BurnoutConstants.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static unsigned guAssertions = 0;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { ++guAssertions; std::fprintf(stderr, "ASSERT: %s\n", lpcMessage); return 0; }
void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; void WriteToLog(const char*) {} }
namespace Message { u64 gxMessageFilterFlags = 0; }
}

namespace Fixture {
struct Post { s32 miId; s32 miSize; std::vector<u8> maBytes; };
struct GuiQueue {
    std::vector<Post> maPosts;
    void AddEvent(const CgsModule::Event* lpEvent, s32 liId, s32 liSize) {
        const u8* lpBytes = reinterpret_cast<const u8*>(lpEvent);
        maPosts.push_back({ liId, liSize, std::vector<u8>(lpBytes, lpBytes + liSize) });
    }
};
struct GuiInput {
    GuiQueue mQueue;
    GuiQueue* GetGuiEvents() { return &mQueue; }
};
#include "rcem_shutdown_gui_events.inc"
}

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

int main() {
    using namespace BrnGameState::GameStateModuleIO;

    // ---- 121 -> 374 -------------------------------------------------------------------------
    {
        Fixture::GuiInput lInput;
        ShutdownFinishedAction lAction; lAction.meActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_3;
        Fixture::Translate(E_ACTION_SHUTDOWN_FINISHED, reinterpret_cast<const CgsModule::Event*>(&lAction), &lInput);
        Check(lInput.mQueue.maPosts.size() == 1, "121: exactly one GUI event posted");
        Check(!lInput.mQueue.maPosts.empty() && lInput.mQueue.maPosts[0].miId == 374,
              "121: GUI id 374 (li r5, 0x176 @0x823D8AE8)");
        Check(!lInput.mQueue.maPosts.empty() && lInput.mQueue.maPosts[0].miSize == 1,
              "121: record size 1 (li r6, 1 @0x823D8AE4)");
    }

#if RCEM_HAS_120_ARM
    // ---- 120 -> 373 (only once the POST_RIVAL state has bodies; the runner enforces it) ------
    {
        Fixture::GuiInput lInput;
        ShutdownAction lAction; std::memset(&lAction, 0, sizeof(lAction));
        lAction.mVictimCarID = 0x1122334455667788ull;
        lAction.meVictimIndex = E_ACTIVE_RACE_CAR_INDEX_3;
        Fixture::Translate(E_ACTION_SHUTDOWN, reinterpret_cast<const CgsModule::Event*>(&lAction), &lInput);
        Check(lInput.mQueue.maPosts.size() == 1 && lInput.mQueue.maPosts[0].miId == 373 &&
              lInput.mQueue.maPosts[0].miSize == 8, "120: GUI id 373, size 8");
        u64 luCarId = 0;
        if (!lInput.mQueue.maPosts.empty() && lInput.mQueue.maPosts[0].maBytes.size() == 8)
            std::memcpy(&luCarId, lInput.mQueue.maPosts[0].maBytes.data(), 8);
        Check(luCarId == 0x1122334455667788ull, "120: record is the action's +0 car id (ld r11, 0(r31))");
    }
#endif

    // An action with no arm posts nothing.
    {
        Fixture::GuiInput lInput;
        u8 lau8Payload[24] = {};
        Fixture::Translate(E_ACTION_PLAYER_INVULNERABLE, reinterpret_cast<const CgsModule::Event*>(lau8Payload), &lInput);
        Check(lInput.mQueue.maPosts.empty(), "an action without a translator arm posts nothing");
    }

    Check(guAssertions == 0, "no assertions");
    std::printf("RcemShutdownGuiEvents: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
