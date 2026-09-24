// FX-FLOW (crash parity 2026-09-24, G13-X5 remainder): the GUI side of the free-burn rival
// shutdown, extracted by run_fxflow_rival_shutdown_gui.py:
//   * GuiCache::RecEvent's PRODUCTION arms for GUI 373 / 374 (BrnGuiCache.cpp), run inside a
//     fixture switch over the two members they write;
//   * BrnGameModule::TranslateGameActionsToGuiEvents' PRODUCTION action-120 arm and its TU-local
//     wire record (GameBridgeGameStateToX_StuntGuiEvents.cpp), run against the real ShutdownAction
//     (BrnGameActions.h) with a recording PushGuiEvent.
//
// Checked against the ARTIST image:
//   GuiCache::RecEvent @0x8250DDF0, jump table `id - 0xF5` (@0x8250F320):
//     373  0x8250FFA8  ld r11, 0(r30) ; stdx r11, r31, 0x9FF0      mShutdownCarID = payload u64
//     374  0x8250FFC4  li r11, 1 ; stb r11, 0x4B75(r31)            mbCarUnlockPending = 1
//   TranslateGameActionsToGuiEvents @0x823E9CE0 case 120 (0x823ED88C..0x823ED8A0):
//     ld r11, 0(r31) ; std r11, var_35D8 ; bl AddGuiEvent<GuiShutdownEvent> (0x823D8990: 373, 8)
#include "types.hpp"
#include "BrnCommonTypes.h"                                  // CgsID
#include "GameSource/GameState/BrnGameActions.h"             // the real ShutdownAction / E_ACTION_SHUTDOWN
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstdlib>
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
    DebugPrint* gpDebugPrint = nullptr;   // the [td-gui] witness stays silent
    StrStreamBase& DebugPrint::operator<<(const char*) { return *this; }
}
namespace Message { u64 gxMessageFilterFlags = 0; }
}

// ---- the GuiCache arms ------------------------------------------------------------------------
struct CacheFixture
{
    CgsID mShutdownCarID     = 0;
    bool  mbCarUnlockPending = false;

    void RecEvent(const CgsModule::Event* lpEvent, s32 liEventId)
    {
        switch (liEventId)
        {
#include "cache_arms.inc"
        default:
            break;
        }
    }
};

// ---- the translator arm -------------------------------------------------------------------------
struct PostedEvent
{
    s32 miType;
    s32 miSize;
    u8  maBytes[16];
};

struct RecordingInput
{
    PostedEvent maPosted[8];
    s32         miCount = 0;
};

template <class GuiEventT>
static void PushGuiEvent(const GuiEventT& lrEvent, RecordingInput* lpGuiInput)
{
    PostedEvent& lrPosted = lpGuiInput->maPosted[lpGuiInput->miCount++];
    lrPosted.miType = lrEvent.GetEventType();
    lrPosted.miSize = static_cast<s32>(sizeof(GuiEventT));
    std::memset(lrPosted.maBytes, 0xCD, sizeof(lrPosted.maBytes));
    std::memcpy(lrPosted.maBytes, &lrEvent, sizeof(GuiEventT));
}

namespace
{
#include "wire373.inc"
}

static void TranslateOne(s32 liActionType, const CgsModule::Event* lpAction, RecordingInput* lpGuiInput)
{
    switch (liActionType)
    {
#include "translator_arm.inc"
    default:
        break;
    }
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
    namespace GsmIO = BrnGameState::GameStateModuleIO;

    // ---- GuiCache ------------------------------------------------------------------------------
    {
        CacheFixture lCache;
        alignas(16) CgsID lId = 0x0123456789ABCDEFull;
        lCache.RecEvent(reinterpret_cast<const CgsModule::Event*>(&lId), 373);
        Check(lCache.mShutdownCarID == 0x0123456789ABCDEFull && !lCache.mbCarUnlockPending,
              "GuiCache 373: mShutdownCarID = the payload's u64 (stdx 0x9FF0 @0x8250FFB4), nothing else");

        alignas(16) u8 lu8Zero = 0;
        lCache.RecEvent(reinterpret_cast<const CgsModule::Event*>(&lu8Zero), 374);
        Check(lCache.mbCarUnlockPending && lCache.mShutdownCarID == 0x0123456789ABCDEFull,
              "GuiCache 374: mbCarUnlockPending = 1 without reading the byte (stb 0x4B75 @0x8250FFC8)");
    }

    // ---- the translator's action-120 arm ---------------------------------------------------------
    {
        GsmIO::ShutdownAction lAction;
        std::memset(&lAction, 0xA5, sizeof(lAction));
        lAction.mVictimCarID  = 0x1111222233334444ull;
        lAction.mRivalID      = 0x5555666677778888ull;
        lAction.meVictimIndex = static_cast<decltype(lAction.meVictimIndex)>(3);
        RecordingInput lInput;
        TranslateOne(GsmIO::E_ACTION_SHUTDOWN, reinterpret_cast<const CgsModule::Event*>(&lAction), &lInput);

        CgsID lPosted = 0;
        if (lInput.miCount == 1)
            std::memcpy(&lPosted, lInput.maPosted[0].maBytes, sizeof(lPosted));
        Check(lInput.miCount == 1 && lInput.maPosted[0].miType == 373 && lInput.maPosted[0].miSize == 8,
              "action 120 posts ONE GUI 373 of 8 bytes (AddGuiEvent<GuiShutdownEvent> @0x823D8990)");
        Check(lPosted == 0x1111222233334444ull,
              "the 373 payload is the record's +0 victim car id (`ld r11, 0(r31)` @0x823ED88C)");

        RecordingInput lOther;
        TranslateOne(121, reinterpret_cast<const CgsModule::Event*>(&lAction), &lOther);
        Check(lOther.miCount == 0, "the 120 arm does not answer action 121 (its sibling arm is separate)");
    }

    Check(gAsserts == 0, "no assert on the way");

    std::printf("FxFlowRivalShutdownGui: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
