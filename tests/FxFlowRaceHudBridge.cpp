// FX-FLOW (crash parity 2026-09-24, G11-D1 remainder, consumer half): the race-mode HUD message arms of
// BrnGameModule::TranslateGameActionsToGuiEvents, extracted from GameBridgeGameStateToX_StuntGuiEvents.cpp
// by run_fxflow_race_hud_bridge.py and run inside a fixture switch with a recording PushGuiEvent.
//
// Checked against the ARTIST image (jpt_823EA1F0 of 0x823E9CE0):
//   242 @0x823ED9F4  `lwz 8` / `ld 0`                  -> AddGuiEvent<GuiTookLeadEvent>        484 / 16
//   245 @0x823EC034  `ld 0` / `lfs 8` / `lbz 0x10` / `lwz 0xC` -> AddGuiEvent<GuiInEventLeaderSplit> 420 / 24
//   246 @0x823EC068  an unwritten stack byte           -> AddGuiEvent<GuiInEventNeckAndNeck>   421 / 1
//   247 @0x823EC07C  `lwz 0` / `lwz 4`                 -> AddGuiEvent<GuiInEventFinisher>      423 / 8
//   248 @0x823EC0A0  `ld 8` / `ld 0` / `lwz 0x10`      -> AddGuiEvent<GuiInEventRivalProgress> 422 / 24
//   250 @0x823EC0CC  `ld 0` / `lwz 8`                  -> AddGuiEvent<GuiNetworkPlayerCrashingEvent> 482 / 16
//   243 / 244 / 249: the table's default list -- nothing posted.
// The action records are built as RAW BYTES at the X360 producers' offsets (HUDMessageLogic's stores),
// so a PC record whose layout drifted from the console's would post the wrong payload here.
#include "types.hpp"
#include "BrnCommonTypes.h"                                  // CgsID
#include "GameSource/GameState/BrnGameActions.h"             // the real HUDMessage*Action records
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"              // the real BrnGui GUI records
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
    int   FireAssert(const char* lpcText, const char*, int) { ++gAsserts; std::fprintf(stderr, "assert: %s\n", lpcText); return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Log
{
    DebugPrint* gpDebugPrint = nullptr;
    StrStreamBase& DebugPrint::operator<<(const char*) { return *this; }
}
namespace Message { u64 gxMessageFilterFlags = 0; }
}

struct PostedEvent
{
    s32 miType;
    s32 miSize;
    alignas(16) u8 maBytes[32];
};

struct RecordingInput
{
    PostedEvent maPosted[8];
    s32         miCount = 0;
};

template <class GuiEventT>
static void PushGuiEvent(const GuiEventT& lrEvent, RecordingInput* lpGuiInput)
{
    static_assert(sizeof(GuiEventT) <= 32, "fixture record buffer");
    PostedEvent& lrPosted = lpGuiInput->maPosted[lpGuiInput->miCount++];
    lrPosted.miType = lrEvent.GetEventType();
    lrPosted.miSize = static_cast<s32>(sizeof(GuiEventT));
    std::memset(lrPosted.maBytes, 0xCD, sizeof(lrPosted.maBytes));
    std::memcpy(lrPosted.maBytes, &lrEvent, sizeof(GuiEventT));
}

// The diag rung is not behaviour; the fixture keeps it silent.
static void RaceHudBridgeDiag(s32, s32, s32, f32) {}

namespace
{
#include "wire421.inc"
}

static void TranslateOne(s32 liActionType, const CgsModule::Event* lpAction, RecordingInput* lpGuiInput)
{
    switch (liActionType)
    {
#include "race_arms.inc"
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

// One raw action record, built at the console producer's offsets.
struct RawRecord
{
    alignas(16) u8 maBytes[32];
    RawRecord() { std::memset(maBytes, 0xA5, sizeof(maBytes)); }
    void Put64(s32 liOffset, u64 luValue) { std::memcpy(maBytes + liOffset, &luValue, 8); }
    void Put32(s32 liOffset, u32 luValue) { std::memcpy(maBytes + liOffset, &luValue, 4); }
    void PutF(s32 liOffset, f32 lfValue) { std::memcpy(maBytes + liOffset, &lfValue, 4); }
    void Put8(s32 liOffset, u8 luValue) { maBytes[liOffset] = luValue; }
    const CgsModule::Event* AsEvent() const { return reinterpret_cast<const CgsModule::Event*>(maBytes); }
};

static u64 Get64(const PostedEvent& lr, s32 liOffset) { u64 lu; std::memcpy(&lu, lr.maBytes + liOffset, 8); return lu; }
static u32 Get32(const PostedEvent& lr, s32 liOffset) { u32 lu; std::memcpy(&lu, lr.maBytes + liOffset, 4); return lu; }
static f32 GetF(const PostedEvent& lr, s32 liOffset) { f32 lf; std::memcpy(&lf, lr.maBytes + liOffset, 4); return lf; }

static bool One(const RecordingInput& lrIn, s32 liType, s32 liSize)
{
    return lrIn.miCount == 1 && lrIn.maPosted[0].miType == liType && lrIn.maPosted[0].miSize == liSize;
}

int main()
{
    // 242 -> 484: {CgsID mCarId @0, slot @8} -> GuiTookLeadEvent {mOfflineRivalCarID @0, slot @8}
    {
        RawRecord lRecord;
        lRecord.Put64(0, 0x1111222233334444ull);
        lRecord.Put32(8, 5);
        RecordingInput lIn;
        TranslateOne(242, lRecord.AsEvent(), &lIn);
        Check(One(lIn, 484, 16), "action 242 posts ONE GUI 484 of 16 bytes (AddGuiEvent<GuiTookLeadEvent> @0x823D8C70)");
        Check(lIn.miCount == 1 && Get64(lIn.maPosted[0], 0) == 0x1111222233334444ull && Get32(lIn.maPosted[0], 8) == 5,
              "484 payload: the leader's car id at +0 (`ld 0`), its slot at +8 (`lwz 8`) -- @0x823EDA14..0x823EDA2C");
    }
    // 245 -> 420: {CgsID @0, f32 split @8, slot @0xC, flag @0x10}
    {
        RawRecord lRecord;
        lRecord.Put64(0, 0x5555666677778888ull);
        lRecord.PutF(8, 3.25f);
        lRecord.Put32(0xC, 2);
        lRecord.Put8(0x10, 1);
        RecordingInput lIn;
        TranslateOne(245, lRecord.AsEvent(), &lIn);
        Check(One(lIn, 420, 24), "action 245 posts ONE GUI 420 of 24 bytes (AddGuiEvent<GuiInEventLeaderSplit> @0x823D55D0)");
        Check(lIn.miCount == 1 && Get64(lIn.maPosted[0], 0) == 0x5555666677778888ull && GetF(lIn.maPosted[0], 8) == 3.25f
                  && Get32(lIn.maPosted[0], 0xC) == 2 && lIn.maPosted[0].maBytes[0x10] == 1,
              "420 payload: id +0, split +8, leader slot +0xC, player-leads byte +0x10 (@0x823EC034..0x823EC05C)");
    }
    // 246 -> 421: one byte, the action is not read
    {
        RawRecord lRecord;
        RecordingInput lIn;
        TranslateOne(246, lRecord.AsEvent(), &lIn);
        Check(One(lIn, 421, 1), "action 246 posts ONE GUI 421 of 1 byte (AddGuiEvent<GuiInEventNeckAndNeck> @0x823D5688)");
    }
    // 247 -> 423: {slot @0, place @4}
    {
        RawRecord lRecord;
        lRecord.Put32(0, 6);
        lRecord.Put32(4, 3);
        RecordingInput lIn;
        TranslateOne(247, lRecord.AsEvent(), &lIn);
        Check(One(lIn, 423, 8) && Get32(lIn.maPosted[0], 0) == 6 && Get32(lIn.maPosted[0], 4) == 3,
              "action 247 posts ONE GUI 423 of 8 bytes {slot +0, place +4} (@0x823EC07C..0x823EC094)");
    }
    // 248 -> 422: {rival id @0, landmark id @8, slot @0x10}
    {
        RawRecord lRecord;
        lRecord.Put64(0, 0x9999AAAABBBBCCCCull);
        lRecord.Put64(8, 0x0102030405060708ull);
        lRecord.Put32(0x10, 7);
        RecordingInput lIn;
        TranslateOne(248, lRecord.AsEvent(), &lIn);
        Check(One(lIn, 422, 24), "action 248 posts ONE GUI 422 of 24 bytes (AddGuiEvent<GuiInEventRivalProgress> @0x823D57F8)");
        Check(lIn.miCount == 1 && Get64(lIn.maPosted[0], 0) == 0x9999AAAABBBBCCCCull
                  && Get64(lIn.maPosted[0], 8) == 0x0102030405060708ull && Get32(lIn.maPosted[0], 0x10) == 7,
              "422 payload: rival id +0, landmark id +8, slot +0x10 (@0x823EC0A0..0x823EC0C0)");
    }
    // 250 -> 482: {CgsID @0, slot @8}
    {
        RawRecord lRecord;
        lRecord.Put64(0, 0x0A0B0C0D0E0F1011ull);
        lRecord.Put32(8, 4);
        RecordingInput lIn;
        TranslateOne(250, lRecord.AsEvent(), &lIn);
        Check(One(lIn, 482, 16) && Get64(lIn.maPosted[0], 0) == 0x0A0B0C0D0E0F1011ull && Get32(lIn.maPosted[0], 8) == 4,
              "action 250 posts ONE GUI 482 of 16 bytes {id +0, slot +8} (@0x823EC0CC..0x823EC0E8)");
    }
    // 243 / 244 / 249: the console's default list
    {
        RawRecord lRecord;
        RecordingInput lIn;
        TranslateOne(243, lRecord.AsEvent(), &lIn);
        TranslateOne(244, lRecord.AsEvent(), &lIn);
        TranslateOne(249, lRecord.AsEvent(), &lIn);
        Check(lIn.miCount == 0, "actions 243 / 244 / 249 post nothing (jpt_823EA1F0 default list)");
    }

    Check(gAsserts == 0, "no assert on the way (the 242 arm's `lpTookLeadAction` holds)");

    std::printf("FxFlowRaceHudBridge: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
