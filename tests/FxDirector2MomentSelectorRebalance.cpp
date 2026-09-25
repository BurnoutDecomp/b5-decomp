// FX-DIRECTOR2 (crash parity 2026-09-25, CC-12): MomentSelector::Update's max-active-moments rebalance, and the two
// pickers it needs.
//
// The runner (run_fxdirector2_moment_selector_rebalance.py) lifts the revision's PRODUCTION MomentSelector::Update,
// PickBestInhibitedMoment and PickWorstUninhibitedMoment (with their constants) out of BrnMomentSelector.cpp into
// fxd2_rebalance.inc. It compiles them against the real BrnMomentSelector.h. The moments are recording FakeMoments;
// MomentHandle::GetMoment maps a handle to the FakeMoment at its index. The real CgsStrStream.cpp backs the Update's
// one-shot [jump-ladder] line (gpDebugPrint is null here, so it never prints).
//
// Against the ARTIST bodies:
//   MomentSelector::Update @0x82239FC0, the tail 0x8223A41C..0x8223A660 -- with a limit (+0x1E2) and a ready-but-
//     held-back moment: under budget, un-inhibit the best FUSSY candidates (a direct `stb 0, 0x17B`), returning when
//     the held-back count runs out; then the swap loop (best FUSSY un-inhibited, worst FUSSY inhibited through
//     Moment::Inhibit -- +0x17B = 1, Release, meState = SEARCHING), `do { if (!idle) break; ... } while (held-back)`.
//   PickBestInhibitedMoment @0x8221C028 / PickWorstUninhibitedMoment @0x8221C358 -- (1 - recency) * weighting, the
//     FUSSY / ANY branch tables, strict compares (a NaN never wins), the unknown-option assert.
// ArbStateCrashing::Construct @0x82259EA0 is the retail setup: four moments (TUMBLING lead inhibitable, TUMBLING
// trucking-side inhibitable, HARD_STOP not, BYSTANDER not) under a limit of ONE.
#include "GameSource/Director/MomentController/BrnMomentSelector.h"
#include "GameSource/Director/MomentController/BrnMoment.h"
#include "GameSource/Director/MomentController/BrnMomentController.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;
static std::vector<std::string> gAssertLog;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcText, const char*, int)
    {
        ++gAsserts;
        gAssertLog.push_back(lpcText ? lpcText : "");
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log
{
    DebugPrint* gpDebugPrint = nullptr;
    StrStreamBase& DebugPrint::operator<<(const char*) { return *this; }
}
}

using namespace BrnDirector;

// ---- the moments -----------------------------------------------------------------------------------------------
static std::vector<const Moment*> gReleases;

struct FakeMoment : public Moment
{
    bool Prepare(void*) override { return true; }
    void Update(f32, void*, const void*) override {}
    bool Release() override
    {
        gReleases.push_back(this);
        return true;
    }
    const char* GetName() const override { return "FakeMoment"; }
    EType GetInstanceType() override { return E_MOMENT_TUMBLING; }
};

static FakeMoment gaMoments[MomentSelector::KU_MAX_MOMENTS];
static MomentSelector gSelector;

namespace BrnDirector
{
    void Moment::SetParameters(const Parameters*) {}
    void Moment::Destruct() {}
namespace Camera
{
    void Camera::Construct() {}
    void ValidityAccount::MaskToFailFlags() {}
}

    // The handle -> the FakeMoment at its index in the selector's handle array.
    Moment* MomentController::MomentHandle::GetMoment() const
    {
        CGS_ASSERT(mbIsAllocated, "mbIsAllocated");
        const ptrdiff_t liIndex = this - &gSelector.mMomentHandleArray.maElements[0];
        return &gaMoments[liIndex];
    }

#include "fxd2_rebalance.inc"
}

// ---- helpers -----------------------------------------------------------------------------------------------
static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("FAIL  %s\n", lpcName);
    }
}

struct MomentSetup
{
    bool mbAllocated;
    bool mbCanBeInhibited;
    f32  mfWeighting;
    f32  mfRecency;
    bool mbInhibited;
    bool mbConditionsMet;
    Moment::EState meState;
    bool mbCanSwitchToMeNow;
};

// A prepared selector over N moments; the recency factor is 1 so Update's decay leaves the recencies as set.
static void Setup(const MomentSetup* lpaSetup, u32 luCount, bool lbHasLimit, u32 luLimit)
{
    std::memset(&gSelector, 0, sizeof(gSelector));
    gSelector.mMomentDescriptionArray.miCount = static_cast<s32>(luCount);
    gSelector.mMomentHandleArray.miCount      = static_cast<s32>(luCount);
    gSelector.mRecencyArray.miCount           = static_cast<s32>(luCount);
    gSelector.mfRecencyFactor                 = 1.0f;
    gSelector.mbPrepared                      = true;
    gSelector.mbHasMaxLimit                   = lbHasLimit;
    gSelector.muMaxActiveMomentLimit          = luLimit;
    gSelector.mbHasSelectedMoment             = false;
    for (u32 i = 0; i < luCount; ++i)
    {
        const MomentSetup& lrSetup = lpaSetup[i];
        MomentDescription& lrDescription = gSelector.mMomentDescriptionArray.maElements[i];
        lrDescription.meMomentType     = Moment::E_MOMENT_TUMBLING;
        lrDescription.meMomentParamID  = static_cast<MomentParameterBank::EMomentParamID>(0);
        lrDescription.mfWeighting      = lrSetup.mfWeighting;
        lrDescription.mbCanBeInhibited = lrSetup.mbCanBeInhibited;
        gSelector.mMomentHandleArray.maElements[i].mbIsAllocated = lrSetup.mbAllocated;
        gSelector.mRecencyArray.maElements[i] = lrSetup.mfRecency;
        FakeMoment& lrMoment = gaMoments[i];
        lrMoment.meState              = lrSetup.meState;
        lrMoment.mbIsInhibited        = lrSetup.mbInhibited;
        lrMoment.mbConditionsMet      = lrSetup.mbConditionsMet;
        lrMoment.mbCanSwitchToMeNow   = lrSetup.mbCanSwitchToMeNow;
        lrMoment.mbCanSwitchFromMeNow = true;
    }
    gReleases.clear();
    gAssertLog.clear();
}

static const Moment::EState KE_SEARCHING = Moment::E_STATE_INVALID_SEARCHING;
static const Moment::EState KE_VALID     = Moment::E_STATE_VALID;

int main()
{
    // ================= (a) the crash state's swap =================
    // lead tumbling: running (uninhibited) but its conditions are not met; trucking-side tumbling: held back
    // (inhibited by Prepare) with its conditions met; hard stop and bystander: not inhibitable, ready.
    {
        const MomentSetup kaSetup[] = {
            { true, true,  1.0f, 0.0f, false, false, KE_SEARCHING, false },   // 0 lead tumbling
            { true, true,  1.0f, 0.0f, true,  true,  KE_SEARCHING, false },   // 1 trucking-side tumbling
            { true, false, 1.0f, 0.0f, false, true,  KE_SEARCHING, false },   // 2 hard stop
            { true, false, 1.0f, 0.0f, false, true,  KE_SEARCHING, false },   // 3 bystander
        };
        Setup(kaSetup, 4, true, 1);
        gSelector.Update(1.0f / 30.0f);
        Check(!gaMoments[1].IsInhibited() && gaMoments[0].IsInhibited()
                  && gReleases.size() == 1 && gReleases[0] == &gaMoments[0]
                  && gaMoments[0].GetState() == KE_SEARCHING && gAssertLog.empty(),
              "K1 limit 1, the lead idle and the trucking-side held back: the swap un-inhibits the trucking-side "
              "moment and inhibits the lead (+0x17B = 1, Release, SEARCHING)");
        Check(!gaMoments[2].IsInhibited() && !gaMoments[3].IsInhibited(),
              "K2 the two non-inhibitable moments are untouched");
    }

    // ================= (b) both held back, under budget =================
    {
        const MomentSetup kaSetup[] = {
            { true, true,  1.0f, 0.75f, true, true, KE_SEARCHING, false },   // 0: (1 - 0.75) * 1 = 0.25
            { true, true,  1.0f, 0.25f, true, true, KE_SEARCHING, false },   // 1: (1 - 0.25) * 1 = 0.75  <- best
            { true, false, 1.0f, 0.0f,  false, true, KE_SEARCHING, false },
        };
        Setup(kaSetup, 3, true, 1);
        gSelector.Update(1.0f / 30.0f);
        Check(!gaMoments[1].IsInhibited() && gaMoments[0].IsInhibited() && gReleases.empty()
                  && gaMoments[1].GetState() == KE_SEARCHING,
              "K3 limit 1 with none running: the higher (1 - recency) * weighting held-back moment is un-inhibited "
              "directly (no Release, no state change) and the other stays held back");
    }

    // ================= (c) no limit =================
    {
        const MomentSetup kaSetup[] = {
            { true, true, 1.0f, 0.0f, false, false, KE_SEARCHING, false },
            { true, true, 1.0f, 0.0f, true,  true,  KE_SEARCHING, false },
        };
        Setup(kaSetup, 2, false, 1);
        gSelector.Update(1.0f / 30.0f);
        Check(gaMoments[1].IsInhibited() && !gaMoments[0].IsInhibited() && gReleases.empty(),
              "K4 mbHasMaxLimit false (`lbz 0x1E2 ; beq`): nothing is rebalanced");
    }

    // ================= (e) the early return inside the first loop =================
    {
        const MomentSetup kaSetup[] = {
            { true, true, 1.0f, 0.0f, false, false, KE_SEARCHING, false },   // idle, running
            { true, true, 1.0f, 0.0f, true,  true,  KE_SEARCHING, false },   // held back
        };
        Setup(kaSetup, 2, true, 2);
        gSelector.Update(1.0f / 30.0f);
        Check(!gaMoments[1].IsInhibited() && !gaMoments[0].IsInhibited() && gReleases.empty(),
              "K5 limit 2 with one running: the held-back moment is un-inhibited and Update RETURNS as the "
              "held-back count reaches 0 -- the idle one is not swapped out");
    }

    // ================= the swap loop runs until the held-back count is spent =================
    {
        const MomentSetup kaSetup[] = {
            { true, true, 1.0f, 0.5f, false, false, KE_SEARCHING, false },   // idle, score 0.5
            { true, true, 1.0f, 0.9f, false, false, KE_SEARCHING, false },   // idle, score 0.1  <- worst
            { true, true, 1.0f, 0.0f, true,  true,  KE_SEARCHING, false },   // held back, score 1.0 <- best
            { true, true, 1.0f, 0.6f, true,  true,  KE_SEARCHING, false },   // held back, score 0.4
        };
        Setup(kaSetup, 4, true, 1);
        gSelector.Update(1.0f / 30.0f);
        // over budget (2 running, limit 1): swap 1 -- best held back #2 in, worst idle #1 out; swap 2 -- best
        // held back #3 in, worst idle #0 out (the only idle one left not inhibited).
        Check(!gaMoments[2].IsInhibited() && !gaMoments[3].IsInhibited() && gaMoments[1].IsInhibited()
                  && gaMoments[0].IsInhibited() && gReleases.size() == 2 && gReleases[0] == &gaMoments[1]
                  && gReleases[1] == &gaMoments[0],
              "K6 two held back and two idle: two swaps, the best held-back in and the worst idle out each time "
              "(`while (held-back)`)");
    }

    // ================= the pickers =================
    u32 luIndex = 99;
    {
        const MomentSetup kaSetup[] = {
            { false, true, 9.0f, 0.0f,  true,  true,  KE_SEARCHING, false },   // unallocated: skipped
            { true,  true, 1.0f, 0.5f,  true,  true,  KE_SEARCHING, false },   // 0.5
            { true,  true, 2.0f, 0.25f, true,  true,  KE_SEARCHING, false },   // 1.5  <- best
            { true,  true, 3.0f, 0.5f,  true,  true,  KE_SEARCHING, false },   // 1.5  tie: the first keeps it
            { true,  true, 9.0f, 0.0f,  false, true,  KE_SEARCHING, false },   // not inhibited
            { true,  true, 9.0f, 0.0f,  true,  false, KE_SEARCHING, false },   // conditions not met
        };
        Setup(kaSetup, 6, true, 1);
        const bool lbFound = gSelector.PickBestInhibitedMoment(&luIndex, MomentSelector::E_PICK_BEST_FUSSY);
        Check(lbFound && luIndex == 2, "K7 PickBestInhibitedMoment FUSSY: the highest score among inhibited + ready "
                                       "moments; a tie keeps the first (`fcmpu ; ble`); unallocated skipped");
        const bool lbAny = gSelector.PickBestInhibitedMoment(&luIndex, MomentSelector::E_PICK_BEST_ANY);
        Check(lbAny && luIndex == 5, "K8 PickBestInhibitedMoment ANY: every allocated moment takes the seed arm -- "
                                     "the LAST allocated one wins, whatever its state");
    }
    {
        const MomentSetup kaSetup[] = {
            { true, true, 1.0f, std::numeric_limits<f32>::quiet_NaN(), true, true, KE_SEARCHING, false },   // seeds NaN
            { true, true, 1.0f, 0.0f, true, true, KE_SEARCHING, false },                                       // 1.0
        };
        Setup(kaSetup, 2, true, 1);
        gSelector.PickBestInhibitedMoment(&luIndex, MomentSelector::E_PICK_BEST_FUSSY);
        Check(luIndex == 0, "K9 a NaN seed is never beaten (`fcmpu ; ble` is taken when unordered)");
    }
    {
        const MomentSetup kaSetup[] = {
            { true, true, 1.0f, 0.0f, false, true, KE_SEARCHING, false },   // running, ready: not a candidate
        };
        Setup(kaSetup, 1, true, 1);
        luIndex = 99;
        const bool lbFound = gSelector.PickBestInhibitedMoment(&luIndex, MomentSelector::E_PICK_BEST_FUSSY);
        Check(!lbFound && luIndex == 0, "K10 nothing to pick: false, and the index is still written (0)");
        gAssertLog.clear();
        gSelector.PickBestInhibitedMoment(&luIndex, static_cast<MomentSelector::EPickBestInhibitedOptions>(2));
        gSelector.PickWorstUninhibitedMoment(&luIndex, static_cast<MomentSelector::EPickWorstUninhibitedOptions>(7));
        Check(gAssertLog.size() == 2 && gAssertLog[0] == "Unknown option: " && gAssertLog[1] == "Unknown option: ",
              "K11 an unknown option trips \"Unknown option: \" in both pickers (cpp:476 / :545)");
    }
    {
        const MomentSetup kaSetup[] = {
            { true, true,  1.0f, 0.5f,  false, false, KE_SEARCHING, false },   // idle 0.5
            { true, false, 1.0f, 0.9f,  false, false, KE_SEARCHING, false },   // idle 0.1, NOT inhibitable <- worst
            { true, true,  1.0f, 0.95f, true,  false, KE_SEARCHING, false },   // inhibited: not a FUSSY candidate
            { true, true,  1.0f, 0.95f, false, true,  KE_SEARCHING, false },   // ready: not a FUSSY candidate
        };
        Setup(kaSetup, 4, true, 1);
        const bool lbFound = gSelector.PickWorstUninhibitedMoment(&luIndex, MomentSelector::E_PICK_WORST_FUSSY);
        Check(lbFound && luIndex == 1, "K12 PickWorstUninhibitedMoment FUSSY: the lowest score among the idle "
                                       "moments -- the description's mbCanBeInhibited is NOT consulted");
        const bool lbAny = gSelector.PickWorstUninhibitedMoment(&luIndex, MomentSelector::E_PICK_WORST_ANY);
        Check(lbAny && luIndex == 3, "K13 PickWorstUninhibitedMoment ANY: the last inhibitable moment takes the seed "
                                     "arm and wins");
    }

    {
        const MomentSetup kaSetup[] = {
            { true, true, 1.0f, 0.7f, false, false, KE_SEARCHING, false },   // idle 0.3
            { true, true, 1.0f, 0.7f, false, false, KE_SEARCHING, false },   // idle 0.3 (tie)
        };
        Setup(kaSetup, 2, true, 1);
        gSelector.PickWorstUninhibitedMoment(&luIndex, MomentSelector::E_PICK_WORST_FUSSY);
        Check(luIndex == 0, "K14 PickWorstUninhibitedMoment FUSSY: a tie keeps the first (`fcmpu ; bge`)");
    }
    {
        const MomentSetup kaSetup[] = {
            { true, false, 1.0f, 0.9f, false, false, KE_SEARCHING, false },   // the only idle moment, NOT inhibitable
        };
        Setup(kaSetup, 1, true, 1);
        luIndex = 99;
        const bool lbFound = gSelector.PickWorstUninhibitedMoment(&luIndex, MomentSelector::E_PICK_WORST_FUSSY);
        Check(lbFound && luIndex == 0, "K15 PickWorstUninhibitedMoment FUSSY seeds on the first idle moment whatever its "
                                       "description says (0x8221C4C4..0x8221C554)");
    }
    {
        // Two running idle, one held back: ONE swap, then the held-back count is spent and the loop stops (`while
        // (held-back)`, not `while (idle)` -- a second pass would find no held-back moment and assert).
        const MomentSetup kaSetup[] = {
            { true, true, 1.0f, 0.5f, false, false, KE_SEARCHING, false },   // idle 0.5
            { true, true, 1.0f, 0.9f, false, false, KE_SEARCHING, false },   // idle 0.1 <- worst
            { true, true, 1.0f, 0.0f, true,  true,  KE_SEARCHING, false },   // held back
        };
        Setup(kaSetup, 3, true, 1);
        gSelector.Update(1.0f / 30.0f);
        Check(gReleases.size() == 1 && gReleases[0] == &gaMoments[1] && !gaMoments[2].IsInhibited()
                  && !gaMoments[0].IsInhibited() && gAssertLog.empty(),
              "K16 two idle and one held back: exactly one swap (the worst idle out), no assert");
    }

    std::printf("FxDirector2MomentSelectorRebalance: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
