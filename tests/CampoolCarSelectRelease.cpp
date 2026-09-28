// L1 CAMPOOL (owner's list 2026-09-27): ArbStateCarSelect::Release @0x82236050 -- the junkyard's camera behaviours are
// handed back when the car select gives the frame to roaming.
//
// The runner (run_campool_carselect_release.py) extracts the revision's ArbStateCarSelect::Release body from
// BrnArbStateCarSelect.cpp into campool_carselect.inc and compiles it against the revision's BrnArbStateCarSelect.h.
// The two manager calls it makes are recording stand-ins here; the handles and the interpolater helpers are the real
// inline types. The state is built in zeroed storage with every handle allocated to a distinct key (the manager does
// not care which behaviour a key names), then released as the CHANGING_TO_ROAMING arm releases it.
//
// The console, store for store (ARTIST 0x82236050..0x822361E8):
//   0x82236068  stw 0, 0x2D4(this)             meState = E_STATE_INACTIVE, before anything else
//   nine inlined BehaviourHandle::Release bodies, in this order (the handle's allocated byte, then
//   UnSetBehaviourUsedByHandle(handle manager, handle key), then pool / manager / behaviour / byte cleared):
//     +0x180 mTransitionCam   +0x20C mToCarSelectInterpolater   +0x25C mFromGameplayInterpolater
//     +0x234 mToGameplayInterpolater   +0x1A8 mIntroNoNewCars   +0x1BC mIntroNewCars
//     +0x1F8 mLookAroundCarCam   +0x1E4 mIdleCam   +0x1D0 mGameIntro
//   0x822361D4  CheckNoBehavioursAreAllocatedByState(lrSharedInfo.mpBehaviourManager, this)
//   li r3, 1
// and it never touches mCarUnlockCam (+0x194).
#include "GameSource/Director/Arbitrator/States/BrnArbStateCarSelect.h"

#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <vector>

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { return 0; }
    void* EndAssert() { return nullptr; }
}
}

// ---- recording stand-ins for the manager side --------------------------------------------------------------
struct UnSetCall
{
    const void* mpManager;
    u32         muKey;
    s32         meStateAtCall;   // the state's meState when the call happened
};
static std::vector<UnSetCall> gUnSets;
struct CheckCall
{
    const void* mpManager;
    const void* mpState;
    size_t      muUnSetsBefore;
};
static std::vector<CheckCall> gChecks;
static const BrnDirector::ArbStateCarSelect* gpState = nullptr;

namespace BrnDirector
{
namespace Camera
{
    void BehaviourManager::UnSetBehaviourUsedByHandle(u32 luAllocationKey)
    {
        gUnSets.push_back({ this, luAllocationKey, gpState ? static_cast<s32>(gpState->meState) : -1 });
    }
    void BehaviourManager::CheckNoBehavioursAreAllocatedByState(ArbitratorState* lpArbitratorState)
    {
        gChecks.push_back({ this, lpArbitratorState, gUnSets.size() });
    }
    void BehaviourManager::CheckNoBehavioursAreAllocatedByState(const void* lpState)
    {
        gChecks.push_back({ this, lpState, gUnSets.size() });
    }
}

// The body under test (the revision's own text).
#include "campool_carselect.inc"
}

using namespace BrnDirector;

static unsigned giChecks = 0, giFailures = 0;
static void Check(bool lbPassed, const std::string& lrLabel)
{
    ++giChecks;
    if (!lbPassed)
        ++giFailures;
    std::printf("%s  %s\n", lbPassed ? "PASS" : "FAIL", lrLabel.c_str());
}

template <typename THandle>
static void Hold(THandle& lrHandle, Camera::BehaviourManager* lpManager, u32 luKey)
{
    lrHandle.mbAllocated     = true;
    lrHandle.muAllocationKey = luKey;
    lrHandle.mpHelperPool    = reinterpret_cast<Camera::BehaviourManager::HelperPool*>(
                                   static_cast<uintptr_t>(0x1000u + luKey));   // a distinct, never-dereferenced pool
    lrHandle.mpManager       = lpManager;
    lrHandle.mpBehaviour     = nullptr;
}

template <typename THandle>
static bool Cleared(const THandle& lrHandle)
{
    return !lrHandle.mbAllocated && lrHandle.mpHelperPool == nullptr && lrHandle.mpManager == nullptr
        && lrHandle.mpBehaviour == nullptr;
}

int main()
{
    static alignas(16) unsigned char saState[sizeof(ArbStateCarSelect)];
    static alignas(16) unsigned char saInfo[sizeof(ArbStateSharedInfo)];
    static alignas(16) unsigned char saManager[64];
    std::memset(saState, 0, sizeof(saState));
    std::memset(saInfo, 0, sizeof(saInfo));
    ArbStateCarSelect& lrState = *reinterpret_cast<ArbStateCarSelect*>(saState);
    ArbStateSharedInfo& lrInfo = *reinterpret_cast<ArbStateSharedInfo*>(saInfo);
    Camera::BehaviourManager* lpManager = reinterpret_cast<Camera::BehaviourManager*>(saManager);
    lrInfo.mpBehaviourManager = lpManager;
    gpState = &lrState;

    // Every handle the state has, allocated, with the key = its console offset (so an out-of-order release is
    // readable in the output).
    Hold(lrState.mTransitionCam,                           lpManager, 0x180);
    Hold(lrState.mCarUnlockCam,                            lpManager, 0x194);
    Hold(lrState.mIntroNoNewCars,                          lpManager, 0x1A8);
    Hold(lrState.mIntroNewCars,                            lpManager, 0x1BC);
    Hold(lrState.mGameIntro,                               lpManager, 0x1D0);
    Hold(lrState.mIdleCam,                                 lpManager, 0x1E4);
    Hold(lrState.mLookAroundCarCam,                        lpManager, 0x1F8);
    Hold(lrState.mToCarSelectInterpolater.mInterpolater,   lpManager, 0x20C);
    Hold(lrState.mToGameplayInterpolater.mInterpolater,    lpManager, 0x234);
    Hold(lrState.mFromGameplayInterpolater.mInterpolater,  lpManager, 0x25C);
    lrState.meState = ArbStateCarSelect::E_STATE_CHANGING_TO_ROAMING;

    // The CHANGING_TO_ROAMING arm's call, dispatched to the override (qualified: the state was never constructed,
    // so it has no vptr to dispatch through).
    const bool lbReturned = lrState.ArbStateCarSelect::Release(lrInfo);

    Check(lbReturned, "Release returns true (li r3, 1)");
    Check(lrState.meState == ArbStateCarSelect::E_STATE_INACTIVE, "meState is INACTIVE after Release (+0x2D4)");
    Check(!gUnSets.empty() && gUnSets[0].meStateAtCall == ArbStateCarSelect::E_STATE_INACTIVE,
          "meState = INACTIVE is the FIRST store (0x82236068), before any handle is released");

    static const u32 KAU_ORDER[9] = { 0x180, 0x20C, 0x25C, 0x234, 0x1A8, 0x1BC, 0x1F8, 0x1E4, 0x1D0 };
    static const char* const KAPC_NAMES[9] = { "mTransitionCam", "mToCarSelectInterpolater", "mFromGameplayInterpolater",
                                               "mToGameplayInterpolater", "mIntroNoNewCars", "mIntroNewCars",
                                               "mLookAroundCarCam", "mIdleCam", "mGameIntro" };
    char lacLabel[200];
    std::snprintf(lacLabel, sizeof(lacLabel), "nine UnSetBehaviourUsedByHandle calls (got %zu)", gUnSets.size());
    Check(gUnSets.size() == 9, lacLabel);
    for (int liCall = 0; liCall < 9; ++liCall)
    {
        const bool lbHave = liCall < static_cast<int>(gUnSets.size());
        std::snprintf(lacLabel, sizeof(lacLabel), "release #%d is %s (+0x%X) through the handle's manager (got %s0x%X)",
                      liCall + 1, KAPC_NAMES[liCall], KAU_ORDER[liCall], lbHave ? "" : "none ",
                      lbHave ? gUnSets[liCall].muKey : 0u);
        Check(lbHave && gUnSets[liCall].muKey == KAU_ORDER[liCall] && gUnSets[liCall].mpManager == lpManager, lacLabel);
    }

    Check(Cleared(lrState.mTransitionCam),                          "mTransitionCam cleared");
    Check(Cleared(lrState.mToCarSelectInterpolater.mInterpolater),  "mToCarSelectInterpolater cleared");
    Check(Cleared(lrState.mFromGameplayInterpolater.mInterpolater), "mFromGameplayInterpolater cleared");
    Check(Cleared(lrState.mToGameplayInterpolater.mInterpolater),   "mToGameplayInterpolater cleared");
    Check(Cleared(lrState.mIntroNoNewCars),                         "mIntroNoNewCars cleared");
    Check(Cleared(lrState.mIntroNewCars),                           "mIntroNewCars cleared");
    Check(Cleared(lrState.mLookAroundCarCam),                       "mLookAroundCarCam cleared");
    Check(Cleared(lrState.mIdleCam),                                "mIdleCam cleared");
    Check(Cleared(lrState.mGameIntro),                              "mGameIntro cleared");
    Check(lrState.mCarUnlockCam.mbAllocated && lrState.mCarUnlockCam.muAllocationKey == 0x194
              && lrState.mCarUnlockCam.mpManager == lpManager,
          "mCarUnlockCam (+0x194) is NOT released (the CAR_UNLOCK arm hands it back itself)");

    Check(gChecks.size() == 1 && gChecks[0].mpManager == lpManager && gChecks[0].mpState == &lrState,
          "CheckNoBehavioursAreAllocatedByState(lrSharedInfo.mpBehaviourManager, this), once");
    Check(gChecks.size() == 1 && gChecks[0].muUnSetsBefore == gUnSets.size(),
          "the check runs after the last release (0x822361D4)");

    // A second Release (nothing held any more) releases nothing and still checks.
    const size_t luUnSetsBefore = gUnSets.size();
    lrState.meState = ArbStateCarSelect::E_STATE_IDLE;
    const bool lbSecond = lrState.ArbStateCarSelect::Release(lrInfo);
    Check(lbSecond && gUnSets.size() == luUnSetsBefore && gChecks.size() == 2
              && lrState.meState == ArbStateCarSelect::E_STATE_INACTIVE,
          "a second Release skips the unallocated handles, still checks and drops to INACTIVE");

    std::printf("CampoolCarSelectRelease: %u checks, %u failures\n", giChecks, giFailures);
    return giFailures == 0 ? 0 : 1;
}
