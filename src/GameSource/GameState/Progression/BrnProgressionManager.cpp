// ===================================================================================
// BrnProgression::ProgressionManager  -- the offline progression / unlock manager.
//   GameSource/Unity/../GameState/Progression/BrnProgressionManager.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the ASM is the spine: signatures, branches,
// constants and side-effects are taken from the disassembly, not the Hex-Rays pseudocode).
// The nine functions homed here are the boot-trace + caller-named slice of the manager; the
// full ProgressionManager TU is much larger. Every member is reached BY NAME through the
// minimal layout in BrnProgressionManager.h -- no raw-offset pointer arithmetic.
//
// NOTE on the X360 byte offsets quoted in BrnProgressionManager.h: they are the 32-bit-pointer
// ABI offsets proven from the XEX. The PC reconstruction compiles 64-bit, so the embedded
// Profile and pointer members are naturally wider; behaviour is identical because every access
// is by name.
// ===================================================================================

#include "BrnProgressionManager.h"
#include "BrnProfile.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <string.h>   // memcpy (X360 XMemCpy)

namespace BrnProgression
{

// ------------------------------------------------------------------------------------
// ProgressionManager::ProgressionManager  @ 0x827DEA50  (EXECUTED in the boot trace)
//
// X360 ctor. It performs three groups of stores:
//   (1) resets the 18 manager-handle head slots to the -1 sentinel (the ctor loop:
//       18 stores of -1 at +0x10, stride 0x14);
//   (2) marks the embedded Profile's index->element containers "unconstructed" by poking
//       the -1 sentinel into their count words (the freeburn-challenge array + the five
//       mugshot arrays + the late trophy/achievement tail). FLAG: those count words are
//       PRIVATE Profile state; the X360 reaches them as raw interior offsets while inlining.
//       This reconstruction does NOT poke Profile internals by raw offset (forbidden) -- the
//       embedded Profile is default-constructed and its container-sentinel reset is owned by
//       the Profile TU's own Construct/Clear path. The observable manager-level state the
//       boot trace depends on (head slots, debug component, event lists) is reproduced below.
//   (3) installs the progression debug component's vtable and empties the two intrusive
//       event lists to their self-referential sentinel state.
// ------------------------------------------------------------------------------------
ProgressionManager::ProgressionManager()
{
    // (1) 18 head handle slots -> -1 "unset" sentinel.
    for (s32 liSlot = 0; liSlot < KI_HANDLE_SLOT_COUNT; ++liSlot)
    {
        maHandleSlots[liSlot].mi32Id = -1;
    }

    // (3a) install the debug component's vtable (X360: result[33250] = off_820CDE4C). The
    // real ProgressionDebugComponent vptr is supplied when Prepare2 constructs it; the ctor
    // only seeds the slot so an early teardown sees a valid component. FLAG: the X360 vtable
    // symbol off_820CDE4C is not yet homed; modelled as the seeded-null slot.
    mDebugComponent.mpVTable = nullptr;

    // road-rules ruled tallies start clear (the ctor leaves the +133456..+133464 region 0).
    miNumberOfParCrashRoadRulesRuledByPlayer         = 0;
    miNumberOfParTimeRoadRulesRuledByPlayer          = 0;
    miNumberOfNumberOfCompleteRoadRulesRuledByPlayer = 0;

    // Prepare2 back-pointers start null.
    mpTriggerData        = nullptr;
    mpGameStateModule    = nullptr;
    mpAchievementManager = nullptr;

    // (3b) the two resource pointers (mpProgressionData / mpAISectionData) reach the
    // X360 ctor's self-referential sentinel state (count-0/self-link/zero pattern)
    // through their BaseResourcePtr default ctors -- no explicit stores needed here.
}

// ------------------------------------------------------------------------------------
// ProgressionManager::Prepare2  @ 0x8239DC98
//
// Two-phase load. Asserts the output + receiver-queue pointers, loads the progression data;
// on success wires the trigger-data + game-state-module + achievement back-pointers, computes
// the landmark AI-section indices, processes the loaded preset races, constructs + registers
// the debug component, and sets up the roaming sections. Returns true iff the load succeeded.
//
// FLAG: LoadProgressionData / ComputeLandmarkAISectionIndices / ProcessLoadedPresetRaces /
// ProgressionDebugComponent::Construct / DebugComponent::Register / SetupRoamingSections are
// sibling functions in other (not-yet-reconstructed) ProgressionManager TUs. The ASM proves
// the call sequence + the three stores; the helper bodies are out of scope for this slice, so
// the orchestration is documented rather than dispatched (calling undeclared siblings would not
// compile). The three back-pointer stores -- the observable side effects -- are reproduced.
// ------------------------------------------------------------------------------------
bool ProgressionManager::Prepare2(void* lpOutput, void* lpGameStateModule,
                                  BrnGameState::GameStateModuleIO::GameActionQueue* lpReceiverQueue,
                                  void* lpTriggerData,
                                  BrnGameState::AchievementManagerBase* lpAchievementManager)
{
    CGS_ASSERT(lpOutput != nullptr, "lpOutput");
    CGS_ASSERT(lpReceiverQueue != nullptr, "lpReceiverQueue");

    // X360: if ( LoadProgressionData(this, lpOutput, lpReceiverQueue) ) { ... } else return false;
    // LoadProgressionData lives in another ProgressionManager TU; its result gates the wiring.
    // Until it is reconstructed this slice records the back-pointers Prepare2 installs on success.
    CGS_ASSERT(lpTriggerData != nullptr, "lpTriggerData");
    mpTriggerData     = lpTriggerData;           // X360 +0x20924 (a5)
    mpGameStateModule = lpGameStateModule;       // X360 +0x2093C (a3)

    CGS_ASSERT(lpAchievementManager != nullptr, "lpAchievementManager");
    mpAchievementManager = lpAchievementManager; // X360 +0x20938 (a6)

    // X360 then calls, in order:
    //   ComputeLandmarkAISectionIndices(this);
    //   ProcessLoadedPresetRaces(this);
    //   ProgressionDebugComponent::Construct(&mDebugComponent, this, lpGameStateModule);
    //   CgsDev::DebugComponent::Register(&mDebugComponent);
    //   SetupRoamingSections(this, ...);
    // (helper bodies land with their own TUs; see FLAG above.)
    return true;
}

// ------------------------------------------------------------------------------------
// ProgressionManager::AreRoadRulesAvailable  @ 0x82311520
//
// X360: returns 1 if the player's medal-progress count (mProfile.muMedalCountFromTheStart,
// the X360 a1[10720] read) is >= 4, OR if either tail availability flag is non-zero.
// ------------------------------------------------------------------------------------
bool ProgressionManager::AreRoadRulesAvailable() const
{
    if (mProfile.GetMedalCountFromTheStart() >= 4u)
    {
        return true;
    }
    if (miNumberOfParCrashRoadRulesRuledByPlayer != 0)
    {
        return true;
    }
    if (miNumberOfParTimeRoadRulesRuledByPlayer != 0)
    {
        return true;
    }
    return false;
}

// ------------------------------------------------------------------------------------
// ProgressionManager::GetEvent  @ 0x82359850
//
// Map an offline game-mode index (0..5) to its event-data id. Pure index->constant table
// (the X360 jump table). Unknown index asserts and returns -1.
// Constants are the X360 li immediates: 0->0, 1->3, 2->7, 3->8, 4->5, 5->4.
// ------------------------------------------------------------------------------------
s32 ProgressionManager::GetEvent(s32 liGameType) const
{
    switch (liGameType)
    {
        case 0: return 0;
        case 1: return 3;
        case 2: return 7;
        case 3: return 8;
        case 4: return 5;
        case 5: return 4;
        default:
            CGS_ASSERT(false,
                "I dont know what this game type is! Maybe its new mode and needs adding this switch statement?\n");
            return -1;
    }
}

// ------------------------------------------------------------------------------------
// ProgressionManager::GetOnlin  @ 0x82359960
//
// Map an online game-mode index (0..2) to its event-data id. The X360 immediates:
// 0->10, 1->11, 2->13. Unknown index asserts and returns -1.
// ------------------------------------------------------------------------------------
s32 ProgressionManager::GetOnlin(u32 luGameType) const
{
    if (luGameType == 0)
    {
        return 10;
    }
    if (luGameType == 1)
    {
        return 11;
    }
    if (luGameType < 3)   // i.e. == 2
    {
        return 13;
    }

    CGS_ASSERT(false,
        "I dont know what this game type is! Maybe its new mode and needs adding this switch statement?\n");
    return -1;
}

// ------------------------------------------------------------------------------------
// ProgressionManager::IsCarUnlocked  @ 0x823635C0
//
// True when the player already owns lCarId. The X360 scans the embedded Profile's owned-car
// list (Profile +0x26C count word, Profile +0x280 maCars[] with a 0x18 stride, comparing the
// 8-byte id at each record's +0). Reconstructed through the named Profile car accessors.
// ------------------------------------------------------------------------------------
bool ProgressionManager::IsCarUnlocked(CgsID lCarId) const
{
    const s32 liCarCount = mProfile.GetCarCount();
    if (liCarCount <= 0)
    {
        return false;
    }

    for (s32 liIndex = 0; liIndex < liCarCount; ++liIndex)
    {
        if (mProfile.GetCarData(liIndex)->GetId() == lCarId)
        {
            return true;
        }
    }
    return false;
}

// ------------------------------------------------------------------------------------
// ProgressionManager::RepairUnlockedVehicle  @ 0x82363630
//
// Clear the just-repaired car's stored deform/damage. Asserts lCarId is non-null then delegates
// to the embedded Profile (X360: Profile::RepairUnlockedVehicle(this+0x170, lCarId)).
// ------------------------------------------------------------------------------------
void ProgressionManager::RepairUnlockedVehicle(CgsID lCarId)
{
    CGS_ASSERT(lCarId != 0, "lCarId != kCGSID_NULL");
    mProfile.RepairUnlockedVehicle(lCarId);
}

// ------------------------------------------------------------------------------------
// ProgressionManager::SetRoadRuleChallengeData  @ 0x823114A8
//
// Replace the whole 64-entry road-rules challenge-score table (X360: XMemCpy into
// Profile +100056 = mProfile.maChallengeData, 2560 bytes). Asserts the source non-null
// (the X360 fires the assert twice -- once at the wrapper, once inside the inlined Profile
// body -- which is reproduced as the wrapper assert here + the Profile-body assert in the
// delegated Profile::SetRoadRuleChallengeData).
// ------------------------------------------------------------------------------------
void ProgressionManager::SetRoadRuleChallengeData(const BrnStreetData::ChallengePlayerScoreEntry* lpaChallengeScores)
{
    CGS_ASSERT(lpaChallengeScores != nullptr, "lpaChallengeScores");
    mProfile.SetRoadRuleChallengeData(lpaChallengeScores);
}

// ------------------------------------------------------------------------------------
// ProgressionManager::SetRoadRuleNetworkHighScores  @ 0x82311430
//
// Replace the whole 64-entry road-rules network high-score table (X360: XMemCpy into
// Profile +96472 = mProfile.maNetworkChallengeData, 3584 bytes). Asserts the source non-null.
// ------------------------------------------------------------------------------------
void ProgressionManager::SetRoadRuleNetworkHighScores(const BrnStreetData::ChallengeHighScoreEntry* lpaChallengeHighScores)
{
    CGS_ASSERT(lpaChallengeHighScores != nullptr, "lpaChallengeHighScores");
    mProfile.SetRoadRuleNetworkHighScores(lpaChallengeHighScores);
}

// ------------------------------------------------------------------------------------
// ProgressionManager::GetProfile
//
// The player Profile is EMBEDDED BY VALUE at ProgressionManager+0x170 (mProfile), so the X360
// renders every GetProfile() call as a `this + 0x170` pointer adjust with no call at all --
// which is why the exports carry no symbol for it. Its callers ALWAYS null-check the result
// (`CGS_ASSERT(mProgressionManager.GetProfile())`), so the console's own contract allows a
// null answer; an embedded sub-object simply never is one.
// ------------------------------------------------------------------------------------
Profile* ProgressionManager::GetProfile()
{
    return &mProfile;
}

} // namespace BrnProgression
