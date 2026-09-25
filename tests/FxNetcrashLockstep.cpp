// crash parity FX-NETCRASH (2026-09-25), online traffic lockstep: RecalculateActiveHulls' hull ORDER.
//
// run_fxnetcrash_lockstep.py extracts the PRODUCTION statements of TrafficEntityModule::
// RecalculateActiveHulls @0x8274C870 from the previous-set snapshot to the second SetDifference
// (0x8274C9A0..0x8274CBB8): the union of every race car's hull list into mActiveHulls, the
// console's std::_Sort<ushort *,int> @0x8274CB98, and the two SetDifferences that produce the
// caller's new and old hull sets. They are compiled as LockFixture::Recalc over the real ::Set / ::Array
// containers.
//
// Two "machines" hold the same two players in opposite slots, as a LAN pair does: each machine is
// its own active race car 0. Online, every machine must visit the hulls in the same order, because
// FillNewHull (new set), KillOutOfAreaTraffic (old set) and RebuildGeneratorList (mActiveHulls)
// each draw the traffic RNG hull by hull. The expected order is the console's: ascending.
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Containers/CgsSet.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>

static unsigned guAsserts = 0, guChecks = 0, guFailures = 0;

namespace CgsDev
{
namespace Assert
{
    char* gpcMessageBuffer = nullptr;
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int)
    {
        ++guAsserts;
        std::printf("ASSERT: %s\n", lpcMessage);
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
}

namespace BrnTraffic
{
namespace
{
    inline void LogMissingLeg_T1(bool& lrbAlreadyLogged, const char*) { lrbAlreadyLogged = true; }
}

    struct LockFixture
    {
        typedef TrafficEntityModule::ActiveHullSet ActiveHullSet;

        ::Array<u16, KU_MAX_ACTIVE_HULLS_PER_RACECAR> maaRaceCarHulls[E_ACTIVE_RACE_CAR_INDEX_COUNT];
        ActiveHullSet mActiveHulls;
        s32           miDEBUGOverBudgetness;

        void Construct()
        {
            for (u32 luCar = 0; luCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++luCar)
            {
                maaRaceCarHulls[luCar].Construct();
            }
            mActiveHulls.Construct();
            miDEBUGOverBudgetness = 7;
        }

        void SetCar(u32 luCar, const std::vector<u16>& lrHulls)
        {
            maaRaceCarHulls[luCar].Clear();
            for (u16 luHull : lrHulls)
            {
                maaRaceCarHulls[luCar].Append(luHull);
            }
        }

        void Recalc(ActiveHullSet* lpOutNewHulls, ActiveHullSet* lpOutOldHulls)
        {
#include "recalc_block.inc"
        }
    };
}

typedef BrnTraffic::LockFixture::ActiveHullSet HullSet;

static std::vector<u16> Items(const HullSet& lrSet)
{
    std::vector<u16> la;
    for (u32 luIndex = 0; luIndex < lrSet.GetLength(); ++luIndex)
    {
        la.push_back(lrSet.GetItem(luIndex));
    }
    return la;
}

static std::string Text(const std::vector<u16>& la)
{
    std::string ls;
    for (u16 lu : la)
    {
        ls += (ls.empty() ? "" : ",") + std::to_string(lu);
    }
    return ls;
}

static void Check(bool lbPass, const char* lpcName, const std::string& lrDetail = std::string())
{
    ++guChecks;
    if (!lbPass)
    {
        ++guFailures;
    }
    std::printf("%s  %s%s%s\n", lbPass ? "PASS" : "FAIL", lpcName, lrDetail.empty() ? "" : "  -- ", lrDetail.c_str());
}

// One decision frame on one machine: set the two players' hull lists in THIS machine's slot order,
// run the production statements, and return (active, new, old).
struct Frame { std::vector<u16> mActive, mNew, mOld; };

static Frame Step(BrnTraffic::LockFixture& lrMachine, const std::vector<u16>& lrSlot0, const std::vector<u16>& lrSlot1)
{
    lrMachine.SetCar(0, lrSlot0);
    lrMachine.SetCar(1, lrSlot1);
    HullSet lNew, lOld;
    lNew.Construct();
    lOld.Construct();
    lrMachine.Recalc(&lNew, &lOld);
    Frame lFrame;
    lFrame.mActive = Items(lrMachine.mActiveHulls);
    lFrame.mNew    = Items(lNew);
    lFrame.mOld    = Items(lOld);
    return lFrame;
}

int main()
{
    // Hull lists in the replay's shape: {the car's hull} then its PVS, in the Pvs' own order.
    const std::vector<u16> laHost0 = { 144, 123, 165, 145, 143, 124, 122, 166, 164 };   // host in hull 144
    const std::vector<u16> laGuest = { 102, 81, 123, 103, 101, 82, 80, 124, 122 };      // guest in hull 102
    const std::vector<u16> laHost1 = { 124, 103, 145, 125, 123, 104, 102, 146, 144 };   // host moves to 124

    BrnTraffic::LockFixture* lpHost  = new BrnTraffic::LockFixture();   // host: itself in slot 0
    BrnTraffic::LockFixture* lpGuest = new BrnTraffic::LockFixture();   // guest: itself in slot 0
    lpHost->Construct();
    lpGuest->Construct();

    // Decision frame 1: both players' lists arrive (from an empty set).
    const Frame lHost1  = Step(*lpHost,  laHost0, laGuest);
    const Frame lGuest1 = Step(*lpGuest, laGuest, laHost0);
    Check(lHost1.mActive == lGuest1.mActive,
          "frame 1: both machines hold mActiveHulls in the SAME order, whatever slot each player occupies",
          "host " + Text(lHost1.mActive) + " | guest " + Text(lGuest1.mActive));
    Check(std::is_sorted(lHost1.mActive.begin(), lHost1.mActive.end()) && !lHost1.mActive.empty(),
          "frame 1: mActiveHulls is ascending (std::_Sort @0x8274CB98)", Text(lHost1.mActive));
    Check(lHost1.mNew == lGuest1.mNew,
          "frame 1: the NEW hulls FillNewHull visits come in the same order on both",
          "host " + Text(lHost1.mNew) + " | guest " + Text(lGuest1.mNew));

    // Decision frame 2: the host moves; the same change applies on both machines (lockstep).
    const Frame lHost2  = Step(*lpHost,  laHost1, laGuest);
    const Frame lGuest2 = Step(*lpGuest, laGuest, laHost1);
    Check(lHost2.mActive == lGuest2.mActive && std::is_sorted(lHost2.mActive.begin(), lHost2.mActive.end()),
          "frame 2: mActiveHulls identical and ascending after the host's hull change",
          "host " + Text(lHost2.mActive) + " | guest " + Text(lGuest2.mActive));
    Check(lHost2.mNew == lGuest2.mNew && !lHost2.mNew.empty(),
          "frame 2: the NEW hulls (FillNewHull order) match", "host " + Text(lHost2.mNew) + " | guest " + Text(lGuest2.mNew));
    Check(lHost2.mOld == lGuest2.mOld && !lHost2.mOld.empty(),
          "frame 2: the OLD hulls (KillOutOfAreaTraffic order) match", "host " + Text(lHost2.mOld) + " | guest " + Text(lGuest2.mOld));

    // The SET is the same with or without the sort (control): only the ORDER is at stake.
    std::vector<u16> laHostSorted = lHost2.mActive, laGuestSorted = lGuest2.mActive;
    std::sort(laHostSorted.begin(), laHostSorted.end());
    std::sort(laGuestSorted.begin(), laGuestSorted.end());
    Check(laHostSorted == laGuestSorted, "control: the two machines' active SETS agree");

    // Control: empty lists -- the sort is skipped (the console's `ble` @0x8274CB10), nothing asserts.
    BrnTraffic::LockFixture* lpEmpty = new BrnTraffic::LockFixture();
    lpEmpty->Construct();
    const unsigned luAssertsBefore = guAsserts;
    const Frame lEmpty = Step(*lpEmpty, std::vector<u16>(), std::vector<u16>());
    Check(lEmpty.mActive.empty() && lEmpty.mNew.empty() && lEmpty.mOld.empty() && guAsserts == luAssertsBefore,
          "control: no hulls -> empty sets, no assert");

    std::printf("FxNetcrashLockstep: %u checks, %u failures\n", guChecks, guFailures);
    delete lpHost;
    delete lpGuest;
    delete lpEmpty;
    return guFailures ? 1 : 0;
}
