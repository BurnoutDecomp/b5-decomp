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
#include "GameShared/GameClasses/Core/CgsID.h"                        // CgsIDCompress (AddCar's "CARBEAGT" test)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"            // CgsDev::Log::gpDebugPrint (the two FLAG lines)
#include "BrnProgressionCarData.h"                                    // BrnProgression::CarData (colour/palette/unlock type)
#include "SharedClasses/Progression/BrnProgressionData.h"             // BrnProgression::ProgressionData (rank count)
#include "SharedClasses/DataLists/VehicleList.h"                      // BrnResource::VehicleList (GetVehicleIndex / GetVehicleData)
#include "SharedClasses/DataLists/VehicleListEntry.h"                 // BrnResource::VehicleListEntry (livery type / parent id)

#include <string.h>   // memcpy (X360 XMemCpy)

namespace BrnProgression
{
// The verbatim X360-baked source path this TU's asserts reference.
static const char* const KAC_PROGMGR_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProgressionManager.cpp";


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

// ============================================================================================
// THE JUNKYARD / CAR-SELECT PRODUCER SURFACE (2026-08-01).
// Everything below is what CarSelectManager and GameStateModule reach for through the
// progression layer. Bodies recovered from the X360 ASM (the Hex-Rays prototypes for AddCar /
// OnPlayerCarChange render the 64-bit CgsID register arguments as one `__int64` and drop the
// rest -- see the per-body notes).
// ============================================================================================

// The loaded ProgressionData resource (X360: the ResourcePtr at this+133348). Every caller
// null-tests the answer, so the raw HasMemoryResource() test is used rather than operator->
// (whose own assert would pre-empt the caller's).
const ProgressionData* ProgressionManager::GetProgressionData() const
{
    if (!mpProgressionData.HasMemoryResource())
    {
        return 0;
    }
    return mpProgressionData.operator->();
}

// The CarData record for the car the player is currently in (X360 this+133328).
CarData* ProgressionManager::GetCurrentCarData()
{
    return mpCurrentCarData;
}

// X360 this+133448 -- the loaded vehicle list. See the header FLAG on the install site.
void ProgressionManager::SetVehicleList(const BrnResource::VehicleList* lpVehicleList)
{
    mpVehicleList = lpVehicleList;
}

// --------------------------------------------------------------------------------------------
// GetProgressionRank (X360 0x823701D8).
// The cached rank byte at this+133484, CLAMPED into the loaded rank table:
//   * read UNSIGNED and compared `>= 0x80` -- i.e. any signed-negative cache (the Profile seeds
//     -2 for "not started") answers rank 0 without touching the resource;
//   * otherwise `min(rank, rankCount - 1)` against ProgressionData::muProgressionRankCount (+0x14).
// --------------------------------------------------------------------------------------------
s32 ProgressionManager::GetProgressionRank() const
{
    if (static_cast<u8>(mi8ProgressionRank) >= 0x80u)
    {
        return 0;
    }

    const s32 liRank      = static_cast<s32>(static_cast<u8>(mi8ProgressionRank));
    const s32 liRankCount = static_cast<s32>(mpProgressionData->GetProgressionRankCount());
    if (liRank < liRankCount)
    {
        return liRank;
    }
    return liRankCount - 1;
}

// --------------------------------------------------------------------------------------------
// AddCar (X360 0x8237A970).
// Hand the car to the profile, then two tallies and the derived-("silver")-car fan-out.
// ARG SHAPE FROM ASM: r3=this, r4=carId, r5=unlockType. (ProgressionManager::OnPlayerCarChange
// @0x8237AC38 calls it as `li r5,0` -> unlock type E_UNLOCK_TYPE_UNLOCK.)
//
// ⛔ HONEST PARTIAL -- the derived-car leg. When the profile's mbGoldCarsUnlocked flag is set the
// console builds the car's colour-livery list (BrnProgression::DerivedCarArray::
// ConstructColourLiveryList @0x82374F60), walks it, and for every entry whose livery kind == 4 it
// adds that derived car to the profile too and marks its unlock sequence already-shown. That
// whole path needs BrnDerivedCars.h (DerivedCarArray + ConstructColourLiveryList +
// UnlockDerivedCarCollection + DEBUG_PrintArray, ~600 X360 instructions), which is NOT
// reconstructed. It is gated on mbGoldCarsUnlocked, which a fresh profile leaves FALSE, so it is
// off the start-of-game path entirely -- and it announces itself in the log when it is hit.
// DELETE-WHEN BrnDerivedCars.h lands.
// --------------------------------------------------------------------------------------------
CarData* ProgressionManager::AddCar(CgsID lCarId, s32 leUnlockType)
{
    CarData* lpCarData = mProfile.AddCar(lCarId, static_cast<CarData::UnlockType>(leUnlockType));
    if (lpCarData == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpCarData != NULL", KAC_PROGMGR_FILE, 536);
        CgsDev::Assert::EndAssert();
    }

    // X360: `if (a3 == 5) ++*(this + 133468);` -- E_UNLOCK_TYPE_SPONSOR.
    if (leUnlockType == CarData::E_UNLOCK_TYPE_SPONSOR)
    {
        ++miSponsorCarCount;
    }
    // X360: `if (carId == CgsIDCompress("CARBEAGT")) ++*(this + 133468);` -- the same counter is
    // bumped a second time for that one car id. (CgsIDCompress is constant-folded by the console
    // compiler; the packed literal is reproduced by the shared helper.)
    if (lCarId == CgsIDCompress("CARBEAGT"))
    {
        ++miSponsorCarCount;
    }

    if (mProfile.GetGoldCarsUnlocked())
    {
        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[FLAG PC bring-up] ProgressionManager::AddCar: the derived-("
                   "silver)-car fan-out is NOT reconstructed (needs BrnDerivedCars.h -- "
                   "DerivedCarArray::ConstructColourLiveryList @0x82374F60). Car "
                << static_cast<u32>(lCarId) << " was added, its derived variants were not.\n";
        }
    }

    return lpCarData;
}

// --------------------------------------------------------------------------------------------
// OnPlayerCarChange (X360 0x8237AC38).
// ARG SHAPE FROM ASM: r3=this, r4=carId, r5=wheelId, r6=the bool.
//   lbUpdateProfile == false -> just clear the cached current-car record and return.
//   lbUpdateProfile == true  -> persist the chosen car+wheel onto the profile's spawn slots,
//     make sure the car is owned (adding it -- and marking its unlock sequence already-shown --
//     when it is not), then cache the chosen-livery record for the car's BASE id (a livery
//     variant, entry+0xE9 in {1,3,4}, resolves through GetParentId()).
// --------------------------------------------------------------------------------------------
void ProgressionManager::OnPlayerCarChange(CgsID lCarId, CgsID lWheelId, bool lbUpdateProfile)
{
    if (!lbUpdateProfile)
    {
        mpCurrentCarData = 0;   // X360 `stwx r10(0), this, 0x208D0`
        return;
    }

    // X360 `std r29, 0x1C0(this)` / `std r5, 0x1C8(this)` == mProfile.mSpawnCarId / mSpawnWheelId
    // (mProfile is embedded at this+0x170, so +0x1C0/+0x1C8 are Profile+80/+88).
    mProfile.SetSpawnCarId(lCarId);
    mProfile.SetSpawnWheelId(lWheelId);

    mpCurrentCarData = mProfile.FindCar(lCarId);
    if (mpCurrentCarData == 0)
    {
        mpCurrentCarData = AddCar(lCarId, CarData::E_UNLOCK_TYPE_UNLOCK);
        mProfile.SetCarUnlockAlreadyShown(lCarId);
    }

    const BrnResource::VehicleList* lpVehicleList = mpVehicleList;
    const s32 liVehicleIndex = (lpVehicleList != 0) ? lpVehicleList->GetVehicleIndex(lCarId) : -1;
    const BrnResource::VehicleListEntry* lpVehicleListEntry =
        (liVehicleIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liVehicleIndex);

    if (lpVehicleListEntry == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpVehicleListEntry", KAC_PROGMGR_FILE, 1258);
        CgsDev::Assert::EndAssert();
        return;   // the X360 falls through into a null deref; bail instead of faulting
    }

    const u8 luLiveryType = lpVehicleListEntry->GetLiveryType();
    const CgsID lBaseCarId =
        (luLiveryType == 1 || luLiveryType == 3 || luLiveryType == 4)
            ? lpVehicleListEntry->GetParentId()
            : lCarId;

    mpCurrentLiveryData = mProfile.GetChosenLiveryDataForBaseCar(lBaseCarId);
}

// --------------------------------------------------------------------------------------------
// GetCarColourAndPalette (X360 0x8237C0D8).
// Answer lCarId's colour + palette indices. The profile's own CarData record wins; the 0xFF
// "unset" sentinel in either field falls back to the car's authored defaults out of its
// VehicleListEntry (`BYTE2 / LOBYTE` of the word at entry+0xEC).
// --------------------------------------------------------------------------------------------
void ProgressionManager::GetCarColourAndPalette(CgsID lCarId, s32* lpiColour, s32* lpiPalette)
{
    // X360: a linear walk of mProfile.maCars[0 .. miCarCount) comparing the record's id.
    const CarData* lpCarData = 0;
    const s32 liCarCount = mProfile.GetCarCount();
    for (s32 liCar = 0; liCar < liCarCount; ++liCar)
    {
        const CarData* lpCandidate = mProfile.GetCarData(liCar);
        if (lpCandidate != 0 && lpCandidate->GetId() == lCarId)
        {
            lpCarData = lpCandidate;
            break;
        }
    }

    *lpiColour  = 255;
    *lpiPalette = 255;
    if (lpCarData != 0)
    {
        *lpiColour  = lpCarData->mu8ColourIndex;
        *lpiPalette = lpCarData->mu8PaletteIndex;
    }

    if (*lpiColour != 255 && *lpiPalette != 255)
    {
        return;
    }

    const BrnResource::VehicleList* lpVehicleList = mpVehicleList;
    const s32 liVehicleIndex = (lpVehicleList != 0) ? lpVehicleList->GetVehicleIndex(lCarId) : -1;
    const BrnResource::VehicleListEntry* lpVehicleListEntry =
        (liVehicleIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liVehicleIndex);

    if (lpVehicleListEntry == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpVehicleListEntry", KAC_PROGMGR_FILE, 4500);
        CgsDev::Assert::EndAssert();
        return;   // the X360 falls through into a null deref; bail instead of faulting
    }

    // ⚠️ FLAG: the authored default colour/palette are the two bytes at entry+0xEE / +0xEF, inside
    // VehicleListEntry's trailing maPad224 span (X360 `lbz r11,0xEE` -> colour out,
    // `lbz r11,0xEF` -> palette out). Not named fields yet -- DELETE-WHEN the VehicleListEntry
    // gameplay tail is homed.
    const u8* lpcEntryBytes = reinterpret_cast<const u8*>(lpVehicleListEntry);
    *lpiColour  = lpcEntryBytes[0xEE];
    *lpiPalette = lpcEntryBytes[0xEF];
}

// The two one-byte request flags CarSelectManager::UpdateExitState sets on junkyard exit
// (X360 `stbx r25(1), mpProgressionManager, 0x20971` and `... 0x20988`).
void ProgressionManager::RequestUpdateRivals()
{
    mbUpdateRivalsRequested = true;
}

void ProgressionManager::SetDriveThrusDirtyFlag()
{
    mbDriveThrusDirty = true;
}

} // namespace BrnProgression
