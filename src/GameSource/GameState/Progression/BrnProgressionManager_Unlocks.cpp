// ===================================================================================
// BrnProgression::ProgressionManager -- THE TROPHY / SPECIAL-CAR UNLOCK WRITERS.
//   GameSource/Unity/../GameState/Progression/BrnProgressionManager.cpp  (per-function
//   partfile of that TU, the house BrnProgressionManager_EventFinish.cpp precedent; the
//   console homes all three functions in BrnProgressionManager.cpp, which is why every
//   assert below is fired with that file's baked path + the console's own line number
//   rather than through CGS_ASSERT's __FILE__/__LINE__.)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   ProgressionManager::UnlockSpecialCars    @0x8237AF38  (107 instructions)
//   ProgressionManager::UnlockCarFromTrophy  @0x8237B0E8  (57 instructions)
//   ProgressionManager::OnTrophyUnlock       @0x82389740  (500 instructions)
//
// [drive-thru link-closure wave, 2026-08-27] These three were PARKED in the header on
// 2026-08-20 as "not small" and blocked on a missing ProgressionData trophy-table header.
// Two thirds of that park had gone stale: the table ROOT was already modelled
// (BrnProgressionData.h, GetTrophyUnlock @0x823569F0 bodied) and the "four unmodelled
// manager/Profile fields" were all modelled Profile members reached through the manager
// (+776/+780/+482 == Profile+408/+412/+114). What was genuinely missing was the table's
// ELEMENT TYPE -- see the ⚠️⚠️ note in SharedClasses/Progression/BrnTrophyUnlockData.h,
// which this wave homed.
//
// ⚠️ THE ASM IS THE SPINE. Hex-Rays mis-renders three things here and each is called out
// at its site:
//   (a) `UnlockCarFromTrophy(__SPAIR64__(a1, v12), a2)` -- it fused r3:r4 into one 64-bit
//       argument. The call is (this, CgsID, unlockType) in r3/r4/r5.
//   (b) the profile car scans render as `*(v6 + 4) != a1`; the asm is `ld r7, 0(r10)`, a
//       full 64-bit compare at offset ZERO (the same register-pair confusion).
//   (c) `if (&v8[6 * v9] != -640)` / `if (24 * v4 + ... == -640)` are the compiler's
//       always-true null checks on `&array[i]`, not real comparisons.
//
// Every member is reached BY NAME through BrnProgressionManager.h / BrnProfile.h; no raw
// offset arithmetic on `this`.
// ===================================================================================

#include "GameSource/GameState/Progression/BrnProgressionManager.h"

#include "GameSource/GameState/Progression/BrnProfile.h"            // BrnProgression::Profile / CarData
#include "GameSource/GameState/Progression/BrnProgressionCarData.h" // BrnProgression::CarData (UnlockType, SetUnlockDeformationAmount)
#include "SharedClasses/Progression/BrnProgressionData.h"           // BrnProgression::ProgressionData (trophy table root)
#include "SharedClasses/Progression/BrnTrophyUnlockData.h"          // BrnProgression::TrophyUnlockData (the 16-byte element)
#include "SharedClasses/DataLists/VehicleList.h"                    // BrnResource::VehicleList
#include "SharedClasses/DataLists/VehicleListEntry.h"               // BrnResource::VehicleListEntry (GetId/GetParentId/GetLiveryType)
#include "GameSource/GameState/BrnGameActions.h"                    // GameStateModuleIO::TrophyUnlockAction
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CgsDev::Assert::Begin/Fire/End
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h" // OnFindAllDriveThrus
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"    // VariableEventQueue<13312,16>::AddEvent

namespace BrnProgression
{

namespace GsmIO = BrnGameState::GameStateModuleIO;

// The console's baked assert path for BrnProgressionManager.cpp (this partfile's console home).
static const char* const KAC_UNLOCKS_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProgressionManager.cpp";

// flt_82001CC0 == 0.0f -- the "this car has never been deformed" comparand in UnlockSpecialCars.
static const f32 KF_NO_UNLOCK_DEFORMATION = 0.0f;
// flt_82029BB8 == 0.85f -- the deform a trophy car is presented with in the unlock sequence.
// (The same literal ReallyEnterJunkyardAtStartOfGame @0x823931F8 writes as a bare
// `*(carData + 12) = 0.85f`; both read the same .rdata word.)
static const f32 KF_TROPHY_CAR_UNLOCK_DEFORMATION = 0.85f;

// ===================================================================================
// ProgressionManager::UnlockCarFromTrophy  @ 0x8237B0E8
//
// Award the car a trophy unlocks. Returns false -- doing NOTHING -- when the profile
// already owns it, which is what stops OnTrophyUnlock's table walk from re-awarding the
// same car every time a tally is re-evaluated, and is why OnTrophyUnlock stops its walk
// on a true.
//
// ⚠️ ARG SHAPE FROM ASM (0x8237B0F4..0x8237B100): `mr r29, r3` / `mr r30, r4` /
// `mr r28, r5` -- this, the 64-bit CgsID, the unlock type. Hex-Rays' `(__int64 a1, int a2)`
// is r3:r4 fused into one value; there is no doubleword first parameter.
// ===================================================================================
bool ProgressionManager::UnlockCarFromTrophy(CgsID lCarId, s32 liTrophyType)
{
    // `lwz r9, 0x26C(r8)` + the `addi r10, r8, 0x280` / `ld r7, 0(r10)` / `cmpld` / stride
    // 0x18 walk is Profile::FindCar open-coded. Already owning the car ends the call.
    if (mProfile.FindCar(lCarId) != 0)
    {
        return false;
    }

    // `li r5, 2` == CarData::E_UNLOCK_TYPE_TROPHY.
    CarData* lpCarData = AddCar(lCarId, CarData::E_UNLOCK_TYPE_TROPHY);
    if (lpCarData == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpCarData != NULL", KAC_UNLOCKS_FILE, 2322);
        CgsDev::Assert::EndAssert();
    }

    // `stfs f0, 0xC(r31)` with f0 == flt_82029BB8.
    lpCarData->SetUnlockDeformationAmount(KF_TROPHY_CAR_UNLOCK_DEFORMATION);

    // The 16-byte record the console builds at var_40 and hands to Append:
    //   std r30, 0x90+var_40(r1)   -> rec+0x00, the CgsID
    //   stw r28, 0x90+var_38(r1)   -> rec+0x08, the unlock type
    // (That store pair is half the proof that TrophyUnlockAction's members are CgsID-first;
    //  the other half is SendTrophyUnlockUpdate's two named asserts. See BrnGameActions.h.)
    BrnGameState::GameStateModuleIO::TrophyUnlockAction lUnlock;
    lUnlock.mCarToUnlock = lCarId;
    lUnlock.meUnlockType = static_cast<TrophyUnlockData::UnlockType>(liTrophyType);
    mQueueOfTrophyCarUnLocks.Append(lUnlock);

    return true;
}

// ===================================================================================
// ProgressionManager::UnlockSpecialCars  @ 0x8237AF38
//
// Walk the loaded vehicle list; for every entry whose LIVERY TYPE (VehicleListEntry+0xE9,
// `lbz r11, 0xE9(r29)`) equals lu8LiveryType, award that entry's car IF the profile
// already owns its PARENT car -- the special/derived cars are variants of cars you own.
// CheckForSpecialCarUnlocks calls it with 4 (rank-gated set) and 3 (100%-gated set).
//
// ⚠️ The parent-owned test is not decoration: the `beq`/`b loc_8237B0C4` pair at
// 0x8237AFEC/0x8237B000 means an entry whose parent is NOT in the profile is skipped
// entirely -- no AddCar, no livery choice.
// ===================================================================================
void ProgressionManager::UnlockSpecialCars(u8 lu8LiveryType)
{
    // `lwz r3, 0(r22)` / `lwz r11, 0x3400(r3)` -- the bound is re-read from the list every
    // iteration (0x8237B0C4), not cached.
    for (s32 liIndex = 0; liIndex < mpVehicleList->GetVehicleCount(); ++liIndex)
    {
        const BrnResource::VehicleListEntry* lpVehicleListEntry =
            mpVehicleList->GetVehicleData(liIndex);
        if (lpVehicleListEntry == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpVehicleListEntry != NULL", KAC_UNLOCKS_FILE, 2117);
            CgsDev::Assert::EndAssert();
        }

        if (lpVehicleListEntry->GetLiveryType() != lu8LiveryType)
        {
            continue;
        }

        // `ld r8, 8(r29)` -- the PARENT car's id, scanned for in the profile.
        const CgsID lParentCarId  = lpVehicleListEntry->GetParentId();
        const CgsID lDerivedCarId = lpVehicleListEntry->GetId();

        if (mProfile.FindCar(lParentCarId) == 0)
        {
            continue;   // parent not owned -> this variant is not awarded
        }

        // `li r5, 4` == CarData::E_UNLOCK_TYPE_GOLD_SILVER, for BOTH livery types --
        // the console passes the unlock KIND, not the livery type, to AddCar.
        CarData* lpCarData = AddCar(lDerivedCarId, CarData::E_UNLOCK_TYPE_GOLD_SILVER);
        if (lpCarData == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpCarData != NULL", KAC_UNLOCKS_FILE, 2124);
            CgsDev::Assert::EndAssert();
        }

        // `stb r26, 0xA(r31)` with r26 == 1 -- special cars skip the unlock cinematic.
        lpCarData->SetUnlockSequenceAlreadyShown();

        // The console re-runs the parent scan (0x8237B058..0x8237B0A8) rather than reusing
        // the first result, then makes the new variant the chosen livery for the base car
        // UNLESS the parent still carries an unlock deformation. `lfs f0, 0xC(r11)` /
        // `fcmpu f0, f31` with f31 == flt_82001CC0 == 0.0f: a parent that is still sitting
        // in its own unlock sequence keeps its livery until that finishes.
        const CarData* lpParentCarData = mProfile.FindCar(lParentCarId);
        if (lpParentCarData == 0 ||
            lpParentCarData->GetUnlockDeformationAmount() == KF_NO_UNLOCK_DEFORMATION)
        {
            mProfile.SetChosenLiveryIdForBaseCar(lParentCarId, lDerivedCarId);
        }
    }
}

// ===================================================================================
// ProgressionManager::OnTrophyUnlock  @ 0x82389740
//
// THE TROPHY-CONDITION MACHINE. Walk PROGRESSION.DAT's trophy-unlock table; for every
// record whose type equals liTrophyType, test that type's own tally against the record's
// threshold, and on the first record that both matches and passes, award its car and stop.
//
// SHAPE OF THE WALK (asm 0x8238982C..0x82389F34):
//   * the bound is muTrophyUnlockCount, cached once at 0x823897A0 into var_188 and reloaded
//     at the tail (0x82389F14); the record pointer is `table_base + 16 * index` with the
//     byte offset carried in var_1A0 and advanced by 16;
//   * every iteration re-fetches the ProgressionData through the resource pointer and
//     re-fires the bound assert -- that is GetTrophyUnlock @0x823569F0 inlined, which is
//     why the assert's baked location is BrnProgressionData.h:302 and not this file;
//   * a record whose type does not match is skipped WITHOUT touching the unlocked flag
//     (`bne cr6, loc_82389F18` at 0x8238989C jumps past the test).
//
// ⭐ A ZERO THRESHOLD MEANS "ALREADY EARNED", NOT "NEVER". `cmplwi cr6, r25, 0` /
// `bne loc_82389CD8` at 0x823898A8: only a NON-zero muNumberTrophyUnlock reaches the
// tally switch. A zero one falls through the per-type "we need a number" assert chain to
// LABEL_37, which sets the flag TRUE. That is how types 1..21 (the "complete/find all X"
// trophies, whose condition the CALLER has already decided -- e.g. OnDriveThru's
// AreAllDriveThrusCompleted) get awarded: they carry no number, and the asserts exist only
// to catch a data author leaving a 0 in one of the 22..34 "reach N of these" rows.
// ⚠️ This is the [[placeholder-identity-element]] shape inverted -- here 0 is not inert and
// not "immediately never"; it is "immediately YES". Do not "guard" it.
// ===================================================================================
void ProgressionManager::OnTrophyUnlock(s32 liTrophyType)
{
    // `GetTotalWinCount(&var_1A0, &var_198, &var_19C)` then `add r11, r11, r10` over the
    // FIRST and THIRD out-params only -- rank wins + special-event wins. The middle
    // (non-rank) count is deliberately not in the medal total.
    u32 luRankWins         = 0;
    u32 luNonRankWins      = 0;
    u32 luSpecialEventWins = 0;
    mProfile.GetTotalWinCount(luRankWins, luNonRankWins, luSpecialEventWins);
    const u32 luTotalMedals = luRankWins + luSpecialEventWins;

    const ProgressionData* lpProgressionData = GetProgressionData();
    if (lpProgressionData == 0)
    {
        // The console reaches the table through ResourcePtr<ProgressionData>::operator->,
        // which asserts internally; there is no null test in this body. Guarding here rather
        // than dereferencing null is a PC bring-up difference and nothing else -- everything
        // below is a pure read of a resource that Prepare2's loader binds.
        return;
    }

    const u32 luTrophyUnlockCount = lpProgressionData->GetTrophyUnlockCount();
    bool      lbConditionMet      = false;

    for (u32 luIndex = 0; luIndex < luTrophyUnlockCount; ++luIndex)
    {
        // GetTrophyUnlock inlined: the bound assert is BrnProgressionData.h:302, the
        // console's own baked location for it.
        const TrophyUnlockData* lpCurrentTrophyUnlockData = lpProgressionData->GetTrophyUnlock(luIndex);
        if (lpCurrentTrophyUnlockData == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpCurrentTrophyUnlockData != NULL", KAC_UNLOCKS_FILE, 2162);
            CgsDev::Assert::EndAssert();
        }

        // `lhz r23, 4(r24)` then `cmpw` against the argument -- a halfword field widened for
        // the compare, which is why the record's type is a u16 and not the enum's int.
        const s32 liRecordType = static_cast<s32>(lpCurrentTrophyUnlockData->GetUnlockType());
        if (liRecordType != liTrophyType)
        {
            continue;
        }

        const u32   luNumberRequired = lpCurrentTrophyUnlockData->GetNumberForTrophyUnlock();
        const CgsID lCarUnlockId     = lpCurrentTrophyUnlockData->GetCarUnlockID();

        if (luNumberRequired == 0)
        {
            // The data-authoring guard chain. Each arm is the console's own line number; all
            // of them fall through to "condition met", asserts or not.
            switch (liTrophyType)
            {
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_MEDELS:
                    CGS_ASSERT(false, "We need a number of medals for this\n");                  // :2174
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_CRASH_ROADRULES:
                    CGS_ASSERT(false, "We need a number of Road rules for this\n");              // :2175
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_ROADRULES:
                    CGS_ASSERT(false, "We need a number of Road rules for this\n");              // :2176
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_TIME_ROADRULES:
                    CGS_ASSERT(false, "We need a number of Road rules for this\n");              // :2177
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_NORMALTAKEDOWNS:
                    CGS_ASSERT(false, "We need a number that is not 0\n");                       // :2178
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_SIGNATURETAKEDOWNS:
                    CGS_ASSERT(false, "We need a number that is not 0\n");                       // :2179
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_JUMPS:
                    CGS_ASSERT(false, "We need a number that is not 0\n");                       // :2180
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_SMASHES:
                    CGS_ASSERT(false, "We need a number that is not 0\n");                       // :2181
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_STUNTS:
                    CGS_ASSERT(false, "We need a number that is not 0\n");                       // :2184
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_ONLINE_VERTICLE_TAKEDOWNS:
                    CGS_ASSERT(false, "We need a number that is not 0\n");                       // :2185
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_PERCENTAGE_PARALLELPARK_ONLINE:
                    CGS_ASSERT(false, "We need a number that is not 0\n");                       // :2186
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_OF_EACH_ONLINE_EVENT_COMPLETE:
                    CGS_ASSERT(false, "We need a number that is not 0\n");                       // :2187
                    break;
                default:
                    // Types 1..21 (and 34) reach here with no assert: they carry no number by
                    // design. The console's switch has no default arm and simply falls through.
                    break;
            }
            lbConditionMet = true;
        }
        else
        {
            // The `addi r11, r23, -0x16 / cmplwi r11, 0xC` jump table @0x82389CD8 -- exactly
            // types 22..34, with 27 falling to the default (unknown) arm. Every comparison is
            // the compiler's unsigned `>=` idiom (`subfc/subfe/addi r22, r11, 1`) except where
            // noted.
            switch (liTrophyType)
            {
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_MEDELS:
                    lbConditionMet = (luTotalMedals >= luNumberRequired);
                    break;

                // The three road-rule tallies are the MANAGER's, not the profile's
                // (manager+133464/+133460/+133456).
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_ROADRULES:
                    lbConditionMet = (static_cast<u32>(miNumberOfNumberOfCompleteRoadRulesRuledByPlayer)
                                          >= luNumberRequired);
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_TIME_ROADRULES:
                    lbConditionMet = (static_cast<u32>(miNumberOfParTimeRoadRulesRuledByPlayer)
                                          >= luNumberRequired);
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_CRASH_ROADRULES:
                    lbConditionMet = (static_cast<u32>(miNumberOfParCrashRoadRulesRuledByPlayer)
                                          >= luNumberRequired);
                    break;

                case TrophyUnlockData::E_UNLOCKTYPE_NUM_NORMALTAKEDOWNS:
                    lbConditionMet = (static_cast<u32>(mProfile.GetTotalTakedownCount())
                                          >= luNumberRequired);
                    break;

                // ⛔⛔ THE COMPARISON IS THE OTHER WAY ROUND, AND THAT IS THE BINARY, NOT A
                // TYPO. Case 31 is the ONLY arm whose `subfc` operands are swapped:
                //     0x82389E4C  lwz   r11, 0x30C(r19)      ; miTotalOnlineVerticleTakedownCount
                //     0x82389E50  subfc r11, r11, r25        ; r25 - r11   <-- reversed
                //     0x82389E54  subfe r11, r11, r11
                //     0x82389E58  addi  r22, r11, 1          ; r22 = (threshold >= tally)
                // Every other arm is `subfc r11, r25, r11` == (tally >= threshold). So this
                // trophy is awarded while the player has FEWER vertical takedowns than the
                // requirement and stops being awarded once they pass it -- i.e. it is
                // effectively always true at 0 takedowns. Reproduced verbatim; it looks like an
                // original-source slip and is exactly the kind of thing a "tidy-up" would
                // silently correct. Do not flip it without new evidence.
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_ONLINE_VERTICLE_TAKEDOWNS:
                    lbConditionMet = (luNumberRequired
                                          >= static_cast<u32>(mProfile.GetTotalOnlineVerticleTakedownCount()));
                    break;

                case TrophyUnlockData::E_UNLOCKTYPE_NUM_JUMPS:
                    lbConditionMet = (static_cast<u32>(mProfile.GetStuntElementCount(
                                          BrnGameState::E_STUNT_ELEMENT_TYPE_JUMP)) >= luNumberRequired);
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_SMASHES:
                    lbConditionMet = (static_cast<u32>(mProfile.GetStuntElementCount(
                                          BrnGameState::E_STUNT_ELEMENT_TYPE_SMASH)) >= luNumberRequired);
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_STUNTS:
                    lbConditionMet = (static_cast<u32>(mProfile.GetStuntElementCount(
                                          BrnGameState::E_STUNT_ELEMENT_TYPE_BILLBOARD)) >= luNumberRequired);
                    break;

                // `lbz r11, 0x1E2(r19)` + `extsb` -- a SIGNED byte, compared through the same
                // unsigned idiom, so a negative rating wraps huge and passes. Reproduced as the
                // sign-extended value re-read as unsigned rather than "cleaned up".
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_PERCENTAGE_PARALLELPARK_ONLINE:
                    lbConditionMet = (static_cast<u32>(static_cast<s32>(
                                          mProfile.GetPowerParkingBetweenOtherPlayersBestRating()))
                                      >= luNumberRequired);
                    break;

                // ⭐ THIS ARM IGNORES THE THRESHOLD ENTIRELY. `lwz 0x130/0x134/0x13C(r16)`
                // against the literal 5 (`cmpwi cr6, r11, 5`), on the PROFILE not the manager:
                // Profile+304/+308/+316 == maGameModeTypeAmountCompleted[10]/[11]/[13], the
                // three online modes E_MODE_ONLINE_RACE, E_MODE_ONLINE_ROAD_RAGE and
                // E_MODE_ONLINE_BURNING_HOME_RUN. Index 12 (E_MODE_ONLINE_FUGITIVE) is NOT
                // tested -- the console reads +0x134 then jumps to +0x13C.
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_OF_EACH_ONLINE_EVENT_COMPLETE:
                    lbConditionMet =
                        mProfile.GetGameModeTypeCompleted(GsmIO::E_MODE_ONLINE_RACE)             >= 5 &&
                        mProfile.GetGameModeTypeCompleted(GsmIO::E_MODE_ONLINE_ROAD_RAGE)        >= 5 &&
                        mProfile.GetGameModeTypeCompleted(GsmIO::E_MODE_ONLINE_BURNING_HOME_RUN) >= 5;
                    break;

                case TrophyUnlockData::E_UNLOCKTYPE_NUM_MUG_SHOTS_COLLECTED:
                    lbConditionMet = (static_cast<u32>(mProfile.GetNumAllMugshots()) >= luNumberRequired);
                    break;

                default:
                    // Includes E_UNLOCKTYPE_NUM_SIGNATURETAKEDOWNS (27), which the jump table
                    // routes to the default arm -- it has no tally in this build.
                    CGS_ASSERT(false, "Unknown trophy unlock");                                  // :2270
                    break;
            }
        }

        if (lbConditionMet)
        {
            if (UnlockCarFromTrophy(lCarUnlockId, liTrophyType))
            {
                return;   // `bne cr6, loc_82389F38` -- the walk stops at the first award
            }
        }
    }
}


// ===================================================================================
// ProgressionManager::OnDriveThru  @ 0x82399DD0   ⭐⭐⭐ THE LAST UNRESOLVED EXTERNAL
//
// Record the discovery of a drive-thru and fan out everything that follows from it.
// DriveThruManager::HandleDriveThru is its only caller, and that path is HOT: a
// __debugbreak() left in this symbol's place broke into the debugger ~9.5 s into an
// ordinary junkyard -> car-select -> free-burn drive, via ProcessPlayerTriggers.
//
// ⚠️ ARG SHAPE FROM ASM (0x82399DDC..0x82399DF0): r3=this, r4=the drive-thru id ->r30,
// r5=the region type ->r29, r6=the game-action queue ->r27, and r31 = this+0x170, the
// embedded Profile. The Profile calls that "take no arguments" in the pseudocode
// (IsDriveThruDiscoverd) are the compiler leaving r4/r5 untouched from the prologue --
// nothing clobbers them between `mr r30, r4` and the call, which is why only `mr r3, r31`
// is emitted.
//
// ⭐ THE WHOLE CASCADE IS GATED, AND ONLY THE FIRST GATE IS COMMON. A re-entered
// drive-thru returns immediately (IsDriveThruDiscoverd). A newly discovered one runs
// AddDriveThru; only when THAT awards a trophy does OnTrophyUnlock fire; and the
// completion pair (CheckForSpecialCarUnlocks + SendGameCompletionResults) needs every
// drive-thru in the world found. The unconditional part is the action-55 autosave post
// and the dirty flag.
//
// ⚠️ THE AUTOSAVE POST CARRIES A ZERO PAYLOAD (`li r11, 0` / `stb r11, var_40` before
// `li r5, 0x37`), i.e. an UNFORCED autosave: GuiModule ORs that byte into
// mbForceProfileAutosave, so a zero leaves the 60-second throttle in charge. The
// completion path's own request (mbAutosaveRequested, drained by PreWorldUpdate) posts
// the same action with a payload of ONE and does bypass it. Same id, different urgency.
// ===================================================================================
void ProgressionManager::OnDriveThru(CgsID lId, BrnTrigger::GenericRegion::Type leType,
                                     BrnGameState::GameStateModuleIO::GameActionQueue* lpQueue)
{
    if (mProfile.IsDriveThruDiscoverd(lId, leType))
    {
        return;
    }

    // AddDriveThru returns the TrophyUnlockData::UnlockType this discovery completed, or
    // E_UNLOCKTYPE_NONE (0) when the category is not finished yet.
    const s32 liTrophyType = mProfile.AddDriveThru(lId, leType);
    if (liTrophyType != TrophyUnlockData::E_UNLOCKTYPE_NONE)
    {
        OnTrophyUnlock(liTrophyType);

        // `li r4, 0x15` == 21 == E_UNLOCKTYPE_FIND_ALL_DRIVE_THRUS -- the umbrella trophy on
        // top of the per-category one, awarded only when this was the last category to close.
        if (mProfile.AreAllDriveThrusCompleted())
        {
            OnTrophyUnlock(TrophyUnlockData::E_UNLOCKTYPE_FIND_ALL_DRIVE_THRUS);
        }
    }

    if (mProfile.AreAllDriveThrusCompleted())
    {
        // `lwzx r3, r28, 0x20938` -- the achievement manager, dereferenced with no null test
        // by the console. Guarded here for the same reason every other consumer in this tree
        // guards it: nothing in the mounted set calls Prepare2, so it reads NULL today.
        BrnGameState::AchievementManagerBase* lpAchievementManager = GetAchievementManager();
        if (lpAchievementManager != 0)
        {
            lpAchievementManager->OnFindAllDriveThrus();
        }
    }

    if (lpQueue != 0)
    {
        if (mProfile.AreAllDriveThrusCompleted())
        {
            CheckForSpecialCarUnlocks();
            SendGameCompletionResults(lpQueue);
        }

        u8 lu8AutosaveIsForced = 0;
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lu8AutosaveIsForced),
                          BrnGameState::GameStateModuleIO::E_ACTION_REQUEST_AUTOSAVE, 1);
    }

    // `stbx r10(1), r28, 0x20988` == manager+133512, unconditional on the not-yet-discovered
    // path (it sits AFTER the `if (lpQueue)` block, not inside it).
    mbDriveThrusDirty = true;
}

}
