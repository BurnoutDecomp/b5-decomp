// FX-GS2 (crash parity 2026-09-23, G10-D9): the PRODUCTION CrashModeScoring::DealWithRemovedTraffic
// (and GetRecentCrash, which DealWithHitTrafficCar gates on), extracted from
// src/GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoring.cpp by run_fxgs2_removed_traffic.py,
// against the real CrashModeScoring layout and the real BrnGui::GuiRemovedTrafficEvent (id 209).
//
// Checked against the ARTIST asm of DealWithRemovedTraffic @0x8232BF90:
//   0x8232BFA4  the removed-id count (+0x34) is read ONCE; the CgsArray.h:336 assert fires on -1
//               and a -1 count runs no pass (`cmpwi r25, 0 ; ble`)
//   0x8232BFF4  GetItem(i) ; lhz -> the removed index as a u16
//   0x8232BFF8  the recent-crash count (+0x7C+0x200) is RE-READ per removed id
//   0x8232C040  lhz RecentCrash+0 ; cmplw -> unsigned 16-bit match
//   0x8232C064  Erase(j) (order-preserving) and on to the next removed id: first match only
#include "GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoringRecentCrash.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
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
}

// The production bodies under test (or the runner's labelled empty stand-in).
#include "removed_traffic.inc"

using BrnGameState::CrashModeScoring;
typedef CrashModeScoring::RecentCrash RecentCrash;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

alignas(16) static unsigned char gaScorerStorage[sizeof(CrashModeScoring)];

// A scorer whose recent-crash set holds these traffic indices, in order (chain count = position,
// time = 10 * position, so the entries can be told apart after a shift).
static CrashModeScoring& Scorer(const u16* lpuIndices, s32 liCount)
{
    std::memset(gaScorerStorage, 0, sizeof(gaScorerStorage));
    CrashModeScoring& lr = *reinterpret_cast<CrashModeScoring*>(gaScorerStorage);
    lr.maRecentCrashes.Construct();
    for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
    {
        RecentCrash* lpCrash = lr.maRecentCrashes.AddNew();
        lpCrash->muTrafficCarIndex = lpuIndices[liIndex];
        lpCrash->muCrashChainCount = static_cast<u16>(liIndex);
        lpCrash->mfTimeOfCrash     = 10.0f * static_cast<f32>(liIndex);
    }
    return lr;
}

static BrnGui::GuiRemovedTrafficEvent Removed(const u16* lpuIndices, s32 liCount)
{
    BrnGui::GuiRemovedTrafficEvent lEvent;
    lEvent.mRemovedTrafficArray.Construct();
    for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
        *lEvent.mRemovedTrafficArray.AddNew() = lpuIndices[liIndex];
    return lEvent;
}

// The set is exactly these (index, original position) pairs, in order.
static bool SetIs(const CrashModeScoring& lr, const u16* lpuIndices, const u16* lpuPositions, s32 liCount)
{
    if (lr.maRecentCrashes.GetCount() != liCount)
        return false;
    for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
    {
        const RecentCrash& lrCrash = lr.maRecentCrashes[static_cast<u32>(liIndex)];
        if (lrCrash.muTrafficCarIndex != lpuIndices[liIndex] || lrCrash.muCrashChainCount != lpuPositions[liIndex]
            || lrCrash.mfTimeOfCrash != 10.0f * static_cast<f32>(lpuPositions[liIndex]))
            return false;
    }
    return true;
}

int main()
{
    {
        const u16 lauSet[] = { 5, 7, 9, 12, 7 };
        CrashModeScoring& lr = Scorer(lauSet, 5);
        const u16 lauRemoved[] = { 7, 12 };
        const BrnGui::GuiRemovedTrafficEvent lEvent = Removed(lauRemoved, 2);
        lr.DealWithRemovedTraffic(&lEvent);
        const u16 lauWant[] = { 5, 9, 7 }, lauWantPos[] = { 0, 2, 4 };
        Check(SetIs(lr, lauWant, lauWantPos, 3),
              "each removed id erases its FIRST recent-crash entry, order kept (7 at [1] and 12 go, the second 7 stays)  @0x8232C064");
        Check(lr.GetRecentCrash(12) == nullptr && lr.GetRecentCrash(7) == &lr.maRecentCrashes[2u],
              "...so GetRecentCrash no longer rejects a new car in slot 12 (DealWithHitTrafficCar's gate)");
    }
    {
        const u16 lauSet[] = { 3, 3, 3 };
        CrashModeScoring& lr = Scorer(lauSet, 3);
        const u16 lauRemoved[] = { 3, 3 };
        const BrnGui::GuiRemovedTrafficEvent lEvent = Removed(lauRemoved, 2);
        lr.DealWithRemovedTraffic(&lEvent);
        const u16 lauWant[] = { 3 }, lauWantPos[] = { 2 };
        Check(SetIs(lr, lauWant, lauWantPos, 1),
              "an id listed twice erases two entries (the recent count is re-read per removed id)  @0x8232C020");
    }
    {
        const u16 lauSet[] = { 1, 2, 3 };
        CrashModeScoring& lr = Scorer(lauSet, 3);
        const u16 lauRemoved[] = { 40, 41 };
        const BrnGui::GuiRemovedTrafficEvent lEvent = Removed(lauRemoved, 2);
        lr.DealWithRemovedTraffic(&lEvent);
        const u16 lauWantPos[] = { 0, 1, 2 };
        Check(SetIs(lr, lauSet, lauWantPos, 3), "ids not in the set change nothing");
    }
    {
        const u16 lauSet[] = { 1, 2, 3 };
        CrashModeScoring& lr = Scorer(lauSet, 3);
        const BrnGui::GuiRemovedTrafficEvent lEvent = Removed(nullptr, 0);
        lr.DealWithRemovedTraffic(&lEvent);
        const u16 lauWantPos[] = { 0, 1, 2 };
        Check(SetIs(lr, lauSet, lauWantPos, 3), "an empty removed list changes nothing  @0x8232BFE4");
    }
    {
        // The producer's Array<short,25> carries a (short)-1 as 0xFFFF; lhz/cmplw match it as a u16.
        const u16 lauSet[] = { 0xFFFFu, 4 };
        CrashModeScoring& lr = Scorer(lauSet, 2);
        const u16 lauRemoved[] = { 0xFFFFu };
        const BrnGui::GuiRemovedTrafficEvent lEvent = Removed(lauRemoved, 1);
        lr.DealWithRemovedTraffic(&lEvent);
        const u16 lauWant[] = { 4 }, lauWantPos[] = { 1 };
        Check(SetIs(lr, lauWant, lauWantPos, 1), "the match is an unsigned 16-bit compare (0xFFFF)  @0x8232C044");
    }
    {
        // A 64-entry set (full FIFO): removing the last and the first.
        u16 lauSet[64];
        for (s32 liIndex = 0; liIndex < 64; ++liIndex) lauSet[liIndex] = static_cast<u16>(100 + liIndex);
        CrashModeScoring& lr = Scorer(lauSet, 64);
        const u16 lauRemoved[] = { 163, 100 };
        const BrnGui::GuiRemovedTrafficEvent lEvent = Removed(lauRemoved, 2);
        lr.DealWithRemovedTraffic(&lEvent);
        Check(lr.maRecentCrashes.GetCount() == 62 && lr.maRecentCrashes[0u].muTrafficCarIndex == 101
                  && lr.maRecentCrashes[61u].muTrafficCarIndex == 162 && !lr.maRecentCrashes.IsFull(),
              "a full set drops the removed cars (no longer full, so DealWithHitTrafficCar stops evicting [0])");
    }
    {
        // An event never Constructed: the non-gating assert fires, and -1 runs no pass.
        const u16 lauSet[] = { 1, 2 };
        CrashModeScoring& lr = Scorer(lauSet, 2);
        BrnGui::GuiRemovedTrafficEvent lEvent;
        lEvent.mRemovedTrafficArray.MarkUnconstructed();
        const unsigned luBefore = gAsserts;
        lr.DealWithRemovedTraffic(&lEvent);
        const unsigned luFired = gAsserts - luBefore;
        gAsserts = luBefore;   // expected here
        const u16 lauWantPos[] = { 0, 1 };
        Check(luFired >= 1 && SetIs(lr, lauSet, lauWantPos, 2),
              "a -1 count fires CgsArray.h:336 and runs no pass (cmpwi r25, 0 ; ble)  @0x8232BFC0");
    }
    {
        // 25 removed ids (the record's capacity) against 30 recent crashes.
        u16 lauSet[30];
        for (s32 liIndex = 0; liIndex < 30; ++liIndex) lauSet[liIndex] = static_cast<u16>(liIndex);
        CrashModeScoring& lr = Scorer(lauSet, 30);
        u16 lauRemoved[25];
        for (s32 liIndex = 0; liIndex < 25; ++liIndex) lauRemoved[liIndex] = static_cast<u16>(29 - liIndex);
        const BrnGui::GuiRemovedTrafficEvent lEvent = Removed(lauRemoved, 25);
        lr.DealWithRemovedTraffic(&lEvent);
        const u16 lauWant[] = { 0, 1, 2, 3, 4 }, lauWantPos[] = { 0, 1, 2, 3, 4 };
        Check(SetIs(lr, lauWant, lauWantPos, 5), "a full 25-id record removes all 25 matches");
    }

    Check(gAsserts == 0, "valid records fire no assert");
    std::printf("FxGs2RemovedTraffic: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
