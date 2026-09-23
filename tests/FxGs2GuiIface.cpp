// FX-GS2 (crash parity 2026-09-23, G10-D11 part 2): the PRODUCTION TranslateGuiInterfaceToGuiEvents
// (with its two TU-local wire records, extracted from src/GameSource/Game/BrnGameModule.cpp), the
// PRODUCTION PushGuiEvent (src/GameSource/Game/GameBridgeGameStateToX.h) and the PRODUCTION
// GameStateToGuiInterface Construct / publishers / const queue accessors
// (src/GameSource/GameState/SharedIO/BrnGameStateToGuiIOInterfaces.cpp), by run_fxgs2_gui_iface.py.
// The interface and the GUI payload types are the real ones; the GUI input buffer is a fixture whose
// GetGuiEvents()->AddEvent(event, id, size) records what is posted.
//
// Checked against the ARTIST asm of TranslateGuiInterfaceToGuiEvents @0x823E1D90:
//   eight loops in the order new (+4) / triggered (+0x40) / ending (+0x7C) / overtake (+0xC8) /
//   finish (+0xF4) / took lead (+0x120) / took last (+0x140) / on tail (+0x160), each posting one
//   GUI event per record through AddGuiEvent<T>, whose (id, size) literals are
//   177/12 @0x823D9C24, 179/12 @0x823D9CDC, 181/16 @0x823D9D94, 371/8 @0x823D9E4C,
//   372/8 @0x823D9F04, 484/16 @0x823D8D0C, 485/16 @0x823D9FBC, 486/16 @0x823DA074.
//   Record -> payload stores:
//     177/179  words +0/+4/+8 -> +0/+4/+8 (aggressor, victim, trick type)
//     181      words +0/+4/+8 and the byte +0xC (survived) -> same             (0x823E1F78..94)
//     371      rec +0 (u8 new position) -> +4 (`stb`); rec +4 (slot) -> +0     (0x823E2038..4C)
//     372      rec +0 (finish type) -> +4; rec +4 (slot) -> +0                 (0x823E20F0..104)
//     484/485/486  rec +0 (u64 car id) -> +0; rec +8 (slot) -> +8             (0x823E21B0..CC)
//   The interface is const: no queue is drained or cleared by the translate.
#include "types.hpp"
#include "BrnCommonTypes.h"                                                 // CgsID
#include "GameSource/BurnoutConstants.h"                                    // ::EActiveRaceCarIndex
#include "GameSource/GameState/SharedIO/BrnGameStateToGuiIOInterfaces.h"    // the real GameStateToGuiInterface
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                             // the real GuiDirtyTrick*/TookLead/Last/OnTail payloads
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                 // the [gui-iface] witness (silent here)
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
namespace Log { DebugPrint* gpDebugPrint = nullptr; }   // the [gui-iface] witness stays silent
}

// ---- the GUI input-buffer fixture: GetGuiEvents()->AddEvent(event, id, size) records ------------
namespace CgsGui
{
namespace CgsGuiModuleIO
{
    struct PostedEvent
    {
        s32 miType;
        s32 miSize;
        u8  maBytes[64];
    };

    struct RecordingQueue
    {
        PostedEvent maPosted[64];
        s32         miCount = 0;

        bool AddEvent(const CgsModule::Event* lpEvent, s32 liType, s32 liSize)
        {
            if (miCount >= 64 || liSize < 0 || liSize > 64)
            {
                return false;
            }
            PostedEvent& lrPosted = maPosted[miCount++];
            lrPosted.miType = liType;
            lrPosted.miSize = liSize;
            std::memset(lrPosted.maBytes, 0xCD, sizeof(lrPosted.maBytes));
            std::memcpy(lrPosted.maBytes, lpEvent, static_cast<size_t>(liSize));
            return true;
        }
    };

    struct InputBuffer
    {
        RecordingQueue  mQueue;
        RecordingQueue* GetGuiEvents() { return &mQueue; }
    };
}
}

// The production bodies under test (or the runner's labelled empty stand-in).
#include "gui_iface_methods.inc"

using BrnGameState::GameStateModuleIO::GameStateToGuiInterface;
using CgsGui::CgsGuiModuleIO::InputBuffer;
using CgsGui::CgsGuiModuleIO::PostedEvent;

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

static s32 ReadS32(const PostedEvent& lrEvent, u32 luOffset)
{
    s32 liValue;
    std::memcpy(&liValue, lrEvent.maBytes + luOffset, sizeof(liValue));
    return liValue;
}

static u64 ReadU64(const PostedEvent& lrEvent, u32 luOffset)
{
    u64 luValue;
    std::memcpy(&luValue, lrEvent.maBytes + luOffset, sizeof(luValue));
    return luValue;
}

static bool Is(const PostedEvent& lrEvent, s32 liType, s32 liSize)
{
    return lrEvent.miType == liType && lrEvent.miSize == liSize;
}

static bool Triple(const PostedEvent& lrEvent, s32 liAggressor, s32 liVictim, s32 liType)
{
    return ReadS32(lrEvent, 0) == liAggressor && ReadS32(lrEvent, 4) == liVictim && ReadS32(lrEvent, 8) == liType;
}

static GameStateToGuiInterface gInterface;   // static storage: the queues' inline arrays live here
static InputBuffer             gInput;

int main()
{
    // (1) a freshly Constructed interface posts nothing
    gInterface.Construct();
    BrnGame::TranslateGuiInterfaceToGuiEvents(&gInput, &gInterface);
    Check(gInput.mQueue.miCount == 0, "empty interface: no GUI event posted");

    // (2) every queue carries records: two new tricks, one triggered, two ending (survived, not),
    // one overtake, one finish, one took-lead, one took-last, two on-tail
    gInterface.AddNewDirtyTrick(static_cast<::EActiveRaceCarIndex>(1), static_cast<::EActiveRaceCarIndex>(2),
                                static_cast<BrnNetwork::EPaybackType>(0));
    gInterface.AddNewDirtyTrick(static_cast<::EActiveRaceCarIndex>(3), static_cast<::EActiveRaceCarIndex>(0),
                                static_cast<BrnNetwork::EPaybackType>(2));
    gInterface.AddDirtyTrickTriggered(static_cast<::EActiveRaceCarIndex>(4), static_cast<::EActiveRaceCarIndex>(5),
                                      static_cast<BrnNetwork::EPaybackType>(1));
    gInterface.AddDirtyTrickEnding(static_cast<::EActiveRaceCarIndex>(6), static_cast<::EActiveRaceCarIndex>(7),
                                   static_cast<BrnNetwork::EPaybackType>(2), true);
    gInterface.AddDirtyTrickEnding(static_cast<::EActiveRaceCarIndex>(2), static_cast<::EActiveRaceCarIndex>(1),
                                   static_cast<BrnNetwork::EPaybackType>(0), false);
    {
        // AddOvertakeEvent has no body in the tree: append the record to the queue directly
        BrnGameState::GameStateToGuiOvertakeEvent lOvertake;
        lOvertake.mu8NewPosition       = 3;
        lOvertake.meActiveRaceCarIndex = static_cast<::EActiveRaceCarIndex>(5);
        gInterface.mOvertakeEventQueue.AddEvent(lOvertake);
    }
    gInterface.AddFinishedRaceEvent(static_cast<BrnGui::EFinishType>(2), static_cast<::EActiveRaceCarIndex>(6));
    {
        // AddTookLeadEvent / AddTookLastEvent have no bodies in the tree: append directly
        BrnGameState::GameStateToGuiTookLeadEvent lLead;
        lLead.mOfflineRivalCarID   = 0x0123456789ABCDEFull;
        lLead.meActiveRaceCarIndex = static_cast<::EActiveRaceCarIndex>(4);
        gInterface.mTookLeadEventQueue.AddEvent(lLead);
        BrnGameState::GameStateToGuiTookLastEvent lLast;
        lLast.mOfflineRivalCarID   = 0x1111222233334444ull;
        lLast.meActiveRaceCarIndex = static_cast<::EActiveRaceCarIndex>(7);
        gInterface.mTookLastEventQueue.AddEvent(lLast);
    }
    gInterface.AddOnTailEvent(0x00000000000A5A5Aull, static_cast<::EActiveRaceCarIndex>(2));
    gInterface.AddOnTailEvent(0x7FFF000000000001ull, static_cast<::EActiveRaceCarIndex>(6));

    gInput.mQueue.miCount = 0;
    BrnGame::TranslateGuiInterfaceToGuiEvents(&gInput, &gInterface);
    const PostedEvent* lp = gInput.mQueue.maPosted;
    const bool lbCount = (gInput.mQueue.miCount == 11);
    Check(lbCount, "eleven records -> eleven GUI events");
    if (!lbCount)
    {
        std::printf("       posted %d\n", gInput.mQueue.miCount);
        for (s32 i = 11; i > gInput.mQueue.miCount; --i)
        {
            gInput.mQueue.maPosted[i - 1].miType = -1;   // compare against nothing, not stale data
            gInput.mQueue.maPosted[i - 1].miSize = -1;
        }
    }

    const s32 kaTypes[11] = { 177, 177, 179, 181, 181, 371, 372, 484, 485, 486, 486 };
    const s32 kaSizes[11] = { 12, 12, 12, 16, 16, 8, 8, 16, 16, 16, 16 };
    bool lbOrder = true, lbSizes = true;
    for (s32 i = 0; i < 11; ++i)
    {
        lbOrder = lbOrder && (lp[i].miType == kaTypes[i]);
        lbSizes = lbSizes && (lp[i].miSize == kaSizes[i]);
    }
    Check(lbOrder, "loop order new/triggered/ending/overtake/finish/lead/last/on-tail (ids 177..486)");
    Check(lbSizes, "payload sizes are the AddGuiEvent literals 12/12/12/16/16/8/8/16/16/16/16");

    Check(Is(lp[0], 177, 12) && Triple(lp[0], 1, 2, 0), "177 #0: aggressor/victim/type words copied in place");
    Check(Is(lp[1], 177, 12) && Triple(lp[1], 3, 0, 2), "177 #1: second record, same copy");
    Check(Is(lp[2], 179, 12) && Triple(lp[2], 4, 5, 1), "179: the triggered record's three words");
    Check(Is(lp[3], 181, 16) && Triple(lp[3], 6, 7, 2) && lp[3].maBytes[12] == 1,
          "181 #0: three words + survived byte +0xC == 1");
    Check(Is(lp[4], 181, 16) && Triple(lp[4], 2, 1, 0) && lp[4].maBytes[12] == 0,
          "181 #1: survived byte +0xC == 0");
    Check(Is(lp[5], 371, 8) && ReadS32(lp[5], 0) == 5, "371: the slot (record +4) lands at payload +0");
    Check(Is(lp[5], 371, 8) && lp[5].maBytes[4] == 3, "371: the new position byte (record +0) lands at payload +4");
    Check(Is(lp[6], 372, 8) && ReadS32(lp[6], 0) == 6, "372: the slot (record +4) lands at payload +0");
    Check(Is(lp[6], 372, 8) && ReadS32(lp[6], 4) == 2, "372: the finish type (record +0) lands at payload +4");
    Check(Is(lp[7], 484, 16) && ReadU64(lp[7], 0) == 0x0123456789ABCDEFull && ReadS32(lp[7], 8) == 4,
          "484: car id at +0, slot at +8");
    Check(Is(lp[8], 485, 16) && ReadU64(lp[8], 0) == 0x1111222233334444ull && ReadS32(lp[8], 8) == 7,
          "485: car id at +0, slot at +8");
    Check(Is(lp[9], 486, 16) && ReadU64(lp[9], 0) == 0x00000000000A5A5Aull && ReadS32(lp[9], 8) == 2,
          "486 #0: rival car id at +0, on-tail slot at +8");
    Check(Is(lp[10], 486, 16) && ReadU64(lp[10], 0) == 0x7FFF000000000001ull && ReadS32(lp[10], 8) == 6,
          "486 #1: the second on-tail record");

    // (3) the interface is const: nothing drained
    Check(gInterface.mNewDirtyTrickQueue.GetLength() == 2 && gInterface.mDirtyTrickTriggeredQueue.GetLength() == 1
              && gInterface.mDirtyTrickEndingQueue.GetLength() == 2 && gInterface.mOvertakeEventQueue.GetLength() == 1
              && gInterface.mFinishedRaceEventQueue.GetLength() == 1 && gInterface.mTookLeadEventQueue.GetLength() == 1
              && gInterface.mTookLastEventQueue.GetLength() == 1 && gInterface.mOnTailEventQueue.GetLength() == 2,
          "the translate leaves every GameState queue as it found it");

    // (4) the retire is the interface's own Construct: after it, the leg posts nothing
    gInterface.Construct();
    gInput.mQueue.miCount = 0;
    BrnGame::TranslateGuiInterfaceToGuiEvents(&gInput, &gInterface);
    Check(gInput.mQueue.miCount == 0, "after Construct (the per-sub-step retire) nothing is re-posted");

    // (5) only the on-tail queue populated: exactly one 486, nothing else
    gInterface.AddOnTailEvent(0x0000000000000042ull, static_cast<::EActiveRaceCarIndex>(3));
    gInput.mQueue.miCount = 0;
    BrnGame::TranslateGuiInterfaceToGuiEvents(&gInput, &gInterface);
    Check(gInput.mQueue.miCount == 1 && Is(gInput.mQueue.maPosted[0], 486, 16)
              && ReadU64(gInput.mQueue.maPosted[0], 0) == 0x42ull && ReadS32(gInput.mQueue.maPosted[0], 8) == 3,
          "a lone on-tail record posts exactly one 486 {id, slot}");

    Check(gAsserts == 0, "no assert fired");

    std::printf("FxGs2GuiIface: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
