// ===================================================================================
// BrnProgression::Profile  -- the player's persisted progression record.
//   GameSource/Unity/../GameState/Progression/BrnProfile.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the ASM is the spine; offsets/constants are
// proven there) + the DecFIGS DWARF (member names/types/order) + the LegacySrc idiom. The
// full byte-exact Profile layout lives in BrnProfile.h; every member here is reached BY NAME.
// ===================================================================================

#include "BrnProfile.h"
#include "GameSource/Gui/SaveLoad/BrnGuiSaveLoadProfileDLC1.h"   // BrnGuiSaveLoad::ProfileDLC1::IsDLCCarId (SplitArray DLC test)
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/System/Timer/PS3/CgsDateAndTimePS3.h"
#include "GameShared/GameClasses/Network/Texture/CgsNetworkTexture.h"
#include "GameShared/GameClasses/Network/Utilities/CgsNetworkImageConverter.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint ([profile] selftest witness)

#include <string.h>   // memset / memcpy
#include <stdlib.h>   // getenv (the [profile] selftest gate)

static const char KAC_ARTIST_ASSERT_FILE[] = "..\\..\\..\\GameSource\\GameState/Progression/BrnProfile.h";
static const char KAC_ARTIST_ASSERT_SOURCE[] =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProfile.cpp";

namespace BrnProgression
{

// The alias the original source (and every assert string) uses for the game-state IO namespace.
namespace GsmIO = BrnGameState::GameStateModuleIO;

// ------------------------------------------------------------------------------------
// ProfileEvent -- trivial record accessors (the X360 inlines all of these at their call
// sites; the bodies are attested by the open-coded forms: AddEvent's id/flags stores,
// DEBUG_ClearMedals' flag clear, and the GetMedalAchievedForEventWithID /
// GetTotalWinCount bit tests on the u16 flag word @ +4).
// ------------------------------------------------------------------------------------
void ProfileEvent::Construct(u32 luEventID)
{
    muEventID = luEventID;
    muFlags   = 0;
}

u32 ProfileEvent::GetID() const
{
    return muEventID;
}

u16 ProfileEvent::GetFlags() const
{
    return muFlags;
}

void ProfileEvent::SetFlags(u16 lu16Flags)
{
    muFlags = lu16Flags;
}

bool ProfileEvent::IsFlagSet(Flags leFlag) const
{
    return (muFlags & leFlag) != 0;
}

// EnableFlags / ClearFlags -- DWARF BrnProfile.h:328 / :332. Both are inlined at every console
// site: the discovery arm of GameStateModule::CheckIfPlayerIsAtJunctionWithAnEvent open-codes
// EnableFlags(E_FLAG_DISCOVERED) as `lhz r11, 4(r30) / ori r11, r11, 1 / sth r11, 4(r30)`
// (0x82390750..0x82390760), and ProgressionDebugComponent::DEBUG_ClearMedals @0x8235A168 uses
// the andi. form. Declared here since the odometer/drive-thru waves; bodied now that the
// discovery arm has a caller.
void ProfileEvent::EnableFlags(u16 lu16Flags)
{
    muFlags = static_cast<u16>(muFlags | lu16Flags);
}

void ProfileEvent::ClearFlags(u16 lu16Flags)
{
    muFlags = static_cast<u16>(muFlags & static_cast<u16>(~lu16Flags));
}

// ------------------------------------------------------------------------------------
// CarData::Construct -- initialise one owned-car record (X360 inlines this in AddCar).
// ------------------------------------------------------------------------------------
void CarData::Construct(CgsID lId)
{
    mId                          = lId;
    mu8ColourIndex               = 0xFF;   // AddCar stores 0xFF (-1) for both indices
    mu8PaletteIndex              = 0xFF;
    mbUnlockSequenceAlreadyShown = false;
    mfUnlockDeformedAmount       = 0.0f;
    meUnlockType                 = E_UNLOCK_TYPE_UNLOCK;
}

// ------------------------------------------------------------------------------------
// CarData::SetColourIndex -- X360 0x82354890. Range-assert the index fits in a byte,
// then store the low byte to mu8ColourIndex (+0x08).
// ------------------------------------------------------------------------------------
void CarData::SetColourIndex(s32 liColour)
{
    CGS_ASSERT((u32)liColour < 256,
        CgsDev::Assert::MessageAt("luColourIndex < 256", KAC_ARTIST_ASSERT_FILE, 1560 /* ARTIST 0x823548BC */));
    mu8ColourIndex = (u8)liColour;
}

// ------------------------------------------------------------------------------------
// CarData::SetPaletteIndex -- X360 0x823548F0. Range-assert the index fits in a byte,
// then store the low byte to mu8PaletteIndex (+0x09).
// ------------------------------------------------------------------------------------
void CarData::SetPaletteIndex(s32 liPalette)
{
    CGS_ASSERT((u32)liPalette < 256,
        CgsDev::Assert::MessageAt("luPaletteIndex < 256", KAC_ARTIST_ASSERT_FILE, 1568 /* ARTIST 0x8235491C */));
    mu8PaletteIndex = (u8)liPalette;
}

// ------------------------------------------------------------------------------------
// RivalData::Construct -- initialise one rival record (X360 inlines this in AddRival:
// keep the two ids, zero everything else).
// ------------------------------------------------------------------------------------
void RivalData::Construct(CgsID lRivalId, CgsID lCarId)
{
    mRivalId                     = lRivalId;
    mCarId                       = lCarId;
    meState                      = E_STATE_LOCKED;
    miEventCount                 = 0;
    miTakedownFromCount          = 0;
    miVerticalTakedownFromCount  = 0;
    miTakedownToCount            = 0;
    miVerticalTakedownToCount    = 0;
    miTakedownToInEventCount     = 0;
    miTakedownToInLastEventCount = 0;
    miEventMissingCount          = 0;
    mbHasBeenHit                 = false;
}

// ====================================================================================
// Profile::Construct  @ 0x823708A8
// Reset the whole persisted profile to its empty/new state. De-optimised from the X360
// init (which open-codes every store / memset by offset). Each store below is the named
// equivalent of an X360 store, in the same logical groups.
// ====================================================================================
void Profile::Construct()
{
    s32 liIndex;

    miVersionNumber = KI_VERSION_NUMBER;          // *(this+0) = 28
    macName[0]      = 0;                            // empty name

    mCarPosition.SetZero();                         // (0,0,0,0)
    mCarDirection = Vector3(1.0f, 0.0f, 0.0f);

    mSpawnCarId   = 0;
    mSpawnWheelId = 0;
    muTimeStampOfLastRoadRulesDownload = 0;

    mfDistanceDrivenOnline = 0.0f;
    mfDistanceDrivenOffline = 0.0f;
    mfInCarTimePlayed = 0.0f;

    mi8CurrentProgressionRank                       = -2;   // X360 stores -2
    mi8PowerParkingBestRating                       = 0;
    mi8PowerParkingBetweenOtherPlayersBestRating    = 0;
    muBestNewBurnoutChainScore                      = 0;

    // The four 18-entry game-mode-type tally arrays (X360 layout: [18] each, the DLC island
    // mode's slot 17 included; the X360 unrolls a 13-iteration zero loop over the first words,
    // the rest land in the wider zeroing). Zero them whole.
    for (liIndex = 0; liIndex < 18; ++liIndex)
    {
        maGameModeTypeAmount[liIndex]                       = 0;
        maGameModeTypeAmountDiscovered[liIndex]             = 0;
        maGameModeTypeAmountCompleted[liIndex]              = 0;
        maGameModeTypeAmountCompletedSinceTheStart[liIndex] = 0;
    }

    miTotalTakedownCount               = 0;
    miTotalOnlineVerticleTakedownCount = 0;
    for (liIndex = 0; liIndex < 13; ++liIndex)
        maiTakedownTypeCounts[liIndex] = 0;

    // The three 10-entry win/loss arrays (X360 zeroes them in a single interleaved loop).
    for (liIndex = 0; liIndex < 10; ++liIndex)
    {
        maiWinsPerOfflineGameMode[liIndex]     = 0;
        maiRankWinsPerOfflineGameMode[liIndex] = 0;
        maiLossesPerOfflineGameMode[liIndex]   = 0;
    }

    miCompletedBarrelRolls       = 0;
    mfCompletedAirSpinAngle      = 0.0f;
    mfCompletedHandbreakTurnAngle = 0.0f;
    mfCompletedDriftDistance     = 0.0f;
    mfOncomingDistance           = 0.0f;
    mfAirMaximum                 = 0.0f;
    miHighestShowTimeScore       = 0;
    miBestStuntRunScore          = 0;

    miCarCount        = 0;
    miLiveryDataCount = 0;
    miRivalCount      = 0;
    miEventCount      = 0;

    // The owned-car array: X360 fills all 512 slots (id = 0, colour/palette = 0xFF,
    // unlock-shown = 0, deform = 0, type = 0).
    for (liIndex = 0; liIndex < KI_MAX_PROFILE_CAR_COUNT; ++liIndex)
    {
        maCars[liIndex].mId                          = 0;
        maCars[liIndex].mu8ColourIndex               = 0xFF;
        maCars[liIndex].mu8PaletteIndex              = 0xFF;
        maCars[liIndex].mbUnlockSequenceAlreadyShown = false;
        maCars[liIndex].mfUnlockDeformedAmount       = 0.0f;
        maCars[liIndex].meUnlockType                 = CarData::E_UNLOCK_TYPE_UNLOCK;
    }

    // maLiveryChoices + maRivals + maEvents are zeroed by the wide block clears below.
    memset(&maLiveryChoices[0], 0, sizeof(maLiveryChoices));
    memset(&maRivals[0],        0, sizeof(maRivals));
    memset(&maEvents[0],        0, sizeof(maEvents));

    for (liIndex = 0; liIndex < 3; ++liIndex)
        maStuntElements[liIndex].Clear();

    muMedalCountFromTheStart = 0;
    mbSilverCarsUnlocked = false;   // +42516
    mbGoldCarsUnlocked   = false;   // +42517

    mJunkYardsDriveThruSet.Clear();
    mBodyShopsDriveThruSet.Clear();
    mPaintShopsDriveThruSet.Clear();
    mGasStationsDriveThruSet.Clear();
    mCarParksDriveThruSet.Clear();

    maFreeBurnChallengeData.Clear();

    memset(&mabHitPropBitArray, 0, sizeof(mabHitPropBitArray));
    memset(&maaiStuntCountsByCounty[0][0], 0, sizeof(maaiStuntCountsByCounty));
    memset(&maNetworkChallengeData[0], 0, sizeof(maNetworkChallengeData));
    memset(&maChallengeData[0],        0, sizeof(maChallengeData));

    muLastRoadRulesResetTime = 0;

    // Licence picture: a 9600-byte DXT1 buffer + the (still-invalid) NetworkTexture header.
    memset(&macPlayerLicenceTextureData[0], 0, sizeof(macPlayerLicenceTextureData));
    mbPlayerLicencePictureIsValid = false;

    // The five mugshot galleries: each is an Array<MugshotInfo,20> (Construct -> empty) plus its
    // "available file id" bit array. The X360 walks the five galleries and asserts the running
    // index stays <= E_IMAGE_GALLERY_TYPE_COUNT (== 5).
    //
    // ⭐ [progression wave 2026-09-06, lane profile] THE BIT ARRAY IS SET, NOT CLEARED. The X360
    // loop body (@0x82370BB8..0x82370BD4) issues TWO stores to the SAME address -- `std r31(0),
    // 0(r11)` then `std r26(-1), 0(r10)`, both r11 and r10 holding this+0x1CC88+8*type -- i.e.
    // the zero-init followed by a set-every-bit, and the second store is the one that survives.
    // A SET bit means the file id is AVAILABLE (AddMugshot @0x82370D70 takes the first SET bit
    // and clears it with `andc`; DeleteMugshot @0x82371018 sets the freed id back). The previous
    // `memset(..., 0, ...)` left every gallery with NO available file id, so the very first
    // AddMugshot fell straight through to the full-gallery eviction path on an EMPTY gallery.
    for (liIndex = 0; liIndex < 5; ++liIndex)
    {
        maaMugshotInfo[liIndex].Construct();
        maAvailableMugshotFileIDs[liIndex].UnSetAll();   // X360 `std r31(0), 0(r11)`
        maAvailableMugshotFileIDs[liIndex].SetAll();     // X360 `std r26(-1), 0(r10)`
        CGS_ASSERT(liIndex + 1 <= 5, CgsDev::Assert::MessageAt("leEnumIndex <= E_IMAGE_GALLERY_TYPE_COUNT", "..\\..\\..\\GameSource\\GameState/BrnGameStateSharedEnums.h", 237));
    }

    // The two milestone dates: asm writes mbIsLocal=1 (byte @+0) + the two FILETIME words = 0
    // (+117996 mDateLicenceIssued, +118008 mDate100PercentCompleted). Clear() zeroes the time;
    // SetLocal(true) stamps the leading byte.
    mDateLicenceIssued.Clear();
    mDateLicenceIssued.SetLocal(true);
    mDate100PercentCompleted.Clear();
    mDate100PercentCompleted.SetLocal(true);

    mafCarTypes[0] = 0.0f;
    mafCarTypes[1] = 0.0f;
    mafCarTypes[2] = 0.0f;
    meCurrentCarType = 0;

    memset(&maHasPlayerSeenTraining, 0, sizeof(maHasPlayerSeenTraining));

    miNumOnlineRacesDone = 0;
    miNumOnlineRacesWon  = 0;
    miNumMugshotsSent    = 0;

    miHighestNumberOfTakeDownsInRoadRage = 0;

    mb100PercentCompletionSequenceShown = false;
    mbIsNewProfile                      = true;    // X360 stores 1 here
    mbCreditsSequenceViewed             = false;
    mbOneHundredHudMessageViewed        = false;
    mbHasUnlockedCredits                = false;
    mbHaveSet100PercentCompletedDate    = false;
    mbHaveSeenEliteCompletionSequence   = false;
    mbRedundantBool4                    = false;

    mfRealTimePlayed  = 0.0f;
    mfRedundantFloat4 = 0.0f;

    // Seed the road-rules id from the current time (X360 stamps a fresh DateAndTime's raw
    // value into the low/high road-rules id words).
    CgsSystem::DateAndTime lNow;
    lNow.Update();
    u64 lu64RawTime = lNow.GetRawTimeValue();
    muRoadRulesIDLowBits  = static_cast<u32>(lu64RawTime);
    muRoadRulesIDHighBits = static_cast<u32>(lu64RawTime >> 32);

    memset(&mSeenCompleteAllEventTypeArray, 0, sizeof(mSeenCompleteAllEventTypeArray));

    // Trophy-unlock-sequence-seen bits: all clear on a fresh profile.
    memset(&mSeenTrophyAwardBitArray, 0, sizeof(mSeenTrophyAwardBitArray));

    // The X360 DLC-era tail: the X360's three late zero stores (+120032 / +120824 / +120832)
    // are exactly the two Array count-word clears and the developer-challenge bit clear.
    maTargetEventScores.Clear();               // X360 stw 0 @ +120032
    maEventScoresToUpload.Clear();             // X360 stw 0 @ +120824
    mDeveloperChallengesCompleted.Construct(); // X360 8-byte zero @ +120832

}

// ====================================================================================
// Profile::AddCar  @ 0x82366C80
// Append a new owned-car record and pin its (initially self-) chosen livery.
// ====================================================================================
CarData* Profile::AddCar(CgsID lCarId, CarData::UnlockType leUnlockType)
{
    CGS_ASSERT(miCarCount < KI_MAX_PROFILE_CAR_COUNT, CgsDev::Assert::MessageAt("miCarCount < KI_MAX_PROFILE_CAR_COUNT", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProfile.cpp", 560));

    s32 liIndex = miCarCount;
    ++miCarCount;

    CarData& lCar = maCars[liIndex];
    lCar.mfUnlockDeformedAmount       = 0.0f;
    lCar.mId                          = lCarId;
    lCar.mbUnlockSequenceAlreadyShown = false;
    lCar.mu8ColourIndex               = 0xFF;
    lCar.mu8PaletteIndex              = 0xFF;
    lCar.meUnlockType                 = leUnlockType;

    // The newly-added car is its own base livery to begin with.
    SetChosenLiveryIdForBaseCar(lCarId, lCarId);

    return &lCar;
}

// ====================================================================================
// Profile::AddEvent  @ 0x82359EB8
// Append a new discovered-event record (id + zero flags).
// ====================================================================================
ProfileEvent* Profile::AddEvent(u32 luEventID)
{
    CGS_ASSERT(miEventCount < KI_MAX_EVENTS,
        CgsDev::Assert::MessageAt(
            "miEventCount < KI_MAX_EVENTS", KAC_ARTIST_ASSERT_SOURCE, 687 /* ARTIST 0x82359EE8 */));

    s32 liIndex = miEventCount;
    ProfileEvent& lEvent = maEvents[liIndex];
    ++miEventCount;

    lEvent.Construct(luEventID);   // X360 open-codes: id = luEventID, flags = 0
    return &lEvent;
}

// ====================================================================================
// Profile::FindEvent  (const + non-const)  -- DWARF BrnProfile.h:709 / :713
//
// The id->record lookup the console open-codes at every one of its four sites. Verbatim from
// UnlockToProgressionRank @0x8239DEDC (the shortest copy):
//     lwz  r9, 0x278(r3)          ; miEventCount
//     cmpwi r9, 0 / ble  -> miss  ; SIGNED zero-trip guard
//     addi r10, r3, 0x7080        ; &maEvents[0]
//   loop:
//     lwz  r8, 0(r10)             ; muEventID
//     cmplw r8, r4 / beq -> hit
//     addi r11, r11, 1 / addi r10, r10, 8 / cmpw r11, r9 / blt loop
// No assert on either end -- the console's callers fire their own ("lpProfileEvent",
// "lpEvent") on the NULL answer, which is why none is invented here.
// ====================================================================================
const ProfileEvent* Profile::FindEvent(u32 luEventID) const
{
    for (s32 liIndex = 0; liIndex < miEventCount; ++liIndex)
    {
        if (maEvents[liIndex].GetID() == luEventID)
        {
            return &maEvents[liIndex];
        }
    }
    return 0;
}

ProfileEvent* Profile::FindEvent(u32 luEventID)
{
    // The X360 scan walks the mutable array; the const body is the identical instruction run,
    // so the non-const form delegates (the Profile::GetPlayerBaseDeformAmount precedent).
    return const_cast<ProfileEvent*>(
        static_cast<const Profile*>(this)->FindEvent(luEventID));
}

// ====================================================================================
// Profile::AddRival  @ 0x82359F40
// Append a new rival record (the two ids, everything else zeroed).
// ====================================================================================
RivalData* Profile::AddRival(CgsID lRivalId, CgsID lCarId)
{
    CGS_ASSERT(miRivalCount < KI_MAX_RIVAL_COUNT - 1, CgsDev::Assert::MessageAt("miRivalCount < KI_MAX_RIVAL_COUNT - 1", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProfile.cpp", 729));

    s32 liIndex = miRivalCount;
    RivalData& lRival = maRivals[liIndex];
    ++miRivalCount;

    lRival.Construct(lRivalId, lCarId);
    return &lRival;
}

// ====================================================================================
// Profile::FindCar  @ 0x82359BF8
// Linear lookup of the owned-car record whose id == lCarId; NULL on miss.
// ====================================================================================
CarData* Profile::FindCar(CgsID lCarId)
{
    for (s32 liCarIndex = 0; liCarIndex < miCarCount; ++liCarIndex)     // DWARF :300
    {
        if (maCars[liCarIndex].mId == lCarId)
            return &maCars[liCarIndex];
    }
    return 0;
}

// ====================================================================================
// Profile::FindRival  @ 0x82359C48
// Linear lookup of the rival record whose RIVAL id == lRivalId; NULL on miss.
// (asm 0x82359C5C ld r8,0(r10) compares mRivalId @+0, not mCarId @+8; AddRival stores
// lRivalId @+0. Corroborated by the sibling BrnProgressionData::FindRival(CgsID rivalId).)
// ====================================================================================
RivalData* Profile::FindRival(CgsID lRivalId)
{
    for (s32 liRivalIndex = 0; liRivalIndex < miRivalCount; ++liRivalIndex)   // DWARF :341
    {
        if (maRivals[liRivalIndex].mRivalId == lRivalId)
            return &maRivals[liRivalIndex];
    }
    return 0;
}

// ====================================================================================
// Profile::GetChosenLiveryDataForBaseCar  @ 0x82359DC8
// The livery record keyed on lBaseCarId, or NULL.
// ====================================================================================
LiveryData* Profile::GetChosenLiveryDataForBaseCar(CgsID lBaseCarId)
{
    for (s32 liIndex = 0; liIndex < miLiveryDataCount; ++liIndex)         // DWARF :517
    {
        if (maLiveryChoices[liIndex].mBaseCarId == lBaseCarId)
            return &maLiveryChoices[liIndex];
    }
    return 0;
}

// ====================================================================================
// Profile::GetChosenLiveryIdForBaseCar  @ 0x82359D70
// The saved livery-variant id for lBaseCarId; falls back to lBaseCarId itself on miss.
// ====================================================================================
CgsID Profile::GetChosenLiveryIdForBaseCar(CgsID lBaseCarId) const
{
    s32 liCount = miLiveryDataCount;
    if (liCount <= 0)
        return lBaseCarId;

    for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
    {
        if (maLiveryChoices[liIndex].mBaseCarId == lBaseCarId)
            return maLiveryChoices[liIndex].mChosenLiveryCarId;
    }
    return lBaseCarId;
}

// ====================================================================================
// Profile::SetChosenLiveryIdForBaseCar  @ 0x82359C90
// Pin (or overwrite) the chosen livery for lBaseCarId. If the base car already had a
// livery record, overwrite it in place (and keep its distance-driven); otherwise append a
// new record (distance-driven reset to 0). The slot count is left unchanged on overwrite.
// ====================================================================================
void Profile::SetChosenLiveryIdForBaseCar(CgsID lBaseCarId, CgsID lChosenLiveryId)
{
    bool lbIsNewlyAdded = true;
    s32  liIndex        = 0;

    if (miLiveryDataCount > 0)
    {
        for (liIndex = 0; liIndex < miLiveryDataCount; ++liIndex)
        {
            if (maLiveryChoices[liIndex].mBaseCarId == lBaseCarId)
            {
                // Found an existing record: not newly added. The X360 decrements the count
                // here so the unconditional ++ at the end leaves it net-unchanged.
                lbIsNewlyAdded = false;
                --miLiveryDataCount;
                break;
            }
        }

        CGS_ASSERT(liIndex < KI_MAX_PROFILE_CAR_COUNT, CgsDev::Assert::MessageAt("liIndex < KI_MAX_PROFILE_CAR_COUNT", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProfile.cpp", 601));
    }

    LiveryData& lLivery = maLiveryChoices[liIndex];
    lLivery.mBaseCarId        = lBaseCarId;
    lLivery.mChosenLiveryCarId = lChosenLiveryId;
    if (lbIsNewlyAdded)
        lLivery.mfDistanceDriven = 0.0f;

    ++miLiveryDataCount;
}

// ====================================================================================
// Profile::SetCarUnlockAlreadyShown  @ 0x82359E18
// Mark the owned-car record for lCarId as having had its unlock sequence shown.
// ====================================================================================
void Profile::SetCarUnlockAlreadyShown(CgsID lCarId)
{
    CarData* lpCarData = FindCar(lCarId);

    CGS_ASSERT(lpCarData, CgsDev::Assert::MessageAt("lpCarData", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProfile.cpp", 671));
    if (lpCarData)
        lpCarData->mbUnlockSequenceAlreadyShown = true;
}

// ====================================================================================
// Profile::SetBestStuntStats  @ 0x82359FD8
// Raise each best-stunt stat to the new value if larger (X360 only writes on improvement).
// NOTE: the X360 lays these four out as miOncomingDistance(int)+three floats at +588..+600,
// reached by name as mfOncomingDistance/mfAirMaximum/miHighestShowTimeScore/miBestStuntRunScore
// here would be WRONG -- they are the dedicated best-stunt members miCompletedBarrelRolls +
// the three mfCompleted* stats; that is exactly what the +588/+592/+596/+600 stores hit.
// ====================================================================================
void Profile::SetBestStuntStats(s32 liCompletedBarrelRolls, f32 lfCompletedAirSpinAngle,
                                f32 lfCompletedHandbreakTurnAngle, f32 lfCompletedDriftDistance)
{
    if (miCompletedBarrelRolls < liCompletedBarrelRolls)
        miCompletedBarrelRolls = liCompletedBarrelRolls;

    if (mfCompletedAirSpinAngle < lfCompletedAirSpinAngle)
        mfCompletedAirSpinAngle = lfCompletedAirSpinAngle;

    if (mfCompletedHandbreakTurnAngle < lfCompletedHandbreakTurnAngle)
        mfCompletedHandbreakTurnAngle = lfCompletedHandbreakTurnAngle;

    if (mfCompletedDriftDistance < lfCompletedDriftDistance)
        mfCompletedDriftDistance = lfCompletedDriftDistance;
}

// ====================================================================================
// Profile::GetMedalCountFromTheStart
// Trivial named-member getter for the medal-progress count the X360 reads inline
// (Profile +42512): ProgressionManager::AreRoadRulesAvailable tests it >= 4, and so does
// GameStateModule::PreWorldUpdate when it builds the game-action-193 flag byte. The X360
// emits no out-of-line body (every reader inlines the load); the declaration was already
// in the header, this is its definition.
// ====================================================================================
u32 Profile::GetMedalCountFromTheStart() const
{
    return muMedalCountFromTheStart;
}

// ====================================================================================
// [tut-ticker] Profile::GetInCarTimePlayed
// Trivial named-member getter for the raw Profile+108 read the TrainingManager bodies
// open-code (see the header FLAG: no PC writer accumulates it yet).
// ====================================================================================
f32 Profile::GetInCarTimePlayed() const
{
    return mfInCarTimePlayed;
}

// ====================================================================================
// [tut-ticker] Profile::ClearTrainingFlags
// DEBUG-only reset of the whole 256-bit training bit array. The X360 inlines it into
// TrainingManager::DEBUG_ClearTrainingFlags @0x82366050 as four zero `std`s over
// profile+117952..117983 (v8[14744..14747] = 0); no out-of-line symbol exists.
// ====================================================================================
void Profile::ClearTrainingFlags()
{
    maHasPlayerSeenTraining = CgsContainers::BitArray<256u>();
}

// ====================================================================================
// ⭐ [tut-ticker] Profile::HasPlayerSeenTrainingType  @ 0x8231C878
// The read twin of SetTrainingAlreadySeen below: bounds-assert the tip id (the console's
// own :2769 assert plus the CgsBitArray Get guard, CgsBitArray.h:203), then test the tip's
// bit in the 256-bit training bit array.
// ====================================================================================
bool Profile::HasPlayerSeenTrainingType(ETrainingType leTrainingType) const
{
    CGS_ASSERT(leTrainingType >= 0 && leTrainingType < E_TRAINING_TYPE_COUNT,
               CgsDev::Assert::MessageAt("leTrainingType >= 0 && leTrainingType < E_TRAINING_TYPE_COUNT", "..\\..\\..\\GameSource\\GameState/Progression/BrnProfile.h", 2769));  // :2769
    CGS_ASSERT(static_cast<u32>(leTrainingType) < 256u, "invalid index : ");      // CgsBitArray.h:203

    return maHasPlayerSeenTraining.IsBitSet(static_cast<u32>(leTrainingType));
}

// ====================================================================================
// Profile::GetLicenceIssuedDate  @ 0x8235A0B8
// Copy out the licence-issued date (returned by value).
// ====================================================================================
CgsSystem::DateAndTime Profile::GetLicenceIssuedDate() const
{
    return mDateLicenceIssued;
}

// ====================================================================================
// Profile::SetLicenceIssuedDateAsNow  @ 0x8235A0E0
// Stamp the licence-issued date with "now".
// ====================================================================================
void Profile::SetLicenceIssuedDateAsNow()
{
    mDateLicenceIssued.Update();
}

// ====================================================================================
// Profile::Get100PercentCompletedDate  @ 0x8235A0F0
// Copy out the 100%-completed date (returned by value).
// ====================================================================================
CgsSystem::DateAndTime Profile::Get100PercentCompletedDate() const
{
    return mDate100PercentCompleted;
}

// ====================================================================================
// Completion-sequence flags (trivial getters/setters).
// ====================================================================================
bool Profile::GetSeen100PercentCompletionSequence() const   // 0x8235A118
{
    return mb100PercentCompletionSequenceShown;
}

void Profile::SetSeen100PercentCompletionSequence()         // 0x8235A128
{
    mb100PercentCompletionSequenceShown = true;
}

bool Profile::GetSeenEliteCompletionSequence() const        // 0x8235A140
{
    return mbHaveSeenEliteCompletionSequence;
}

void Profile::SetSeenEliteCompletionSequence()              // 0x8235A150
{
    mbHaveSeenEliteCompletionSequence = true;
}

// ====================================================================================
// Profile::DEBUG_ClearMedals  @ 0x8235A168
// DEBUG: clear every event's flag word and the per-game-mode rank-win/loss arrays.
// ====================================================================================
void Profile::DEBUG_ClearMedals()
{
    for (s32 liIndex = 0; liIndex < miEventCount; ++liIndex)
        maEvents[liIndex].SetFlags(0);

    // The X360 zeroes two interleaved 10-entry arrays here: the rank-win array (+0x1FC/508)
    // and the win array 0x28 bytes before it (+0x1D4/468).
    for (s32 liModeIndex = 0; liModeIndex < 10; ++liModeIndex)
    {
        maiWinsPerOfflineGameMode[liModeIndex]     = 0;
        maiRankWinsPerOfflineGameMode[liModeIndex] = 0;
    }
}

// ====================================================================================
// Profile::GetNumMugshots  @ 0x82366DA0
// The live count of one mugshot gallery.
// ====================================================================================
s32 Profile::GetNumMugshots(s32 leMugshotType)
{
    CGS_ASSERT(leMugshotType < 5, CgsDev::Assert::MessageAt("leMugshotType < GsmIO::E_IMAGE_GALLERY_TYPE_COUNT", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProfile.cpp", 979));

    return maaMugshotInfo[leMugshotType].GetCount();
}

// ====================================================================================
// Profile::GetNumAllMugshots  @ 0x82366D28
// The total live count across all five mugshot galleries.
// ====================================================================================
s32 Profile::GetNumAllMugshots()
{
    s32 liTotal = 0;
    for (s32 liIndex = 0; liIndex < 5; ++liIndex)
        liTotal += maaMugshotInfo[liIndex].GetCount();
    return liTotal;
}

// ====================================================================================
// Profile::SetPlayerLicencePicture  @ 0x8235A020
// Build the player's licence-picture texture from a freshly captured network image:
// prepare a DXT1 NetworkTexture over the embedded 9600-byte buffer, then convert the new
// image into it and flag the picture valid.
// ====================================================================================
void Profile::SetPlayerLicencePicture(const CgsNetwork::NetworkTexture* lpNewPlayerImage)
{
    CGS_ASSERT(lpNewPlayerImage,
        CgsDev::Assert::MessageAt("lpNewPlayerImage", KAC_ARTIST_ASSERT_SOURCE, 1273 /* ARTIST 0x8235A044 */));

    mPlayerLicencePicture.Construct();
    mPlayerLicencePicture.Prepare(&macPlayerLicenceTextureData[0],
                                  KI_PLAYERLICENCEPICTURE_TEXTURESIZEINBYTES,
                                  KI_PLAYERLICENCEPICTURE_WIDTH,
                                  KI_PLAYERLICENCEPICTURE_HEIGHT,
                                  renderengine::PIXELFORMAT_DXT1);

    CgsNetwork::NetworkImageConverter lConverter;
    lConverter.Convert(lpNewPlayerImage, &mPlayerLicencePicture);

    mbPlayerLicencePictureIsValid = true;
}

// ====================================================================================
// MugshotInfo::Construct  (DWARF BrnProfile.h:365; X360-INLINED into both of AddMugshot's
// Append paths -- the six stack stores at 0x82370F30..0x82370F84 / 0x82370FA8..0x82370FF8).
// ====================================================================================
void MugshotInfo::Construct(UniquePlayerID lUniquePlayerID, CgsSystem::DateAndTime lCaptureDate,
                            BrnWorld::WorldRegion lWorldRegion, u16 lu16FileID)
{
    mUniquePlayerID = lUniquePlayerID;   // +0x00, the r5/r6/r7 doubleword triple
    mCaptureDate    = lCaptureDate;      // +0x18 (12 bytes)
    mWorldRegion    = lWorldRegion;      // +0x24 (`std r23`)
    miNumCaptures   = 1;                 // +0x2C (`stw r7`(1) / `li r11,1 ; stw`)
    mu16FileID      = lu16FileID;        // +0x30 (`sth`)
    mbLocked        = false;             // +0x32 (`stb r24`(0))
}

// ====================================================================================
// MugshotInfo -- the named sub-block getters BrnGameStateImageManagerBase builds its
// type-290 "image info" wire record from. See the NAME NOTE in BrnProfile.h: three of
// these names were minted from the offsets that TU saw moved, before the DWARF member
// list was attached; the bodies below say which member each one actually reads.
// ====================================================================================
MugshotInfo::UniquePlayerIDImage MugshotInfo::GetUniquePlayerID() const
{
    // The 16-byte PlayerName base, moved as the two opaque qwords the wire record carries.
    UniquePlayerIDImage lImage;
    memcpy(&lImage, &mUniquePlayerID.macName[0], sizeof(lImage));
    return lImage;
}
s32 MugshotInfo::GetImageWord0() const    { return mCaptureDate.IsLocal() ? 1 : 0; }      // +0x18
s32 MugshotInfo::GetImageWord1() const                                                    // +0x1C
{
    return static_cast<s32>(static_cast<u32>(mCaptureDate.GetRawTimeValue()));
}
s32 MugshotInfo::GetImageWord2() const                                                    // +0x20
{
    return static_cast<s32>(static_cast<u32>(mCaptureDate.GetRawTimeValue() >> 32));
}
u64 MugshotInfo::GetDateTaken() const                                                     // +0x24
{
    // mWorldRegion's 8 bytes, in the console's big-endian qword order (county then district).
    return (static_cast<u64>(static_cast<u32>(mWorldRegion.GetCounty())) << 32)
         |  static_cast<u64>(static_cast<u32>(mWorldRegion.GetDistrict()));
}
s32 MugshotInfo::GetImageWord2C() const { return miNumCaptures; }                          // +0x2C
s32 MugshotInfo::GetFileID() const     { return static_cast<s32>(mu16FileID); }            // +0x30 (lhz == zero-extended)
u8  MugshotInfo::GetLockedFlag() const { return mbLocked ? 1u : 0u; }                      // +0x32

// ====================================================================================
// Profile::AddMugshot  @ 0x82370D70
// Add (or replace the oldest non-locked) mugshot in gallery leMugshotType and return the
// FILE ID it was given (1000*type + bit index), or -1 when the gallery is full of locked
// photos. The X360 claims the first AVAILABLE (== set) bit of the gallery's file-id bit
// array and clears it; if none is available it asserts the gallery is full, evicts the
// first non-locked record and reuses that record's file id.
//
// ⭐ [progression wave 2026-09-06, lane profile] this body used to be a stated partial: it
// claimed a file id with the bit polarity INVERTED (first CLEAR bit, then SetBit) and never
// appended a record at all, because MugshotInfo was an opaque 56-byte pad. Both are fixed --
// the record is real now (MugshotInfo::Construct above) and the polarity is the console's
// (`GetFirstNonZeroBit` + `andc`, see Profile::Construct).
// Hex-Rays suffered a local-allocation failure on this function; the asm is the spine.
// ====================================================================================
s32 Profile::AddMugshot(s32 leMugshotType, MugshotInfo::UniquePlayerID lUniqueID,
                        CgsSystem::DateAndTime lDateTaken, BrnWorld::WorldRegion lRegion)
{
    CGS_ASSERT(leMugshotType != 5, CgsDev::Assert::MessageAt("leMugshotType != GsmIO::E_IMAGE_GALLERY_TYPE_COUNT", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProfile.cpp", 758));  // X360 BrnProfile.cpp:758

    CgsContainers::BitArray<20u>& lrFileIds = maAvailableMugshotFileIDs[leMugshotType];
    CgsContainers::Array<MugshotInfo, 20u>&      lrGallery = maaMugshotInfo[leMugshotType];

    MugshotInfo lRecord;

    // X360 0x82370DD8..0x82370E28: the inlined BitArray<20>::GetFirstNonZeroBit (isolate the
    // lowest set bit with `x - (x & (x-1))`, index == field*64 + 63 - cntlzd), then `>= 20 -> -1`.
    const s32 liFreeFileIdBit = lrFileIds.GetFirstNonZeroBit();
    if (liFreeFileIdBit != CgsContainers::BitArray<20u>::KI_INVALID_BITINDEX)
    {
        CGS_ASSERT(static_cast<u32>(liFreeFileIdBit) < 20u, CgsDev::Assert::MessageAt("luIndex < NUMBITS", "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsBitArray.h", 241));   // CgsBitArray.h:241
        lrFileIds.UnSetBit(static_cast<u32>(liFreeFileIdBit));                      // X360 `andc`

        // X360 `mulli r8, r28, 0x3E8 ; add r30, r8, r31` -- the file id is namespaced per gallery.
        const s32 liFileID = 1000 * leMugshotType + liFreeFileIdBit;
        lRecord.Construct(lUniqueID, lDateTaken, lRegion, static_cast<u16>(liFileID));
        lrGallery.Append(lRecord);
        return liFileID;
    }

    // No file id left: the gallery must be full (X360 BrnProfile.cpp:784).
    CGS_ASSERT(lrGallery.IsFull(), CgsDev::Assert::MessageAt("maaMugshotInfo[leMugshotType].IsFull()", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProfile.cpp", 784));

    // Evict the first NON-LOCKED record and reuse its file id (X360 0x82370E84..0x82371008).
    const s32 liCount = static_cast<s32>(lrGallery.GetLength());
    for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
    {
        MugshotInfo& lrVictim = lrGallery.GetItem(static_cast<u32>(liIndex));
        if (!lrVictim.mbLocked)                                   // X360 `lbz 0x32 ; beq`
        {
            const s32 liFileID = static_cast<s32>(lrVictim.mu16FileID);   // X360 `lhz 0x30`
            lrGallery.Erase(static_cast<u32>(liIndex));
            lRecord.Construct(lUniqueID, lDateTaken, lRegion, static_cast<u16>(liFileID));
            lrGallery.Append(lRecord);
            return liFileID;
        }
    }

    return -1;   // X360 `mr r3, r25` with r25 == -1
}

// ====================================================================================
// Profile::GetMugshotInfo  @ 0x82371290
// The leMugshotType'th gallery's liImageIndex'th record, or NULL when the index is past the
// gallery's live count. The X360 range-asserts the gallery type, then reads the count word
// (whose own "Array used before Construct/Clear was called" assert is Array::GetLength's) and
// SIGNED-compares the index against it before calling Array<MugshotInfo,20>::GetItem.
// ====================================================================================
MugshotInfo* Profile::GetMugshotInfo(s32 leMugshotType, s32 liImageIndex)
{
    CGS_ASSERT(leMugshotType < 5, CgsDev::Assert::MessageAt("leMugshotType < GsmIO::E_IMAGE_GALLERY_TYPE_COUNT", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProfile.cpp", 999));  // X360 BrnProfile.cpp:999

    CgsContainers::Array<MugshotInfo, 20u>& lrGallery = maaMugshotInfo[leMugshotType];
    if (liImageIndex >= static_cast<s32>(lrGallery.GetLength()))    // X360 `cmpw ; bge -> li r3,0`
    {
        return 0;
    }
    return &lrGallery.GetItem(static_cast<u32>(liImageIndex));
}

// ====================================================================================
// Profile::LockOrUnlockMugshot  @ 0x823711C0
// Toggle one record's "locked for deletion" flag. Returns true when the index named a live
// record, false when it did not. The X360 computes `!flag` as `cntlzw ; extrwi ..,1,26`.
// (The console fetches the record TWICE -- once to read the flag, once as the store target;
// one reference is the same store.)
// ====================================================================================
bool Profile::LockOrUnlockMugshot(s32 leMugshotType, s32 liImageIndex)
{
    CGS_ASSERT(leMugshotType < 5, CgsDev::Assert::MessageAt("leMugshotType < GsmIO::E_IMAGE_GALLERY_TYPE_COUNT", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProfile.cpp", 926));  // X360 BrnProfile.cpp:926

    CgsContainers::Array<MugshotInfo, 20u>& lrGallery = maaMugshotInfo[leMugshotType];
    if (liImageIndex >= static_cast<s32>(lrGallery.GetLength()))
    {
        return false;                                             // X360 `li r3, 0`
    }

    MugshotInfo& lrInfo = lrGallery.GetItem(static_cast<u32>(liImageIndex));
    lrInfo.mbLocked = !lrInfo.mbLocked;                            // X360 `stb .., 0x32`
    return true;                                                   // X360 `li r3, 1`
}

// ====================================================================================
// Profile::DeleteMugshot  @ 0x82371018
// Remove one gallery record and hand its file id back to the gallery's available-id bit
// array. Returns the freed file id, or -1 when the index is out of range or the record is
// LOCKED. The bit index is the file id with the gallery's 1000-per-type namespace removed.
// ====================================================================================
s32 Profile::DeleteMugshot(s32 leMugshotType, s32 liImageIndex)
{
    CGS_ASSERT(leMugshotType < 5, CgsDev::Assert::MessageAt("leMugshotType < GsmIO::E_IMAGE_GALLERY_TYPE_COUNT", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProfile.cpp", 836));  // X360 BrnProfile.cpp:836

    s32 liFileID = -1;                                             // X360 `li r27, -1`

    CgsContainers::Array<MugshotInfo, 20u>& lrGallery = maaMugshotInfo[leMugshotType];
    if (liImageIndex < static_cast<s32>(lrGallery.GetLength()))
    {
        MugshotInfo& lrInfo = lrGallery.GetItem(static_cast<u32>(liImageIndex));
        if (!lrInfo.mbLocked)                                      // X360 `lbz 0x32 ; bne -> out`
        {
            liFileID = static_cast<s32>(lrInfo.mu16FileID);        // X360 `lhz 0x30` (zero-extended)
            lrGallery.Erase(static_cast<u32>(liImageIndex));

            // X360 `mulli r11, r30, 0x3E8 ; subf r29, r11, r27` then the inlined
            // BitArray<20>::SetBit. The bounds assert (CgsBitArray.h:222, whose streamed message
            // is "Index: <n>, Number of bits: 20") is NOT a guard on the console -- the store is
            // issued either way -- so it is kept as an assert, not an early-out.
            const s32 liNewlyAvailableID = liFileID - 1000 * leMugshotType;
            CGS_ASSERT(static_cast<u32>(liNewlyAvailableID) < 20u, "luIndex < NUMBITS");
            maAvailableMugshotFileIDs[leMugshotType].SetBit(static_cast<u32>(liNewlyAvailableID));
        }
    }

    return liFileID;
}

// ====================================================================================
// Profile::AddDriveThru  @ 0x82374DB8
// Record a discovered drive-thru of the given category. When the category's set reaches
// its capacity (every drive-thru of that kind found) the matching trophy-unlock id is
// returned; otherwise 0 (E_UNLOCKTYPE_NONE). The X360 switch covers the five drive-thru
// GenericRegion types (0..4); anything else fires the streamed assert. The literal
// unlock ids 17..20 are TrophyUnlockData::UnlockType values (enumerator names
// unrecovered; only NONE=0/COUNT=35 are committed).
// ====================================================================================
s32 Profile::AddDriveThru(CgsID lId, BrnTrigger::GenericRegion::Type leType)
{
    switch (leType)
    {
    case BrnTrigger::GenericRegion::E_TYPE_JUNK_YARD:
        mJunkYardsDriveThruSet.Insert(lId);
        if (mJunkYardsDriveThruSet.GetLength() == 5)
            return 18;   // all junk yards found
        break;

    case BrnTrigger::GenericRegion::E_TYPE_GAS_STATION:
        mGasStationsDriveThruSet.Insert(lId);
        if (mGasStationsDriveThruSet.GetLength() == 14)
            return 17;   // all gas stations found
        break;

    case BrnTrigger::GenericRegion::E_TYPE_BODY_SHOP:
        mBodyShopsDriveThruSet.Insert(lId);
        if (mBodyShopsDriveThruSet.GetLength() == 11)
            return 20;   // all body shops found
        break;

    case BrnTrigger::GenericRegion::E_TYPE_PAINT_SHOP:
        mPaintShopsDriveThruSet.Insert(lId);
        if (mPaintShopsDriveThruSet.GetLength() == 5)
            return 19;   // all paint shops found
        break;

    case BrnTrigger::GenericRegion::E_TYPE_CAR_PARK:
        mCarParksDriveThruSet.Insert(lId);
        return 0;

    default:
        CGS_ASSERT(false, "We must know what type of drive through this is! Right now we don't\n");
        break;
    }

    return 0;
}

// ====================================================================================
// Profile::IsDriveThruDiscoverd  @ 0x8236ABA8   [sic -- the console's own spelling]
// The read twin of AddDriveThru: has this id already been recorded in its category's set?
// Same five-case switch over the drive-thru GenericRegion types, each arm a Set::Contains
// on the SAME set AddDriveThru inserts into (asm jump table @0x8236ABC0: case 0 ->
// this+42520, 1 -> +42712, 2 -> +42568, 3 -> +42664, 4 -> +42832 -- exactly the five
// members below), and the same streamed assert (BrnProfile.h:2587) on any other type.
//
// SIGNATURE from the MANGLED NAME, not the Hex-Rays render: the export decompiles as a
// ten-argument mess because the 32-bit PPC ABI passes the 64-bit CgsID in a register PAIR
// and Hex-Rays folded `this` into the high half of its `a1` (`HIDWORD(a1) + 42520` IS
// `this + 42520`). The link-time mangled form is
// `IsDriveThruDiscoverd(unsigned __int64, BrnTrigger::GenericRegion::Type) const`, which
// matches the header declaration; the switch is on the TYPE, the id is what Contains tests.
// ====================================================================================
bool Profile::IsDriveThruDiscoverd(CgsID lId, BrnTrigger::GenericRegion::Type leType) const
{
    switch (leType)
    {
    case BrnTrigger::GenericRegion::E_TYPE_JUNK_YARD:
        return mJunkYardsDriveThruSet.Contains(lId);

    case BrnTrigger::GenericRegion::E_TYPE_GAS_STATION:
        return mGasStationsDriveThruSet.Contains(lId);

    case BrnTrigger::GenericRegion::E_TYPE_BODY_SHOP:
        return mBodyShopsDriveThruSet.Contains(lId);

    case BrnTrigger::GenericRegion::E_TYPE_PAINT_SHOP:
        return mPaintShopsDriveThruSet.Contains(lId);

    case BrnTrigger::GenericRegion::E_TYPE_CAR_PARK:
        return mCarParksDriveThruSet.Contains(lId);

    default:
        CGS_ASSERT(false, "We must know what type of drive through this is! Right now we don't\n");
        break;
    }

    return false;   // X360 `result = 0` on the assert arm
}

// ====================================================================================
// Profile::GetNumDriveThrusDiscovered  @ 0x823619F8
// How many of that category have been found -- the same five-case switch, each arm a
// Set::GetLength on the same set (asm jump table @0x82361A20: case 0 -> this+42520 via
// Set<CgsID,5>::GetLength, 1 -> +42712 (14), 2 -> +42568 (11), 3 -> +42664 (5),
// 4 -> +42832 (11)); the default arm streams the same assert (BrnProfile.h:2538) and
// returns 0. The capacities the jump table's GetLength instantiations name are exactly the
// five declared set widths, which is what pins each arm to its member.
// ====================================================================================
s32 Profile::GetNumDriveThrusDiscovered(BrnTrigger::GenericRegion::Type leType) const
{
    switch (leType)
    {
    case BrnTrigger::GenericRegion::E_TYPE_JUNK_YARD:
        return static_cast<s32>(mJunkYardsDriveThruSet.GetLength());

    case BrnTrigger::GenericRegion::E_TYPE_GAS_STATION:
        return static_cast<s32>(mGasStationsDriveThruSet.GetLength());

    case BrnTrigger::GenericRegion::E_TYPE_BODY_SHOP:
        return static_cast<s32>(mBodyShopsDriveThruSet.GetLength());

    case BrnTrigger::GenericRegion::E_TYPE_PAINT_SHOP:
        return static_cast<s32>(mPaintShopsDriveThruSet.GetLength());

    case BrnTrigger::GenericRegion::E_TYPE_CAR_PARK:
        return static_cast<s32>(mCarParksDriveThruSet.GetLength());

    default:
        CGS_ASSERT(false, "We must know what type of drive through this is! Right now we don't\n");
        break;
    }

    return 0;   // X360 `result = 0` on the assert arm
}

// ====================================================================================
// THE THREE CALLER-NAMED ALIASES DriveThruManager::UnlockCarChallengeForCar NEEDS.
//
// ⚠️⚠️ NONE OF THESE THREE NAMES IS IN THE BINARY OR IN THE DWARF. The DecFIGS
// declaration of BrnProgression::ProfileEvent (BrnProfile.h:293) has EXACTLY seven methods --
// Construct, GetID, GetFlags, SetFlags, IsFlagSet, EnableFlags, ClearFlags -- and no IsFound
// or SetFound; Profile has no IncrementNumDiscoveredEvents and no FindProfileEventByRaceEventId.
// The X360 open-codes all of them INSIDE UnlockCarChallengeForCar @0x82386840, and the
// already-committed reconstruction of that caller spelled each open-coded run as a named
// method. They are bodied here -- as thin, documented forwards onto the DWARF-attested methods
// that own the same stores -- because a mounted caller with no callee is an LNK2019 and a
// duplicated second scan/second bit-poke would be a real ODR-shaped fork of the same state.
// ⭐ RETIRE-WHEN the caller is rewritten onto the attested names; the header note at
// BrnProfile.h:495 says the same thing from the declaration side.
//
// ASM, @0x823869F4..0x82386A50, the whole run these three cover:
//     lhz    r11, 4(r31)              ; muFlags
//     clrlwi r10, r11, 31             ; r10 = muFlags & 1                 -> IsFound()
//     cmplwi cr6, r10, 0 / bne  -> skip
//     li     r11, 1
//     rlwimi r10, r11, 0,31,15        ; r10 = (muFlags & ~1) | 1          -> SetFound(true)
//     sth    r10, 4(r31)
//     li     r11, 5                   ; E_MODE_BURNING_ROUTE
//     lwz    r10, 0x950(r22)          ; mpProgressionManager
//     lwz    r11, 0x244(r10)          ; ProgressionManager + 580
//     addi   r11, r11, 1
//     stw    r11, 0x244(r10)          ; ++   (a 32-bit word)
// ====================================================================================
bool ProfileEvent::IsFound() const
{
    // `muFlags & 1` -- bit 0 IS E_FLAG_DISCOVERED, so this is IsFlagSet, nothing more.
    return IsFlagSet(E_FLAG_DISCOVERED);
}

void ProfileEvent::SetFound(bool lbFound)
{
    // `rlwimi r10, r11, 0,31,15` inserts bit 0 of r11 into r10 and leaves bits 1..15 alone:
    // a SET when the inserted bit is 1, a CLEAR when it is 0. The console's only call site
    // passes 1, so only the Enable arm is exercised by the binary; the Clear arm is the same
    // instruction with r11 == 0 and is spelled with the attested ClearFlags.
    if (lbFound)
    {
        EnableFlags(E_FLAG_DISCOVERED);
    }
    else
    {
        ClearFlags(E_FLAG_DISCOVERED);
    }
}

// ====================================================================================
// ⛔ Profile::IncrementNumDiscoveredEvents -- DELETED 2026-08-27 (drive-thru link-closure wave).
//
// ⭐⭐ THE NAME WAS WRONG AND THE ASM SAID SO. It was never a generic "one more event found"
// counter: the target is a FIXED word, ProgressionManager+0x244 == Profile+212, and
// 212 == 192 + 4*5 -- element FIVE of maGameModeTypeAmountDiscovered, whose owner is the
// DWARF-attested Profile::AddGameModeTypeToDiscovered @0x82354AA0 (`++v3[a2 + 48]`, i.e.
// `++*(Profile + 192 + 4*type)`). The compiler folded 192 + 4*5 into the constant 0x244, which
// is what made the call look like an anonymous increment.
// ⚠️ PRECISION FIX to the note this replaces: the corroborating `li r11, 5` @0x82386A38 is
// FOUR instructions earlier, not two, and it is consumed by `stw r11, var_88` @0x82386A44 --
// the record's meGameModeType at rec+0x18 -- NOT by the 0x244 access, which reloads r11 from
// `lwz r11, 0x244(r10)` @0x82386A48. That makes it INDEPENDENT corroboration rather than the
// same operand: the arm writes mode 5 into the action AND bumps discovered-counter 5, and
// 5 == GameStateModuleIO::E_MODE_BURNING_ROUTE is exactly what UnlockCarChallengeForCar
// unlocks. The sibling discovery arm in GameStateModule_gSR_00.cpp:395 makes the same call with
// the mode read from the event instead of folded, and already spells it
// AddGameModeTypeToDiscovered -- which is now what the drive-thru arm spells too.
// ====================================================================================

// ====================================================================================
// Profile::FindProfileEventByRaceEventId
//
// The SAME linear scan as the DWARF-attested Profile::FindEvent(u32) above -- count word at
// +0x278, base at +0x7080, 8-byte stride, first record whose muEventID matches, NULL on miss.
// The X360 run at 0x82386988..0x823869B4 is instruction-for-instruction the one FindEvent's
// banner already quotes from UnlockToProgressionRank.
//
// ⚠️ THE WIDENED KEY IS THE CALLER'S, NOT THE BINARY'S. The mangled name the link demands is
// `...FindProfileEventByRaceEventId(unsigned __int64)`, but the console compare is a 32-bit
// `cmplw` of maEvents[i].muEventID against `lwz r11, 0(r30)` == EventJunction::muID, a u32
// field of a 16-byte record with no 64-bit member anywhere in it. So the u64 parameter is a
// widening introduced when the caller was reconstructed, and the narrowing back to the u32 the
// record actually stores happens HERE, once, in the open -- not silently inside a compare.
// ====================================================================================
ProfileEvent* Profile::FindProfileEventByRaceEventId(CgsID lEventId)
{
    return FindEvent(static_cast<u32>(lEventId));
}

// ====================================================================================
// Profile::AreAllDriveThrusCompleted  @ 0x82361870
// True when every drive-thru category's set is full (the X360 short-circuits in this
// exact order: junk yards, body shops, paint shops, gas stations, car parks).
// ====================================================================================
bool Profile::AreAllDriveThrusCompleted()
{
    return mJunkYardsDriveThruSet.GetLength()   == 5  &&
           mBodyShopsDriveThruSet.GetLength()   == 11 &&
           mPaintShopsDriveThruSet.GetLength()  == 5  &&
           mGasStationsDriveThruSet.GetLength() == 14 &&
           mCarParksDriveThruSet.GetLength()    == 11;
}

// ====================================================================================
// Profile::GetDriveThrusFound  @ 0x82361778
// Total discovered drive-thrus across the four TROPHY categories -- the X360 sums junk
// yards + body shops + paint shops + gas stations and deliberately EXCLUDES car parks.
// ====================================================================================
s32 Profile::GetDriveThrusFound() const
{
    return static_cast<s32>(mJunkYardsDriveThruSet.GetLength() +
                            mBodyShopsDriveThruSet.GetLength() +
                            mPaintShopsDriveThruSet.GetLength() +
                            mGasStationsDriveThruSet.GetLength());
}

// ====================================================================================
// Profile::AddGameModeTypeToTotals  -- DWARF BrnProfile.h:751; inlined @0x82366628
// Bump the "this many events of this type exist" tally for one game-mode type. The console
// body is the two instructions ProgressionManager::AddEventTypeToEventTotals ends with,
// `++*(4*(mode+30) + profile)` == ++maGameModeTypeAmount[mode], behind the range assert whose
// baked location is BrnProfile.h:2047 (identical string to the three siblings below).
// ====================================================================================
void Profile::AddGameModeTypeToTotals(GsmIO::EGameModeType lEGameModeType)
{
    CGS_ASSERT(lEGameModeType > GsmIO::E_MODE_NONE, "lEGameModeType > GsmIO::E_MODE_NONE");

    ++maGameModeTypeAmount[lEGameModeType];
}

// ====================================================================================
// Profile::AddGameModeTypeCompleted  @ 0x82354B10
// Bump the completed (and completed-since-the-start) tallies for one game-mode type.
// ====================================================================================
void Profile::AddGameModeTypeCompleted(GsmIO::EGameModeType lEGameModeType)
{
    CGS_ASSERT(lEGameModeType > GsmIO::E_MODE_NONE,
        CgsDev::Assert::MessageAt(
            "lEGameModeType > GsmIO::E_MODE_NONE", KAC_ARTIST_ASSERT_FILE, 2091 /* ARTIST 0x82354B3C */));

    ++maGameModeTypeAmountCompleted[lEGameModeType];
    ++maGameModeTypeAmountCompletedSinceTheStart[lEGameModeType];
}

// ====================================================================================
// Profile::AddGameModeTypeToDiscovered  @ 0x82354AA0
// Bump the discovered tally for one game-mode type.
// ====================================================================================
void Profile::AddGameModeTypeToDiscovered(GsmIO::EGameModeType lEGameModeType)
{
    CGS_ASSERT(lEGameModeType > GsmIO::E_MODE_NONE,
        CgsDev::Assert::MessageAt(
            "lEGameModeType > GsmIO::E_MODE_NONE", KAC_ARTIST_ASSERT_FILE, 2064 /* ARTIST 0x82354ACC */));

    ++maGameModeTypeAmountDiscovered[lEGameModeType];
}

// ====================================================================================
// Profile::GetGameModeTypeAmount  @ 0x82354A38
// ====================================================================================
s32 Profile::GetGameModeTypeAmount(GsmIO::EGameModeType lEGameModeType) const
{
    CGS_ASSERT(lEGameModeType > GsmIO::E_MODE_NONE,
        CgsDev::Assert::MessageAt(
            "lEGameModeType > GsmIO::E_MODE_NONE", KAC_ARTIST_ASSERT_FILE, 2055 /* ARTIST 0x82354A64 */));

    return maGameModeTypeAmount[lEGameModeType];
}

// ====================================================================================
// Profile::GetGameModeTypeDiscovered  @ 0x8240E940
// ====================================================================================
s32 Profile::GetGameModeTypeDiscovered(GsmIO::EGameModeType lEGameModeType) const
{
    CGS_ASSERT(lEGameModeType > GsmIO::E_MODE_NONE,
        CgsDev::Assert::MessageAt(
            "lEGameModeType > GsmIO::E_MODE_NONE", KAC_ARTIST_ASSERT_FILE, 2072 /* ARTIST 0x8240E96C */));

    return maGameModeTypeAmountDiscovered[lEGameModeType];
}

// ====================================================================================
// Profile::AddWinForGameMode  @ 0x82354C80
// Bump both the win and the rank-win tallies for one OFFLINE game-mode.
// ====================================================================================
void Profile::AddWinForGameMode(GsmIO::EGameModeType leGameModeType)
{
    CGS_ASSERT((leGameModeType < GsmIO::E_MODE_OFFLINE_COUNT) && (leGameModeType > GsmIO::E_MODE_NONE),
        CgsDev::Assert::MessageAt(
            "( leGameModeType < GsmIO::E_MODE_OFFLINE_COUNT ) && ( leGameModeType > GsmIO::E_MODE_NONE )",
            KAC_ARTIST_ASSERT_FILE, 2294 /* ARTIST 0x82354CB4 */));

    ++maiWinsPerOfflineGameMode[leGameModeType];
    ++maiRankWinsPerOfflineGameMode[leGameModeType];
}

// ====================================================================================
// Profile::AddLossForGameMode  @ 0x8230FB20
// Bump the loss tally for one OFFLINE game-mode, clamping the result to at least 1
// (the X360 re-stores 1 when the incremented word compares < 1 -- overflow guard).
// ====================================================================================
void Profile::AddLossForGameMode(GsmIO::EGameModeType leGameModeType)
{
    CGS_ASSERT((leGameModeType < GsmIO::E_MODE_OFFLINE_COUNT) && (leGameModeType > GsmIO::E_MODE_NONE),
        CgsDev::Assert::MessageAt(
            "( leGameModeType < GsmIO::E_MODE_OFFLINE_COUNT ) && ( leGameModeType > GsmIO::E_MODE_NONE )",
            KAC_ARTIST_ASSERT_FILE, 2303 /* ARTIST 0x8230FB54 */));

    ++maiLossesPerOfflineGameMode[leGameModeType];
    if (maiLossesPerOfflineGameMode[leGameModeType] < 1)
        maiLossesPerOfflineGameMode[leGameModeType] = 1;
}

// ====================================================================================
// Profile::GetNumRankWinsForGameMode  @ 0x8230FA40
// ====================================================================================
s32 Profile::GetNumRankWinsForGameMode(GsmIO::EGameModeType leGameModeType) const
{
    CGS_ASSERT((leGameModeType < GsmIO::E_MODE_OFFLINE_COUNT) && (leGameModeType > GsmIO::E_MODE_NONE),
        CgsDev::Assert::MessageAt(
            "( leGameModeType < GsmIO::E_MODE_OFFLINE_COUNT ) && ( leGameModeType > GsmIO::E_MODE_NONE )",
            KAC_ARTIST_ASSERT_FILE, 2195 /* ARTIST 0x8230FA74 */));

    return maiRankWinsPerOfflineGameMode[leGameModeType];
}

// ====================================================================================
// Profile::GetNumLossesForGameMode  @ 0x8230FAB0
// ====================================================================================
s32 Profile::GetNumLossesForGameMode(GsmIO::EGameModeType leGameModeType) const
{
    CGS_ASSERT((leGameModeType < GsmIO::E_MODE_OFFLINE_COUNT) && (leGameModeType > GsmIO::E_MODE_NONE),
        CgsDev::Assert::MessageAt(
            "( leGameModeType < GsmIO::E_MODE_OFFLINE_COUNT ) && ( leGameModeType > GsmIO::E_MODE_NONE )",
            KAC_ARTIST_ASSERT_FILE, 2212 /* ARTIST 0x8230FAE4 */));

    return maiLossesPerOfflineGameMode[leGameModeType];
}

// ====================================================================================
// Profile::AddTakedown  @ 0x82354C00
// Bump the total takedown count and the per-type tally.
// ====================================================================================
void Profile::AddTakedown(BrnGameState::ETakedownType leTakedownType)
{
    CGS_ASSERT(leTakedownType > BrnGameState::E_TAKEDOWN_NONE,
        CgsDev::Assert::MessageAt(
            "leTakedownType > BrnGameState::E_TAKEDOWN_NONE",
            KAC_ARTIST_ASSERT_FILE, 2285 /* ARTIST 0x82354C2C */));

    ++miTotalTakedownCount;
    ++maiTakedownTypeCounts[leTakedownType];
}

// ====================================================================================
// Profile::AddStuntElement  @ 0x8236AB00
// Record one completed stunt element. If the element was not already recorded and the
// county is valid, bump the per-county tally first; then insert into the type's set
// (Set<>::Insert is a no-op on a duplicate).
// ====================================================================================
void Profile::AddStuntElement(BrnGameState::StuntElementType leStuntElementType, CgsID lId,
                              BrnWorld::ECounty leCounty)
{
    CGS_ASSERT(leStuntElementType < BrnGameState::E_STUNT_ELEMENT_TYPE_COUNT,
               CgsDev::Assert::MessageAt("leStuntElementType < BrnGameState::E_STUNT_ELEMENT_TYPE_COUNT", "..\\..\\..\\GameSource\\GameState/Progression/BrnProfile.h", 2328));

    if (maStuntElements[leStuntElementType].Find(lId) == CgsContainers::Set<CgsID, 512u>::KU_INVALID &&
        leCounty < BrnWorld::E_COUNTY_VALID_COUNT)
    {
        ++maaiStuntCountsByCounty[leStuntElementType][leCounty];
    }

    maStuntElements[leStuntElementType].Insert(lId);
}

// ====================================================================================
// Profile::IsStuntElementDone  @ 0x823619B0
// True when the stunt element is already in its type's completed set.
// ====================================================================================
bool Profile::IsStuntElementDone(BrnGameState::StuntElementType leStuntElementType, CgsID lId) const
{
    return maStuntElements[leStuntElementType].Find(lId) != CgsContainers::Set<CgsID, 512u>::KU_INVALID;
}

// ====================================================================================
// Profile::GetStuntElementCount  @ 0x82361950
// Live count of one stunt-element set (no type-range assert on the X360).
// ====================================================================================
s32 Profile::GetStuntElementCount(BrnGameState::StuntElementType leStuntElementType) const
{
    return static_cast<s32>(maStuntElements[leStuntElementType].GetLength());
}

// ====================================================================================
// Profile::GetStuntElementCountByCounty  @ 0x82354D10
// The per-county tally for one stunt-element type (s16 slot, sign-extended on return).
// ====================================================================================
s32 Profile::GetStuntElementCountByCounty(BrnGameState::StuntElementType leStuntElementType,
                                          BrnWorld::ECounty leCounty) const
{
    CGS_ASSERT(leStuntElementType < BrnGameState::E_STUNT_ELEMENT_TYPE_COUNT,
        CgsDev::Assert::MessageAt(
            "leStuntElementType < BrnGameState::E_STUNT_ELEMENT_TYPE_COUNT",
            KAC_ARTIST_ASSERT_FILE, 2487 /* ARTIST 0x82354D40 */));
    CGS_ASSERT(leCounty < BrnWorld::E_COUNTY_VALID_COUNT,
        CgsDev::Assert::MessageAt(
            "leCounty < BrnWorld::E_COUNTY_VALID_COUNT",
            KAC_ARTIST_ASSERT_FILE, 2488 /* ARTIST 0x82354D64 */));

    return maaiStuntCountsByCounty[leStuntElementType][leCounty];
}

// ====================================================================================
// Profile::GetCarData  @ 0x82354950
// Checked indexed access into the owned-car records. The DWARF attests the const shape;
// the non-const overload (already committed in the declaration set) shares the body.
// ====================================================================================
const CarData* Profile::GetCarData(s32 liCarIndex) const
{
    CGS_ASSERT(liCarIndex >= 0 && liCarIndex < miCarCount,
        CgsDev::Assert::MessageAt(
            "liCarIndex >= 0 && liCarIndex < miCarCount",
            KAC_ARTIST_ASSERT_FILE, 1923 /* ARTIST 0x82354988 */));

    return &maCars[liCarIndex];
}

CarData* Profile::GetCarData(s32 liCarIndex)
{
    CGS_ASSERT(liCarIndex >= 0 && liCarIndex < miCarCount,
               "liCarIndex >= 0 && liCarIndex < miCarCount");

    return &maCars[liCarIndex];
}

// ====================================================================================
// Profile::GetTotalCarsToShutDown  @ 0x823549D0
// Count the rivals still in the E_STATE_UNLOCKED(1) state (the X360 walks all 64 rival
// records, 4-way unrolled, testing meState @ +0x10 == 1).
// ====================================================================================
s32 Profile::GetTotalCarsToShutDown() const
{
    s32 liTotal = 0;
    for (s32 liIndex = 0; liIndex < KI_MAX_RIVAL_COUNT; ++liIndex)
    {
        if (maRivals[liIndex].meState == RivalData::E_STATE_UNLOCKED)
            ++liTotal;
    }
    return liTotal;
}

// ====================================================================================
// Profile::GetEvent  @ 0x82354DA0
// Checked indexed access into the discovered-event records.
// ====================================================================================
const ProfileEvent* Profile::GetEvent(u32 luIndex) const
{
    CGS_ASSERT(luIndex < static_cast<u32>(miEventCount),
        CgsDev::Assert::MessageAt(
            "luIndex < static_cast<uint32_t>(miEventCount)",
            KAC_ARTIST_ASSERT_FILE, 3004 /* ARTIST 0x82354DD0 */));

    return &maEvents[luIndex];
}

// ====================================================================================
// Profile::GetMedalAchievedForEventWithID  @ 0x82354EB0
// The medal earned for the event with the given id: 0 (gold) when the rank-win flag is
// set, 1 (silver) for a non-rank win, 2 (bronze) for a special-event win, -1 when the
// event is unknown or unwon.
// ====================================================================================
s32 Profile::GetMedalAchievedForEventWithID(s32 liEventID) const
{
    for (s32 liIndex = 0; liIndex < miEventCount; ++liIndex)
    {
        const ProfileEvent& lEvent = maEvents[liIndex];
        if (lEvent.GetID() == static_cast<u32>(liEventID))
        {
            if (lEvent.IsFlagSet(ProfileEvent::E_FLAG_RANK_WIN))
                return 0;
            if (lEvent.IsFlagSet(ProfileEvent::E_FLAG_NON_RANK_WIN))
                return 1;
            if (lEvent.IsFlagSet(ProfileEvent::E_FLAG_WON_SPECIAL_EVENT_BEFORE))
                return 2;
            return -1;
        }
    }
    return -1;
}

// ====================================================================================
// Profile::GetTotalWinCount  @ 0x82354E10
// Tally every event's best medal into the three out-params (rank win beats non-rank win
// beats special-event win, mirroring the X360's else-if chain) and return the rank-win
// count.
// ====================================================================================
u32 Profile::GetTotalWinCount(u32& lruRankWins, u32& lruNonRankWins, u32& lruSpecialEventWins) const
{
    lruRankWins        = 0;
    lruNonRankWins     = 0;
    lruSpecialEventWins = 0;

    for (s32 liIndex = 0; liIndex < miEventCount; ++liIndex)
    {
        const ProfileEvent& lEvent = maEvents[liIndex];
        if (lEvent.IsFlagSet(ProfileEvent::E_FLAG_RANK_WIN))
        {
            ++lruRankWins;
        }
        else if (lEvent.IsFlagSet(ProfileEvent::E_FLAG_NON_RANK_WIN))
        {
            ++lruNonRankWins;
        }
        else if (lEvent.IsFlagSet(ProfileEvent::E_FLAG_WON_SPECIAL_EVENT_BEFORE))
        {
            ++lruSpecialEventWins;
        }
    }

    return lruRankWins;
}

// ====================================================================================
// Profile::GetPlayerBaseDeformAmount  @ 0x8230FBA8
// The persisted deform amount for the owned car with the given id (0.0 when not owned).
// The X360 tail-calls the (non-const) FindCar; const_cast keeps the DWARF-attested const
// shape of this getter.
// ====================================================================================
f32 Profile::GetPlayerBaseDeformAmount(CgsID lCarId) const
{
    CarData* lpCarData = const_cast<Profile*>(this)->FindCar(lCarId);
    if (lpCarData)
        return lpCarData->mfUnlockDeformedAmount;

    return 0.0f;
}

// ====================================================================================
// Profile::RepairUnlockedVehicle  @ 0x82361EB0
// Clear the stored deform/damage of the just-repaired owned car (the X360 inlines the
// FindCar loop, asserts the record exists, then zeroes the deform amount @ +0x0C).
// ====================================================================================
CarData* Profile::RepairUnlockedVehicle(CgsID lCarId)
{
    CarData* lpCarData = FindCar(lCarId);

    CGS_ASSERT(lpCarData, CgsDev::Assert::MessageAt("lpCarData", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProfile.cpp", 671));
    lpCarData->mfUnlockDeformedAmount = 0.0f;   // the X360 stores through even after a failed assert
    return lpCarData;
}

// ====================================================================================
// Profile::RecordPropHit  @ 0x82361C48
// Set the hit bit for one prop (bit index = 600 * zone + prop) in the 300000-bit prop
// bit array. The two range asserts stream "Zone Index: <z> Prop index: <p>\n" on the
// X360 (collapsed to the leading rodata fragment per project convention); the third is
// the BitArray SetBit guard (CgsBitArray.h:222).
// ====================================================================================
void Profile::RecordPropHit(s32 liZoneIndex, s32 liPropIndex)
{
    CGS_ASSERT(liZoneIndex < 500, "Zone Index: ");   // BrnProfile.h:3137
    CGS_ASSERT(liPropIndex < 600, "Zone Index: ");   // BrnProfile.h:3138

    const u32 luBitIndex = static_cast<u32>(600 * liZoneIndex + liPropIndex);
    CGS_ASSERT(luBitIndex < 300000u, "Index: ");     // CgsBitArray.h:222 SetBit guard
    mabHitPropBitArray.SetBit(luBitIndex);
}

// ====================================================================================
// Profile::GetSeenAllEventTypeWonMessage  @ 0x82361F58
// leModeType is BrnProgression::RaceEventData::EModeType (not yet homed; the X360
// compares against E_MODE_COUNT == 6).
// ====================================================================================
bool Profile::GetSeenAllEventTypeWonMessage(s32 leModeType) const
{
    CGS_ASSERT(leModeType > -1, CgsDev::Assert::MessageAt("leModeType > RaceEventData::E_MODE_INVALID", "..\\..\\..\\GameSource\\GameState/Progression/BrnProfile.h", 3320));   // :3320
    CGS_ASSERT(leModeType < 6,  CgsDev::Assert::MessageAt("leModeType < RaceEventData::E_MODE_COUNT", "..\\..\\..\\GameSource\\GameState/Progression/BrnProfile.h", 3321));     // :3321
    CGS_ASSERT(static_cast<u32>(leModeType) < 6u, "invalid index : < 6");        // CgsBitArray.h:203 Get guard

    return mSeenCompleteAllEventTypeArray.IsBitSet(static_cast<u32>(leModeType));
}

// ====================================================================================
// Profile::SetSeenAllEventTypeWonMessage  @ 0x823620A8
// ====================================================================================
void Profile::SetSeenAllEventTypeWonMessage(s32 leModeType)
{
    CGS_ASSERT(leModeType > -1, CgsDev::Assert::MessageAt("leModeType > RaceEventData::E_MODE_INVALID", "..\\..\\..\\GameSource\\GameState/Progression/BrnProfile.h", 3336));   // :3336
    CGS_ASSERT(leModeType < 6,  CgsDev::Assert::MessageAt("leModeType < RaceEventData::E_MODE_COUNT", "..\\..\\..\\GameSource\\GameState/Progression/BrnProfile.h", 3337));     // :3337
    CGS_ASSERT(static_cast<u32>(leModeType) < 6u, "Index: ");                    // CgsBitArray.h:222 SetBit guard

    mSeenCompleteAllEventTypeArray.SetBit(static_cast<u32>(leModeType));
}

// ====================================================================================
// Profile::GetSeenTrophyUnlockSequence  @ 0x82475A30
// leUnlockType is BrnProgression::TrophyUnlockData::UnlockType (DWARF); only NONE=0 /
// COUNT=35 are committed, so the s32 underlying value is taken.
// ====================================================================================
bool Profile::GetSeenTrophyUnlockSequence(s32 leUnlockType) const
{
    CGS_ASSERT(leUnlockType > 0,  CgsDev::Assert::MessageAt("leUnlockType > BrnProgression::TrophyUnlockData::E_UNLOCKTYPE_NONE", "..\\..\\..\\GameSource\\GameState/Progression/BrnProfile.h", 3287));  // :3287
    CGS_ASSERT(leUnlockType < 35, CgsDev::Assert::MessageAt("leUnlockType < BrnProgression::TrophyUnlockData::E_UNLOCKTYPE_COUNT", "..\\..\\..\\GameSource\\GameState/Progression/BrnProfile.h", 3288)); // :3288
    CGS_ASSERT(static_cast<u32>(leUnlockType) < 35u, "invalid index : < 35");    // CgsBitArray.h:203 Get guard

    return mSeenTrophyAwardBitArray.IsBitSet(static_cast<u32>(leUnlockType));
}

// ====================================================================================
// Profile::SetSeenTrophyUnlockSequence  @ 0x824BAA30
// ====================================================================================
void Profile::SetSeenTrophyUnlockSequence(s32 leUnlockType)
{
    CGS_ASSERT(leUnlockType > 0,  CgsDev::Assert::MessageAt("leUnlockType > BrnProgression::TrophyUnlockData::E_UNLOCKTYPE_NONE", "..\\..\\..\\GameSource\\GameState/Progression/BrnProfile.h", 3303));  // :3303
    CGS_ASSERT(leUnlockType < 35, CgsDev::Assert::MessageAt("leUnlockType < BrnProgression::TrophyUnlockData::E_UNLOCKTYPE_COUNT", "..\\..\\..\\GameSource\\GameState/Progression/BrnProfile.h", 3304)); // :3304
    CGS_ASSERT(static_cast<u32>(leUnlockType) < 35u, "Index: ");                 // CgsBitArray.h:222 SetBit guard

    mSeenTrophyAwardBitArray.SetBit(static_cast<u32>(leUnlockType));
}

// ====================================================================================
// Profile::SetTrainingAlreadySeen  @ 0x82361B20
// Mark one training tip as seen in the 256-bit training bit array.
// ====================================================================================
void Profile::SetTrainingAlreadySeen(ETrainingType leTrainingType)
{
    CGS_ASSERT(leTrainingType >= 0 && leTrainingType < E_TRAINING_TYPE_COUNT,
               CgsDev::Assert::MessageAt("leTrainingType >= 0 && leTrainingType < E_TRAINING_TYPE_COUNT", "..\\..\\..\\GameSource\\GameState/Progression/BrnProfile.h", 2761));  // :2761
    CGS_ASSERT(static_cast<u32>(leTrainingType) < 256u, "Index: ");               // CgsBitArray.h:222 SetBit guard

    maHasPlayerSeenTraining.SetBit(static_cast<u32>(leTrainingType));
}

// ====================================================================================
// Profile::IsDeveloperChallengeComplete   (declared BrnProfile.h:611)
//
// The read twin of SetDeveloperChallengeComplete below. It has NO out-of-line symbol in the
// ARTIST image -- every call site folds it inline -- so the body is recovered from those folds.
// The clearest is BrnGameState::DeveloperChallengeManager::OnEventWin @0x8238DB10, which folds it
// FIVE times, once per challenge it can trip (0x8238DC00, 0x8238DCA4, 0x8238DD70, 0x8238DF38,
// 0x8238DFD8). Each fold is byte-identical apart from the mask:
//
//     lwz     r11, 0xA4(this)        ; mpProgressionManager
//     add     r11, r11, 0x1D970      ; + 121200
//     ld      r11, 0(r11)            ; the ONE 64-bit bit field, loaded whole
//     cmpldi  r11, 0 ; beq -> result = 0
//     rlwinm  r11, r11, 0,<b>,<b>    ; isolate the challenge's bit in the low word
//     cmpldi  r11, 0 ; result = (bit != 0)
//
// 121200 is the SAME slot SetDeveloperChallengeComplete writes: that function reaches the array
// as `this + 0x20000 - 0x2800` == Profile+0x1D800 (asm 0x82362330), and DeveloperChallengeManager
// holds the ProgressionManager, whose embedded Profile starts at +0x170 (the manager's own
// `addic. r11, r11, 0x170` GetProfile() assert) -- 0x170 + 0x1D800 == 0x1D970 exactly.
// The five masks decode straight onto the five challenge indices the same function then passes to
// SetChallengeCompleted, which pins index == bit number:
//     rlwinm 0,23,23 -> 1<<8  -> li r4,8      rlwinm 0,22,22 -> 1<<9  -> li r4,9
//     rlwinm 0,17,17 -> 1<<14 -> li r4,0xE    rlwinm 0,28,28 -> 1<<3  -> li r4,3
//     rlwinm 0,27,27 -> 1<<4  -> li r4,4
// so the body is FastBitArray<15>::IsBitSet(liChallengeIndex) and nothing else. The console's
// "whole field != 0" pre-test in front of the mask is the compiler's own fast reject on a
// single-field array (a set bit implies a non-zero field); it changes no result.
//
// NO range assert is reproduced: unlike the Set twin, not one of the five folds emits the
// "Out of range developer challnge" assert pair, and every fold has a constant index, so there is
// no evidence either way about a guard the compiler would have folded away. Inventing one would be
// inventing behaviour; the caller-side indices are the enum's own.
// ====================================================================================
bool Profile::IsDeveloperChallengeComplete(s32 liChallengeIndex) const
{
    return mDeveloperChallengesCompleted.IsBitSet(static_cast<u32>(liChallengeIndex));
}

// ====================================================================================
// Profile::SetDeveloperChallengeComplete  @ 0x823621F0
// Set one developer-challenge-completed bit. The two range asserts stream "Out of range
// developer challnge<i>.\n" ('challnge' typo is X360 rodata); the third is the
// FastBitArray SetBit guard (CgsFastBitArray.h:431). 15 == GsmIO::E_DEVELOPER_CHALLENGE_COUNT.
// ====================================================================================
void Profile::SetDeveloperChallengeComplete(s32 liChallengeIndex)
{
    CGS_ASSERT(liChallengeIndex >= 0, "Out of range developer challnge");   // :3394
    CGS_ASSERT(liChallengeIndex < 15, "Out of range developer challnge");   // :3395
    CGS_ASSERT(liChallengeIndex < 15, "Index ");                            // CgsFastBitArray.h:431 SetBit guard

    mDeveloperChallengesCompleted.SetBit(static_cast<u32>(liChallengeIndex));
}

// ====================================================================================
// Profile::GetTargetEvent  @ 0x82371458
// The target-event-score record for the given event id, or NULL (the X360 re-reads the
// checked live count every iteration -- the Array<>::GetLength constructed-assert,
// CgsArray.h:336, fires from the loop guard).
// ====================================================================================
BrnGameState::GameStateModuleIO::TargetEventScore* Profile::GetTargetEvent(CgsID lEventId)
{
    for (u32 luIndex = 0; luIndex < maTargetEventScores.GetLength(); ++luIndex)
    {
        if (maTargetEventScores[luIndex].mEventId == lEventId)
            return &maTargetEventScores[luIndex];
    }
    return 0;
}

// ====================================================================================
// Profile::SetTargetEventScore  @ 0x823714F8
// Store (or overwrite) the target score for one event: find the record by id, reserve a
// fresh slot when absent (asserting the reserve succeeded), then fill the record --
// event id @ +0x18, score @ +0x20, and the 24-byte by-value head block wholesale.
// ====================================================================================
void Profile::SetTargetEventScore(BrnGameState::GameStateModuleIO::TargetEventScore::OpaqueHead lHead,
                                  CgsID lEventId, s32 liScore)
{
    BrnGameState::GameStateModuleIO::TargetEventScore* lpTargetEventScore = 0;

    for (u32 luIndex = 0; luIndex < maTargetEventScores.GetLength(); ++luIndex)
    {
        if (maTargetEventScores[luIndex].mEventId == lEventId)
        {
            lpTargetEventScore = &maTargetEventScores[luIndex];
            break;
        }
    }

    if (!lpTargetEventScore)
    {
        lpTargetEventScore = maTargetEventScores.AddNew();
        CGS_ASSERT(lpTargetEventScore, CgsDev::Assert::MessageAt("lpTargetEventScore", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProfile.cpp", 1604));   // BrnProfile.cpp:1604
    }

    lpTargetEventScore->mEventId = lEventId;
    lpTargetEventScore->miScore  = liScore;
    lpTargetEventScore->mHead    = lHead;
}

// ====================================================================================
// Profile::RemoveTargetEventScore  @ 0x82371600
// Drop the target-event-score record for one event (unordered EraseFast); the streamed
// "doesn't exist" assert fires when the id is unknown.
// ====================================================================================
void Profile::RemoveTargetEventScore(CgsID lEventId)
{
    u32 luIndex = 0;
    while (luIndex < maTargetEventScores.GetLength() &&
           maTargetEventScores[luIndex].mEventId != lEventId)
    {
        ++luIndex;
    }

    if (luIndex < maTargetEventScores.GetLength())
    {
        maTargetEventScores.EraseFast(luIndex);
    }
    else
    {
        CGS_ASSERT(false, "Tried to remove an event target that doesn't exist\n");   // BrnProfile.cpp:1641
    }
}

// ====================================================================================
// Profile::SetEventScoreToUpload  @ 0x82371740
// Queue (or better) the player's score for one leaderboard event. Burning Route (mode 5)
// scores are times -- keep the LOWER; Stunt Attack (mode 7) scores are points -- keep the
// HIGHER; any other mode fires the streamed "without leaderboard" assert and overwrites.
// A missing record reserves a fresh slot (asserting the reserve succeeded).
// ====================================================================================
void Profile::SetEventScoreToUpload(CgsID lEventId, s32 liScore, GsmIO::EGameModeType leGameMode)
{
    BrnNetwork::LocalEventScoreUploadData* lpEventScoreToUpload = 0;

    u32 luIndex = 0;
    while (luIndex < maEventScoresToUpload.GetLength())
    {
        if (maEventScoresToUpload[luIndex].mu64EventID == lEventId)
        {
            lpEventScoreToUpload = &maEventScoresToUpload[luIndex];
            break;
        }
        ++luIndex;
    }

    if (lpEventScoreToUpload)
    {
        if (leGameMode == GsmIO::E_MODE_BURNING_ROUTE)
        {
            if (liScore >= static_cast<s32>(lpEventScoreToUpload->muScore))
                return;   // times: only a LOWER score replaces the pending upload
        }
        else if (leGameMode == GsmIO::E_MODE_STUNT_ATTACK)
        {
            if (liScore <= static_cast<s32>(lpEventScoreToUpload->muScore))
                return;   // points: only a HIGHER score replaces the pending upload
        }
        else
        {
            // X360 streams "Trying to set score to upload for event without leaderboard: <mode>\n".
            CGS_ASSERT(false, "Trying to set score to upload for event without leaderboard: ");   // BrnProfile.cpp:1698
        }
    }

    if (!lpEventScoreToUpload)
    {
        lpEventScoreToUpload = maEventScoresToUpload.AddNew();
        CGS_ASSERT(lpEventScoreToUpload, CgsDev::Assert::MessageAt("lpEventScoreToUpload", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProfile.cpp", 1714));   // BrnProfile.cpp:1714
    }

    lpEventScoreToUpload->mu64EventID = lEventId;
    lpEventScoreToUpload->muScore     = static_cast<u32>(liScore);
    lpEventScoreToUpload->meGameMode  = static_cast<s32>(leGameMode);
}

// ====================================================================================
// Profile::RemoveEventScoreToUpload  @ 0x823718F0
// Drop the pending upload record for one event (unordered EraseFast); the streamed
// "doesn't exist" assert fires when the id is unknown.
// ====================================================================================
void Profile::RemoveEventScoreToUpload(CgsID lEventId)
{
    u32 luIndex = 0;
    while (luIndex < maEventScoresToUpload.GetLength() &&
           maEventScoresToUpload[luIndex].mu64EventID != lEventId)
    {
        ++luIndex;
    }

    if (luIndex < maEventScoresToUpload.GetLength())
    {
        maEventScoresToUpload.EraseFast(luIndex);
    }
    else
    {
        CGS_ASSERT(false, "Tried to remove an upload event score that doesn't exist\n");   // BrnProfile.cpp:1751
    }
}

// ====================================================================================
// SplitArray<TSrc, TDst>  -- Profile::Serialise save-image splitters.
//
// Four explicit specialisations, one per X360 body. Each walks the live progression array
// and copies every record verbatim into either the base-game run (lpBase) or the DLC run
// (lpDlc), preserving order; the DLC run is capped at liMaxDlcCount. The id-keyed records
// (Car/Livery/Rival) classify a record as DLC via ProfileDLC1::IsDLCCarId (the packed 64-bit
// id at record +0); events classify by an id threshold. De-optimised from the X360 (which
// open-codes the per-record word copies); the whole-record memcpy reproduces the exact byte
// stride each body copies (0x18 / 0x18 / 0x38 / 0x08).
// ====================================================================================

// ------------------------------------------------------------------------------------
// SplitArray<CarData, BrnGuiSaveLoad::CarData>  @ 0x823696F0
// ------------------------------------------------------------------------------------
template<>
void SplitArray<CarData, BrnGuiSaveLoad::CarData>(
        s32 liCount, const CarData* lpSrc,
        s32* lpiBaseCount, BrnGuiSaveLoad::CarData* lpBase,
        s32* lpiDlcCount,  BrnGuiSaveLoad::CarData* lpDlc, s32 liMaxDlcCount)
{
    s32 liBaseIndex = 0;
    s32 liDlcIndex  = 0;

    for (s32 liRemaining = liCount; liRemaining > 0; --liRemaining)
    {
        BrnGuiSaveLoad::CarData* lpDst;
        if (BrnGuiSaveLoad::ProfileDLC1::IsDLCCarId(*lpSrc))   // reads the packed car id @ +0
        {
            CGS_ASSERT(liDlcIndex < liMaxDlcCount, "liDLCIndex < liMaxDlcCount");   // BrnProfile.cpp:56
            lpDst = &lpDlc[liDlcIndex];
            ++liDlcIndex;
        }
        else
        {
            lpDst = &lpBase[liBaseIndex];
            ++liBaseIndex;
        }
        memcpy(lpDst, lpSrc, sizeof(BrnGuiSaveLoad::CarData));   // three-qword record copy
        ++lpSrc;
    }

    *lpiBaseCount = liBaseIndex;
    *lpiDlcCount  = liDlcIndex;
}

// ------------------------------------------------------------------------------------
// SplitArray<LiveryData, BrnGuiSaveLoad::LiveryData>  @ 0x823697C0
// Identical structure to the CarData body (0x18 stride); the DLC test reads the record's id
// at +0, which for a LiveryData record is mBaseCarId.
// ------------------------------------------------------------------------------------
template<>
void SplitArray<LiveryData, BrnGuiSaveLoad::LiveryData>(
        s32 liCount, const LiveryData* lpSrc,
        s32* lpiBaseCount, BrnGuiSaveLoad::LiveryData* lpBase,
        s32* lpiDlcCount,  BrnGuiSaveLoad::LiveryData* lpDlc, s32 liMaxDlcCount)
{
    s32 liBaseIndex = 0;
    s32 liDlcIndex  = 0;

    for (s32 liRemaining = liCount; liRemaining > 0; --liRemaining)
    {
        BrnGuiSaveLoad::LiveryData* lpDst;
        // The X360 passes the raw record pointer to IsDLCCarId, which reads the id at +0.
        if (BrnGuiSaveLoad::ProfileDLC1::IsDLCCarId(reinterpret_cast<const CarData&>(*lpSrc)))
        {
            CGS_ASSERT(liDlcIndex < liMaxDlcCount, "liDLCIndex < liMaxDlcCount");   // BrnProfile.cpp:56
            lpDst = &lpDlc[liDlcIndex];
            ++liDlcIndex;
        }
        else
        {
            lpDst = &lpBase[liBaseIndex];
            ++liBaseIndex;
        }
        memcpy(lpDst, lpSrc, sizeof(BrnGuiSaveLoad::LiveryData));   // three-qword record copy
        ++lpSrc;
    }

    *lpiBaseCount = liBaseIndex;
    *lpiDlcCount  = liDlcIndex;
}

// ------------------------------------------------------------------------------------
// SplitArray<RivalData, BrnGuiSaveLoad::RivalData>  @ 0x82369890
// 0x38 stride (seven-qword copy). The DLC test reads the record's id at +0 (mRivalId).
// ------------------------------------------------------------------------------------
template<>
void SplitArray<RivalData, BrnGuiSaveLoad::RivalData>(
        s32 liCount, const RivalData* lpSrc,
        s32* lpiBaseCount, BrnGuiSaveLoad::RivalData* lpBase,
        s32* lpiDlcCount,  BrnGuiSaveLoad::RivalData* lpDlc, s32 liMaxDlcCount)
{
    s32 liBaseIndex = 0;
    s32 liDlcIndex  = 0;

    for (s32 liRemaining = liCount; liRemaining > 0; --liRemaining)
    {
        BrnGuiSaveLoad::RivalData* lpDst;
        if (BrnGuiSaveLoad::ProfileDLC1::IsDLCCarId(reinterpret_cast<const CarData&>(*lpSrc)))
        {
            CGS_ASSERT(liDlcIndex < liMaxDlcCount, "liDLCIndex < liMaxDlcCount");   // BrnProfile.cpp:56
            lpDst = &lpDlc[liDlcIndex];
            ++liDlcIndex;
        }
        else
        {
            lpDst = &lpBase[liBaseIndex];
            ++liBaseIndex;
        }
        memcpy(lpDst, lpSrc, sizeof(BrnGuiSaveLoad::RivalData));   // seven-qword record copy
        ++lpSrc;
    }

    *lpiBaseCount = liBaseIndex;
    *lpiDlcCount  = liDlcIndex;
}

// ------------------------------------------------------------------------------------
// SplitArray<ProfileEvent, BrnGuiSaveLoad::ProfileEvent>  @ 0x82369988
// 0x08 stride (two-dword copy). Events are NOT classified via IsDLCCarId: a base-game event
// has an id <= 0x975E0, anything larger is a DLC event (the X360 inlines the compare on the
// event id at +0). The base branch is taken first; only the DLC branch bounds-checks.
// ------------------------------------------------------------------------------------
template<>
void SplitArray<ProfileEvent, BrnGuiSaveLoad::ProfileEvent>(
        s32 liCount, const ProfileEvent* lpSrc,
        s32* lpiBaseCount, BrnGuiSaveLoad::ProfileEvent* lpBase,
        s32* lpiDlcCount,  BrnGuiSaveLoad::ProfileEvent* lpDlc, s32 liMaxDlcCount)
{
    const u32 KU_DLC_EVENT_ID_THRESHOLD = 0x975E0u;   // X360 cmplwi 0x975E0 on the event id

    s32 liBaseIndex = 0;
    s32 liDlcIndex  = 0;

    for (s32 liRemaining = liCount; liRemaining > 0; --liRemaining)
    {
        BrnGuiSaveLoad::ProfileEvent* lpDst;
        if (lpSrc->GetID() <= KU_DLC_EVENT_ID_THRESHOLD)
        {
            lpDst = &lpBase[liBaseIndex];
            ++liBaseIndex;
        }
        else
        {
            CGS_ASSERT(liDlcIndex < liMaxDlcCount, "liDLCIndex < liMaxDlcCount");   // BrnProfile.cpp:56
            lpDst = &lpDlc[liDlcIndex];
            ++liDlcIndex;
        }
        memcpy(lpDst, lpSrc, sizeof(BrnGuiSaveLoad::ProfileEvent));   // two-dword record copy
        ++lpSrc;
    }

    *lpiBaseCount = liBaseIndex;
    *lpiDlcCount  = liDlcIndex;
}

// ------------------------------------------------------------------------------------
// Profile::IsStartOfGameDeformActive
//
// ⭐⭐ CORRECTION. The committed comment at BrnCarSelectManager.cpp's call site read
// "FLAG: Profile + 118401 (de-inlined byte read)". 118401 is NOT a Profile offset -- the X360
// (CarSelectManager::ReallyEnterJunkyardAtStartOfGame @0x823931F8) reads
//     if ( *(mpProgressionManager + 118401) )
// and mProfile is embedded at ProgressionManager + 0x170 (368), which is independently proven
// two lines from the same asm (EnterJunkyard @0x82398508 computes `*(a1+16) + 368` as the
// Profile `this`). 118401 - 368 == 118033 == mbIsNewProfile.
//
// So the start-of-game deform is gated on THE PROFILE BEING BRAND NEW: on a first-ever boot
// the start car is handed out pre-deformed (the same asm then writes 0.85f into the CarData's
// mfUnlockDeformedAmount), and on any later boot it is not. That is exactly the user-visible
// "your first car is a wreck" behaviour, and it was pointing at the wrong object.
// (The accessor NAME is this repo's -- the X360 inlines the byte read and the DWARF declares
// no such member function -- but the byte it reads is now attested.)
// ------------------------------------------------------------------------------------
bool Profile::IsStartOfGameDeformActive() const
{
    return mbIsNewProfile;
}

// ------------------------------------------------------------------------------------
// Profile::SetChosenLiveryIdForBaseCar (single-CgsID form)
//
// ⚠️⚠️ SIGNATURE DISCREPANCY, RECORDED NOT PAPERED OVER. This overload is what the CONSOLE
// actually calls: EnterJunkyard @0x82398508 ends
//     Profile::SetChosenLiveryIdForBaseCar(mpProgressionManager + 368, <the one CgsID>)
// and the callee @0x82359C90 reconstructs a SINGLE 64-bit value from its argument halves
// (`v5 = __PAIR64__(a3, a2)`). The repo's OTHER overload -- SetChosenLiveryIdForBaseCar(CgsID,
// CgsID), bodied in this file and attributed to that same 0x82359C90 -- therefore has one
// argument too many for the address it claims. Do NOT assume the two-arg body is faithful;
// it needs its own look. Until then this form forwards with base == chosen, which is what the
// two-arg body's own in-file caller (line ~307) already does and what "pin the chosen livery to
// the base car" means.
// ------------------------------------------------------------------------------------
void Profile::SetChosenLiveryIdForBaseCar(CgsID lBaseCarId)
{
    SetChosenLiveryIdForBaseCar(lBaseCarId, lBaseCarId);
}

// ------------------------------------------------------------------------------------
// Profile::SetRoadRuleNetworkHighScores  (callee of ProgressionManager::SetRoadRuleNetworkHighScores
// @0x82311430) -- wholesale copy of the 64-entry ChallengeHighScoreEntry table into
// maNetworkChallengeData (Profile+96472). X360 XMemCpy size 3584 == 64 * 56.
// ------------------------------------------------------------------------------------
void Profile::SetRoadRuleNetworkHighScores(const BrnStreetData::ChallengeHighScoreEntry* lpaChallengeHighScores)
{
    memcpy(maNetworkChallengeData, lpaChallengeHighScores, sizeof(maNetworkChallengeData));
}

// ------------------------------------------------------------------------------------
// Profile::SetRoadRuleChallengeData  (callee of ProgressionManager::SetRoadRuleChallengeData)
// -- wholesale copy of the 64-entry ChallengePlayerScoreEntry table into maChallengeData
// (Profile+100056). X360 XMemCpy size 2560 == 64 * 40.
// ------------------------------------------------------------------------------------
void Profile::SetRoadRuleChallengeData(const BrnStreetData::ChallengePlayerScoreEntry* lpaChallengeScores)
{
    memcpy(maChallengeData, lpaChallengeScores, sizeof(maChallengeData));
}

// ====================================================================================
// Profile::HasPlayerCompletedFreeburnChallenge  @ 0x82371338
// A one-line forwarder: `addis r3,r3,1 / addi r3,r3,-0x5850` == this + 42928 ==
// maFreeBurnChallengeData, `std r4, arg_18` == the 64-bit CgsID spilled so its ADDRESS can be
// handed to Array<CgsID,2000>::Contains (which takes a const T&), then a tail call.
// Callers: ChallengeManager::CheckForOnlineChallengeUnlocks / ::OnProfileLoaded / ::EndChallenge
// and ChallengeManagerDebugComponent::CompleteAllChallenges.
// ====================================================================================
bool Profile::HasPlayerCompletedFreeburnChallenge(CgsID lChallengeID) const
{
    return maFreeBurnChallengeData.Contains(lChallengeID);
}

// ====================================================================================
// Profile::CompleteFreeburnChallenge  @ 0x82371368
// Record one freeburn challenge as complete and return the array's NEW live length.
//   if (Contains(id)) assert("Trying to mark a freeburn challenge as complete twice",
//                            BrnProfile.cpp:1122)      -- an assert, NOT a guard: the console
//                                                         Appends the duplicate anyway.
//   Append(id)
//   return maFreeBurnChallengeData.GetLength()          -- `lwz r11, 0x3E80(r28)`, the count word
//                                                         after the 2000 8-byte elements, with
//                                                         Array::GetLength's own -1-sentinel
//                                                         assert (CgsArray.h:336) in front of it.
// The X360 streams the duplicate message through the assert message buffer; per project
// convention the dynamic message collapses to the static CGS_ASSERT string.
// ====================================================================================
u32 Profile::CompleteFreeburnChallenge(CgsID lChallengeID)
{
    CGS_ASSERT(!maFreeBurnChallengeData.Contains(lChallengeID),
               "Trying to mark a freeburn challenge as complete twice");

    maFreeBurnChallengeData.Append(lChallengeID);

    return maFreeBurnChallengeData.GetLength();
}

// ====================================================================================
// Profile::GetGameModeTypeCompletedAmountSinceTheStart  @ 0x82354B98
// `if (mode <= -1) assert("lEGameModeType > GsmIO::E_MODE_NONE", BrnProfile.h:2108);`
// then `return *(4 * (mode + 84) + this)` == maGameModeTypeAmountCompletedSinceTheStart[mode]
// (base +336 == 84 * 4). Its one caller is ProgressionManager::OnEventFinishUpdateProfile
// @0x823A0040, which compares it against GetGameModeTypeAmount to decide the
// "every event of this mode is completed" trophy.
// ====================================================================================
s32 Profile::GetGameModeTypeCompletedAmountSinceTheStart(GsmIO::EGameModeType lEGameModeType) const
{
    CGS_ASSERT(lEGameModeType > -1,
        CgsDev::Assert::MessageAt(
            "lEGameModeType > GsmIO::E_MODE_NONE", KAC_ARTIST_ASSERT_FILE, 2108 /* ARTIST 0x82354BC4 */));

    return maGameModeTypeAmountCompletedSinceTheStart[lEGameModeType];
}

   // anonymous namespace

} // namespace BrnProgression

// FOLDED FROM BrnProfile_SaveImage.cpp
// =====================================================================================
// BrnProgression::Profile::Serialise  @0x8237C1F0
// BrnProgression::Profile::Deserialise @0x8237D308
//   GameSource/Unity/../GameState/Progression/BrnProfile.cpp
//
// The progression SAVE-IMAGE CODEC: the live BrnProgression::Profile <-> the two console
// save images (BrnGuiSaveLoad::Profile, 118064 bytes, and BrnGuiSaveLoad::ProfileDLC1,
// 9800 bytes). The console's own home file for both bodies is BrnProfile.cpp -- every
// assert in the pair bakes
//   "d:\p4\b5_main\burnout\main\code\gamesource\unity\../GameState/Progression/BrnProfile.cpp"
// (Deserialise's version assert is BrnProfile.cpp:2044, its developer-challenge assert
// BrnProfile.cpp:2292, SplitArray's DLC bound BrnProfile.cpp:56). They live in this
// per-function TU rather than in BrnProfile.cpp itself for the same reason the
// StreetManager _wB/_wC bodies are split out: the pair is ~700 lines of console code and
// BrnProfile.cpp is already 1500. They are kept TOGETHER in ONE TU deliberately -- they
// are exact inverses over ONE serialised layout table, and duplicating that table across
// two TUs is precisely how a save codec drifts out of sync.
//
// SOURCE OF TRUTH: the X360 ARTIST **assembly** (0x8237C1F0 / 0x8237D308), store for
// store. The Hex-Rays pseudocode for both suffered "local variable allocation has failed"
// and is wrong in several places that matter (it renders the DLC1 image's qword-scaled
// offsets as dword ones, and reports memcpy sizes of 12318/9601 where the asm immediates
// are 0x3018/0x2580 == 12312/9600). Nothing below is taken from it.
//
// -------------------------------------------------------------------------------------
// THE TWO ADDRESS SPACES (the project's classic bug class -- read this before editing)
// -------------------------------------------------------------------------------------
// * The LIVE Profile is reached BY NAME. Its x64 layout is NOT the console's (this TU's
//   probe measured sizeof(Profile) == 120832 on x64 vs the console's 120848), so no
//   console offset may ever be applied to it.
// * The STORED images are fixed-width CONSOLE-FORMAT byte streams -- external serialised
//   data whose layout is fixed by the save file, not by a C++ class. Their offsets are
//   therefore hard console constants (this is the AGENTS.md serialised-blob exception),
//   and they are NOT allowed to move with x64 widths: BrnGuiSaveLoad::Profile's committed
//   recon pins the same numbers from the other side (muManifestCount @+0x268 == the base
//   ProfileEvent count word this TU writes, maVersionManifest @+0x7070 == the base
//   ProfileEvent array this TU fills), and ValidateProfile @0x824EFE30 reads them there.
//   Every image access below goes through the named-offset helpers, never a bare cast.
//
// Two live members are WIDER/NARROWER on x64 than their console serialised slot; both are
// handled explicitly and called out at their site (mPlayerLicencePicture 40 vs 28,
// maaMugshotInfo galleries 1124 vs 1128). Every other block is size-identical and is
// static_assert'ed against the X360 memcpy immediate, so a future layout change fails the
// gate instead of silently overrunning the next field in the image.
// =====================================================================================

#include "BrnProfile.h"

#include "GameSource/Gui/SaveLoad/BrnGuiSaveLoadProfile.h"       // BrnGuiSaveLoad::Profile (image width/version)
#include "GameSource/Gui/SaveLoad/BrnGuiSaveLoadProfileDLC1.h"   // BrnGuiSaveLoad::ProfileDLC1 (+ IsDLCCarId)
#include "GameShared/GameClasses/Core/CgsAssert.h"               // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"   // FastBitArray<120u> over the DLC1 slot
#include "GameShared/GameClasses/Network/Texture/CgsNetworkTexture.h"  // licence-picture descriptor rebuild
#include <string.h>   // memset / memcpy

namespace BrnProgression
{

namespace
{

// -------------------------------------------------------------------------------------
// The two serialised layouts.
//
// Every constant here is an immediate lifted from the X360 assembly of the Serialise /
// Deserialise pair, which agree field-for-field (Deserialise is the exact mirror). The
// progression image's field offsets run exactly 16 bytes BELOW the live member offsets
// from +0x60 onwards -- the console profile carries 16 bytes of live-only head that the
// image does not (`subf r8, r30, r31` + the -0x10 deltas throughout both bodies).
// -------------------------------------------------------------------------------------
namespace SaveImage
{
    // ---- BrnGuiSaveLoad::Profile (118064 bytes) ----
    const u32 KU_VERSION                        = 0;        // stw 0x1C
    const u32 KU_NAME                           = 4;        // memcpy 0x20
    const u32 KU_CAR_POSITION                   = 48;       // 0x30, two std
    const u32 KU_CAR_DIRECTION                  = 64;       // 0x40, two std
    const u32 KU_SPAWN_CAR_ID                   = 80;       // 0x50
    const u32 KU_SPAWN_WHEEL_ID                 = 88;       // 0x58
    const u32 KU_ROAD_RULES_DOWNLOAD_TIMESTAMP  = 96;       // 0x60
    const u32 KU_DISTANCE_DRIVEN_ONLINE         = 100;      // 0x64
    const u32 KU_DISTANCE_DRIVEN_OFFLINE        = 104;      // 0x68
    const u32 KU_IN_CAR_TIME_PLAYED             = 108;      // 0x6C
    const u32 KU_CURRENT_PROGRESSION_RANK       = 112;      // 0x70 (lbz/stb)
    const u32 KU_POWER_PARKING_BEST             = 113;      // 0x71
    const u32 KU_POWER_PARKING_BETWEEN          = 114;      // 0x72
    const u32 KU_BEST_NEW_BURNOUT_CHAIN         = 116;      // 0x74
    const u32 KU_MODE_AMOUNT                    = 120;      // 0x78  \  17 words each; the
    const u32 KU_MODE_DISCOVERED                = 188;      // 0xBC   > 18th (DLC island)
    const u32 KU_MODE_COMPLETED                 = 256;      // 0x100  / slot rides the DLC1
    const u32 KU_MODE_COMPLETED_SINCE_START     = 324;      // 0x144 /  image, see below
    const u32 KU_TOTAL_TAKEDOWN_COUNT           = 392;      // 0x188
    const u32 KU_TOTAL_ONLINE_VERT_TAKEDOWNS    = 396;      // 0x18C
    const u32 KU_TAKEDOWN_TYPE_COUNTS           = 400;      // 0x190, memcpy 0x34
    const u32 KU_WINS_PER_MODE                  = 452;      // 0x1C4, 10 words
    const u32 KU_RANK_WINS_PER_MODE             = 492;      // 0x1EC, 10 words
    const u32 KU_LOSSES_PER_MODE                = 532;      // 0x214, 10 words
    const u32 KU_COMPLETED_BARREL_ROLLS         = 572;      // 0x23C
    const u32 KU_COMPLETED_AIR_SPIN_ANGLE       = 576;      // 0x240
    const u32 KU_COMPLETED_HANDBRAKE_TURN_ANGLE = 580;      // 0x244
    const u32 KU_COMPLETED_DRIFT_DISTANCE       = 584;      // 0x248
    const u32 KU_ONCOMING_DISTANCE              = 588;      // 0x24C
    const u32 KU_AIR_MAXIMUM                    = 592;      // 0x250
    const u32 KU_HIGHEST_SHOWTIME_SCORE         = 596;      // 0x254
    const u32 KU_BEST_STUNT_RUN_SCORE           = 600;      // 0x258
    const u32 KU_CAR_COUNT                      = 604;      // 0x25C  (SplitArray base-run counts)
    const u32 KU_LIVERY_COUNT                   = 608;      // 0x260
    const u32 KU_RIVAL_COUNT                    = 612;      // 0x264
    const u32 KU_EVENT_COUNT                    = 616;      // 0x268  == BrnGuiSaveLoad::Profile::muManifestCount
    const u32 KU_CARS                           = 624;      // 0x270  (512 * 0x18)
    const u32 KU_LIVERIES                       = 12912;    // 0x3270 (512 * 0x18)
    const u32 KU_RIVALS                         = 25200;    // 0x6270 (64 * 0x38)
    const u32 KU_EVENTS                         = 28784;    // 0x7070 == BrnGuiSaveLoad::Profile::maVersionManifest
    const u32 KU_STUNT_ELEMENT_SETS             = 30184;    // 0x75E8, memcpy 0x3018
    const u32 KU_MEDAL_COUNT_FROM_THE_START     = 42496;    // 0xA600
    // Renamed 2026-08-27 with the live members: image+42500 mirrors Profile+42516 (SILVER,
    // rank-gated) and image+42501 mirrors Profile+42517 (GOLD, 100%-gated). Offsets unchanged.
    const u32 KU_SILVER_CARS_UNLOCKED           = 42500;    // 0xA604 <- Profile+42516
    const u32 KU_GOLD_CARS_UNLOCKED             = 42501;    // 0xA605 <- Profile+42517
    const u32 KU_JUNK_YARDS_SET                 = 42504;    // NEVER WRITTEN -- see the note at the copy site
    const u32 KU_BODY_SHOPS_SET                 = 42552;    // 0xA638, memcpy 0x60
    const u32 KU_PAINT_SHOPS_SET                = 42648;    // 0xA698, six std
    const u32 KU_GAS_STATIONS_SET               = 42696;    // 0xA6C8, memcpy 0x78
    const u32 KU_CAR_PARKS_SET                  = 42816;    // 0xA740, memcpy 0x60
    const u32 KU_FREEBURN_CHALLENGE_DATA        = 42912;    // 0xA7A0, memcpy 0x3E88
    const u32 KU_HIT_PROP_BITS                  = 58920;    // 0xE628, memcpy 0x9280
    const u32 KU_STUNT_COUNTS_BY_COUNTY         = 96424;    // 0x178A8, memcpy 0x1E
    const u32 KU_NETWORK_CHALLENGE_DATA         = 96456;    // 0x178C8, memcpy 0xE00
    const u32 KU_CHALLENGE_DATA                 = 100040;   // 0x186C8, memcpy 0xA00
    const u32 KU_LAST_ROAD_RULES_RESET_TIME     = 102600;   // 0x190C8
    const u32 KU_LICENCE_PICTURE                = 102604;   // 0x190CC, seven words (see the PARK)
    const u32 KU_LICENCE_TEXTURE_DATA           = 102632;   // 0x190E8, memcpy 0x2580
    const u32 KU_LICENCE_PICTURE_IS_VALID       = 112232;   // 0x1B668 (lbz/stb)
    const u32 KU_MUGSHOT_GALLERIES              = 112240;   // 0x1B670, memcpy 0x1608 (5 * 1128)
    const u32 KU_MUGSHOT_GALLERY_STRIDE         = 1128;     // console sizeof(Array<MugshotInfo,20>)
    const u32 KU_MUGSHOT_FILE_ID_BITS           = 117880;   // 0x1CC78, five std
    const u32 KU_CAR_TYPES                      = 117920;   // 0x1CCA0, three words
    const u32 KU_CURRENT_CAR_TYPE               = 117932;   // 0x1CCAC
    const u32 KU_TRAINING_SEEN_BITS             = 117936;   // 0x1CCB0, four std
    const u32 KU_NUM_ONLINE_RACES_DONE          = 117968;   // 0x1CCD0
    const u32 KU_NUM_ONLINE_RACES_WON           = 117972;   // 0x1CCD4
    const u32 KU_NUM_MUGSHOTS_SENT              = 117976;   // 0x1CCD8
    const u32 KU_DATE_LICENCE_ISSUED            = 117980;   // 0x1CCDC, three words
    const u32 KU_DATE_100_PERCENT_COMPLETED     = 117992;   // 0x1CCE8, three words
    const u32 KU_HIGHEST_ROAD_RAGE_TAKEDOWNS    = 118004;   // 0x1CCF4
    const u32 KU_SEEN_TROPHY_BITS               = 118008;   // 0x1CCF8 (ldx/std)
    const u32 KU_SEEN_100_PERCENT_SEQUENCE      = 118016;   // 0x1CD00
    const u32 KU_IS_NEW_PROFILE                 = 118017;   // 0x1CD01
    const u32 KU_CREDITS_SEQUENCE_VIEWED        = 118018;   // 0x1CD02
    const u32 KU_ONE_HUNDRED_HUD_MSG_VIEWED     = 118019;   // 0x1CD03
    const u32 KU_HAS_UNLOCKED_CREDITS           = 118020;   // 0x1CD04
    const u32 KU_HAVE_SET_100_PERCENT_DATE      = 118021;   // 0x1CD05
    const u32 KU_SEEN_ELITE_SEQUENCE            = 118022;   // 0x1CD06
    const u32 KU_ROAD_RULES_ID_LOW_BITS         = 118028;   // 0x1CD0C
    const u32 KU_SEEN_ALL_EVENT_TYPE_BITS       = 118032;   // 0x1CD10 (ldx/std)
    const u32 KU_REAL_TIME_PLAYED               = 118040;   // 0x1CD18
    const u32 KU_REDUNDANT_FLOAT4               = 118044;   // 0x1CD1C
    const u32 KU_ROAD_RULES_ID_HIGH_BITS        = 118048;   // 0x1CD20 (last field; image ends at 118052)

    // The four game-mode tally arrays are [18] live but only [17] in the image.
    const s32 KI_SERIALISED_GAME_MODE_COUNT     = 17;
    const s32 KI_DLC_GAME_MODE_SLOT             = 17;   // the DLC island mode, carried by the DLC1 image
    const s32 KI_MUGSHOT_GALLERY_COUNT          = 5;

    // The live-side prop-hit bit-array capacity both bodies range-assert against
    // (X360 `ori r25, r11, 0x93E0` == 300000 == BitArray<300000u>::GetCapacity()).
    const u32 KU_HIT_PROP_BIT_COUNT             = 300000;
}

// ---- BrnGuiSaveLoad::ProfileDLC1 (9800 bytes) ----
namespace SaveImageDLC1
{
    const u32 KU_VERSION                        = 0;        // stw 6 (Serialise is its only writer)
    const u32 KU_TARGET_EVENT_SCORES            = 8;        // memcpy 0x7B0 (Array<TargetEventScore,49>)
    const u32 KU_EVENT_SCORES_TO_UPLOAD         = 1976;     // 0x7B8, memcpy 0x318
    const u32 KU_SPAWN_CAR_ID                   = 2768;     // 0xAD0 (the TRUE spawn id, DLC or not)
    const u32 KU_DLC_CAR_COUNT                  = 2776;     // 0xAD8
    const u32 KU_DLC_LIVERY_COUNT               = 2780;     // 0xADC
    const u32 KU_DLC_RIVAL_COUNT                = 2784;     // 0xAE0
    const u32 KU_DLC_EVENT_COUNT                = 2788;     // 0xAE4
    const u32 KU_DLC_CARS                       = 2792;     // 0xAE8
    const u32 KU_DLC_LIVERIES                   = 5192;     // 0x1448
    const u32 KU_DLC_RIVALS                     = 7592;     // 0x1DA8
    const u32 KU_DLC_EVENTS                     = 9384;     // 0x24A8
    const u32 KU_DEVELOPER_CHALLENGE_BITS       = 9768;     // 0x2628 (FastBitArray<120u>, 16 bytes)
    const u32 KU_MODE_AMOUNT_DLC_SLOT           = 9784;     // 0x2638 \ the four game-mode tallies'
    const u32 KU_MODE_DISCOVERED_DLC_SLOT       = 9788;     // 0x263C  \ 18th (DLC island) slot --
    const u32 KU_MODE_COMPLETED_DLC_SLOT        = 9792;     // 0x2640  / exactly the four fields
    const u32 KU_MODE_COMPLETED_START_DLC_SLOT  = 9796;     // 0x2644 /  ProfileDLC1::Construct clears

    // The DLC-run capacities SplitArray is handed (X360 `li r9, 0x64` x3, `li r9, 0x30`).
    const s32 KI_MAX_DLC_CARS                   = 100;
    const s32 KI_MAX_DLC_LIVERIES               = 100;
    const s32 KI_MAX_DLC_RIVALS                 = 100;
    const s32 KI_MAX_DLC_EVENTS                 = 48;

    // The FastBitArray the DLC1 image carries the developer-challenge bits in. Its own
    // asserts name its capacity ("... is out of range (max bits: 120)",
    // CgsFastBitArray.h:431), and 120 bits == the 16 bytes between +0x2628 and +0x2638.
    const u32 KU_DEVELOPER_CHALLENGE_IMAGE_BITS = 120;
}

typedef CgsContainers::FastBitArray<SaveImageDLC1::KU_DEVELOPER_CHALLENGE_IMAGE_BITS> DeveloperChallengeImageBits;

// -------------------------------------------------------------------------------------
// Named-offset accessors over the two serialised byte images.
//
// The images are external, fixed-layout byte streams (see the banner), so field access is
// by console offset -- but never as a bare pointer-arithmetic cast at the use site. Every
// read/write goes through one of these four helpers, which take the offset as a NAMED
// constant from the tables above and use memcpy for the multi-byte fields (aliasing-safe,
// and it is literally the instruction the console emits for the block fields).
// -------------------------------------------------------------------------------------
template <typename T>
inline void StoreImageField(u8* lpaImage, u32 luOffset, const T& lrValue)
{
    memcpy(lpaImage + luOffset, &lrValue, sizeof(T));
}

template <typename T>
inline void LoadImageField(const u8* lpaImage, u32 luOffset, T& lrValue)
{
    memcpy(&lrValue, lpaImage + luOffset, sizeof(T));
}

inline void StoreImageBlock(u8* lpaImage, u32 luOffset, const void* lpSource, u32 luBytes)
{
    memcpy(lpaImage + luOffset, lpSource, luBytes);
}

inline void LoadImageBlock(const u8* lpaImage, u32 luOffset, void* lpDestination, u32 luBytes)
{
    memcpy(lpDestination, lpaImage + luOffset, luBytes);
}

// A typed view onto a serialised sub-object (a container the console stores whole, or one
// of SplitArray's out-params). Offsets are named constants, never literals at the use site.
template <typename T>
inline T* ImageObjectAt(u8* lpaImage, u32 luOffset)
{
    return reinterpret_cast<T*>(lpaImage + luOffset);
}

template <typename T>
inline const T* ImageObjectAt(const u8* lpaImage, u32 luOffset)
{
    return reinterpret_cast<const T*>(lpaImage + luOffset);
}

}  // anonymous namespace

// =====================================================================================
// BrnProgression::Profile::Serialise  @0x8237C1F0
//
// Clear both save images, stamp their version words, then copy the whole live profile
// into them. Records that are DLC-owned are fanned out into the DLC1 image by SplitArray
// so that a base-game build (which never sees the DLC1 segment) loads a save containing
// only content it owns.
//
// lpUpgrade is the optional PROFILEUPG resource (ProfileManager::ReadProfileData passes
// it only when the DLC entitlement gate passes; NULL otherwise, which is always the case
// on the PC boot path today). It remaps prop-hit bit indices between the build that wrote
// the save and this one -- see the loop at the end.
// =====================================================================================
void Profile::Serialise(BrnGuiSaveLoad::Profile* lpImage,
                        const ProfileUpgradeTable* lpUpgrade,
                        BrnGuiSaveLoad::ProfileDLC1* lpImageDLC1)
{
    u8* lpaImage    = reinterpret_cast<u8*>(lpImage);
    u8* lpaImageDLC = reinterpret_cast<u8*>(lpImageDLC1);

    // ---- prologue: both images cleared, the progression version word stamped ---------
    // (These two memsets and the two version-word stores -- this one and the DLC1 one at
    // the very end -- are the ONLY four stores in the function that do not read the live
    // profile. They are what makes a first-ever boot -- ProfileManager::Bootup runs
    // ReadProfileData BEFORE the storage boot-up -- still present a version-current image
    // to ProfileManager::ValidateProfiles when there is no save on the storage device.
    // BrnGuiSaveLoad::Profile::ConstructImage / ProfileDLC1::ConstructImage were outlined
    // from exactly these four stores while this body was parked; they are now dead code on
    // this path and BrnGuiProfile.cpp's shim no longer calls them.)
    memset(lpaImage,    0, BrnGuiSaveLoad::Profile::KI_IMAGE_SIZE_BYTES);       // memset 0x1CD30
    memset(lpaImageDLC, 0, BrnGuiSaveLoad::ProfileDLC1::KI_IMAGE_SIZE_BYTES);   // memset 0x2648

    const s32 liImageVersion = BrnGuiSaveLoad::Profile::KI_VERSION_CURRENT;     // 28
    StoreImageField(lpaImage, SaveImage::KU_VERSION, liImageVersion);

    // ---- identity / last-known car placement ----------------------------------------
    CGS_STATIC_ASSERT((sizeof(macName) == 32), "X360 memcpy(image+4, name, 0x20)");
    StoreImageBlock(lpaImage, SaveImage::KU_NAME, macName, sizeof(macName));

    CGS_STATIC_ASSERT((sizeof(Vector3) == 16), "X360 stores each Vector3 as two std");
    StoreImageField(lpaImage, SaveImage::KU_CAR_POSITION,  mCarPosition);
    StoreImageField(lpaImage, SaveImage::KU_CAR_DIRECTION, mCarDirection);

    // The DLC1 image always records the TRUE spawn car, DLC or not (the X360 does this
    // store unconditionally, before the substitution branch below).
    StoreImageField(lpaImageDLC, SaveImageDLC1::KU_SPAWN_CAR_ID, mSpawnCarId);

    // A DLC car must not be written into the BASE image as the spawn car: a build without
    // the DLC would try to spawn content it does not have. Substitute the first non-DLC
    // car the player owns (and clear the wheel id with it); if the player owns no non-DLC
    // car at all the X360 leaves BOTH image fields at their memset zero.
    //
    // FLAG (signature, not behaviour): the X360 hands IsDLCCarId the ADDRESS of a bare
    // CgsID -- `mr r3, r28` with r28 == &mSpawnCarId, and a stack copy of maCars[i]'s id
    // in the loop -- so the original declaration is IsDLCCarId(const CgsID&), not the
    // committed IsDLCCarId(const CarData&). BrnGuiSaveLoad is another agent's file, so the
    // call is spelled against the committed signature exactly as this file's four
    // SplitArray specialisations already do (the id is at record +0, so it is the same
    // load either way).
    if (!BrnGuiSaveLoad::ProfileDLC1::IsDLCCarId(reinterpret_cast<const CarData&>(mSpawnCarId)))
    {
        StoreImageField(lpaImage, SaveImage::KU_SPAWN_CAR_ID,   mSpawnCarId);
        StoreImageField(lpaImage, SaveImage::KU_SPAWN_WHEEL_ID, mSpawnWheelId);
    }
    else
    {
        for (s32 liCar = 0; liCar < miCarCount; ++liCar)
        {
            const CgsID lCarId = maCars[liCar].GetId();
            if (!BrnGuiSaveLoad::ProfileDLC1::IsDLCCarId(reinterpret_cast<const CarData&>(lCarId)))
            {
                const CgsID lNoWheelId = 0;
                StoreImageField(lpaImage, SaveImage::KU_SPAWN_CAR_ID,   maCars[liCar].GetId());
                StoreImageField(lpaImage, SaveImage::KU_SPAWN_WHEEL_ID, lNoWheelId);
                break;
            }
        }
    }

    // ---- scalar statistics -----------------------------------------------------------
    StoreImageField(lpaImage, SaveImage::KU_ROAD_RULES_DOWNLOAD_TIMESTAMP, muTimeStampOfLastRoadRulesDownload);
    StoreImageField(lpaImage, SaveImage::KU_DISTANCE_DRIVEN_ONLINE,        mfDistanceDrivenOnline);
    StoreImageField(lpaImage, SaveImage::KU_DISTANCE_DRIVEN_OFFLINE,       mfDistanceDrivenOffline);
    StoreImageField(lpaImage, SaveImage::KU_IN_CAR_TIME_PLAYED,            mfInCarTimePlayed);
    StoreImageField(lpaImage, SaveImage::KU_CURRENT_PROGRESSION_RANK,      mi8CurrentProgressionRank);
    StoreImageField(lpaImage, SaveImage::KU_POWER_PARKING_BEST,            mi8PowerParkingBestRating);
    StoreImageField(lpaImage, SaveImage::KU_POWER_PARKING_BETWEEN,         mi8PowerParkingBetweenOtherPlayersBestRating);
    StoreImageField(lpaImage, SaveImage::KU_BEST_NEW_BURNOUT_CHAIN,        muBestNewBurnoutChainScore);

    // The four game-mode tally arrays: [0..16] into the base image, [17] (the DLC island
    // mode) into the DLC1 image. The X360 fuses all four into one 17-trip loop.
    for (s32 liMode = 0; liMode < SaveImage::KI_SERIALISED_GAME_MODE_COUNT; ++liMode)
    {
        const u32 luWord = static_cast<u32>(liMode) * sizeof(s32);
        StoreImageField(lpaImage, SaveImage::KU_MODE_AMOUNT                + luWord, maGameModeTypeAmount[liMode]);
        StoreImageField(lpaImage, SaveImage::KU_MODE_DISCOVERED            + luWord, maGameModeTypeAmountDiscovered[liMode]);
        StoreImageField(lpaImage, SaveImage::KU_MODE_COMPLETED             + luWord, maGameModeTypeAmountCompleted[liMode]);
        StoreImageField(lpaImage, SaveImage::KU_MODE_COMPLETED_SINCE_START + luWord, maGameModeTypeAmountCompletedSinceTheStart[liMode]);
    }
    StoreImageField(lpaImageDLC, SaveImageDLC1::KU_MODE_AMOUNT_DLC_SLOT,          maGameModeTypeAmount[SaveImage::KI_DLC_GAME_MODE_SLOT]);
    StoreImageField(lpaImageDLC, SaveImageDLC1::KU_MODE_DISCOVERED_DLC_SLOT,      maGameModeTypeAmountDiscovered[SaveImage::KI_DLC_GAME_MODE_SLOT]);
    StoreImageField(lpaImageDLC, SaveImageDLC1::KU_MODE_COMPLETED_DLC_SLOT,       maGameModeTypeAmountCompleted[SaveImage::KI_DLC_GAME_MODE_SLOT]);
    StoreImageField(lpaImageDLC, SaveImageDLC1::KU_MODE_COMPLETED_START_DLC_SLOT, maGameModeTypeAmountCompletedSinceTheStart[SaveImage::KI_DLC_GAME_MODE_SLOT]);

    StoreImageField(lpaImage, SaveImage::KU_TOTAL_TAKEDOWN_COUNT,        miTotalTakedownCount);
    StoreImageField(lpaImage, SaveImage::KU_TOTAL_ONLINE_VERT_TAKEDOWNS, miTotalOnlineVerticleTakedownCount);

    CGS_STATIC_ASSERT((sizeof(maiTakedownTypeCounts) == 52), "X360 memcpy(image+0x190, +0x1A0, 0x34)");
    StoreImageBlock(lpaImage, SaveImage::KU_TAKEDOWN_TYPE_COUNTS, maiTakedownTypeCounts, sizeof(maiTakedownTypeCounts));

    CGS_STATIC_ASSERT((sizeof(maiWinsPerOfflineGameMode) == 40), "X360 10-word copy loop");
    CGS_STATIC_ASSERT((sizeof(maiRankWinsPerOfflineGameMode) == 40), "X360 10-word copy loop");
    CGS_STATIC_ASSERT((sizeof(maiLossesPerOfflineGameMode) == 40), "X360 10-word copy loop");
    StoreImageBlock(lpaImage, SaveImage::KU_WINS_PER_MODE,      maiWinsPerOfflineGameMode,     sizeof(maiWinsPerOfflineGameMode));
    StoreImageBlock(lpaImage, SaveImage::KU_RANK_WINS_PER_MODE, maiRankWinsPerOfflineGameMode, sizeof(maiRankWinsPerOfflineGameMode));
    StoreImageBlock(lpaImage, SaveImage::KU_LOSSES_PER_MODE,    maiLossesPerOfflineGameMode,   sizeof(maiLossesPerOfflineGameMode));

    StoreImageField(lpaImage, SaveImage::KU_COMPLETED_BARREL_ROLLS,         miCompletedBarrelRolls);
    StoreImageField(lpaImage, SaveImage::KU_COMPLETED_AIR_SPIN_ANGLE,       mfCompletedAirSpinAngle);
    StoreImageField(lpaImage, SaveImage::KU_COMPLETED_HANDBRAKE_TURN_ANGLE, mfCompletedHandbreakTurnAngle);
    StoreImageField(lpaImage, SaveImage::KU_COMPLETED_DRIFT_DISTANCE,       mfCompletedDriftDistance);
    StoreImageField(lpaImage, SaveImage::KU_ONCOMING_DISTANCE,              mfOncomingDistance);
    StoreImageField(lpaImage, SaveImage::KU_AIR_MAXIMUM,                    mfAirMaximum);
    StoreImageField(lpaImage, SaveImage::KU_HIGHEST_SHOWTIME_SCORE,         miHighestShowTimeScore);
    StoreImageField(lpaImage, SaveImage::KU_BEST_STUNT_RUN_SCORE,           miBestStuntRunScore);

    // ---- the SplitArray fan-out ------------------------------------------------------
    // Each of the four record arrays is walked once and split, in order, into a BASE run
    // (base image) and a DLC run (DLC1 image), each with its own count word. The base runs
    // land at their console offsets so a DLC-less build reads exactly the records it owns;
    // the ProfileEvent base run is what ValidateProfile reads as the stored "manifest"
    // (count @+0x268, entries @+0x7070).
    //
    // FLAG (console defect, reproduced): the DLC RIVAL run is handed a cap of 100 but the
    // DLC1 image only has room for 32 (+0x1DA8..+0x24A8 == 1792 bytes / 0x38). Cars
    // (100*0x18 == 0xAE8..0x1448), liveries (100*0x18 == 0x1448..0x1DA8) and events
    // (48*8 == 0x24A8..0x2628) all fit their runs exactly, so the 100 on the rival call is
    // a copy-paste of the car/livery cap. Kept verbatim -- the cap is the console's.
    SplitArray<CarData, BrnGuiSaveLoad::CarData>(
        miCarCount, maCars,
        ImageObjectAt<s32>(lpaImage, SaveImage::KU_CAR_COUNT),
        ImageObjectAt<BrnGuiSaveLoad::CarData>(lpaImage, SaveImage::KU_CARS),
        ImageObjectAt<s32>(lpaImageDLC, SaveImageDLC1::KU_DLC_CAR_COUNT),
        ImageObjectAt<BrnGuiSaveLoad::CarData>(lpaImageDLC, SaveImageDLC1::KU_DLC_CARS),
        SaveImageDLC1::KI_MAX_DLC_CARS);

    SplitArray<LiveryData, BrnGuiSaveLoad::LiveryData>(
        miLiveryDataCount, maLiveryChoices,
        ImageObjectAt<s32>(lpaImage, SaveImage::KU_LIVERY_COUNT),
        ImageObjectAt<BrnGuiSaveLoad::LiveryData>(lpaImage, SaveImage::KU_LIVERIES),
        ImageObjectAt<s32>(lpaImageDLC, SaveImageDLC1::KU_DLC_LIVERY_COUNT),
        ImageObjectAt<BrnGuiSaveLoad::LiveryData>(lpaImageDLC, SaveImageDLC1::KU_DLC_LIVERIES),
        SaveImageDLC1::KI_MAX_DLC_LIVERIES);

    SplitArray<RivalData, BrnGuiSaveLoad::RivalData>(
        miRivalCount, maRivals,
        ImageObjectAt<s32>(lpaImage, SaveImage::KU_RIVAL_COUNT),
        ImageObjectAt<BrnGuiSaveLoad::RivalData>(lpaImage, SaveImage::KU_RIVALS),
        ImageObjectAt<s32>(lpaImageDLC, SaveImageDLC1::KU_DLC_RIVAL_COUNT),
        ImageObjectAt<BrnGuiSaveLoad::RivalData>(lpaImageDLC, SaveImageDLC1::KU_DLC_RIVALS),
        SaveImageDLC1::KI_MAX_DLC_RIVALS);

    SplitArray<ProfileEvent, BrnGuiSaveLoad::ProfileEvent>(
        miEventCount, maEvents,
        ImageObjectAt<s32>(lpaImage, SaveImage::KU_EVENT_COUNT),
        ImageObjectAt<BrnGuiSaveLoad::ProfileEvent>(lpaImage, SaveImage::KU_EVENTS),
        ImageObjectAt<s32>(lpaImageDLC, SaveImageDLC1::KU_DLC_EVENT_COUNT),
        ImageObjectAt<BrnGuiSaveLoad::ProfileEvent>(lpaImageDLC, SaveImageDLC1::KU_DLC_EVENTS),
        SaveImageDLC1::KI_MAX_DLC_EVENTS);

    // ---- discovery sets / world progress ---------------------------------------------
    CGS_STATIC_ASSERT((sizeof(maStuntElements) == 12312), "X360 memcpy(image+0x75E8, +0x75F8, 0x3018)");
    StoreImageBlock(lpaImage, SaveImage::KU_STUNT_ELEMENT_SETS, maStuntElements, sizeof(maStuntElements));

    StoreImageField(lpaImage, SaveImage::KU_MEDAL_COUNT_FROM_THE_START, muMedalCountFromTheStart);
    StoreImageField(lpaImage, SaveImage::KU_SILVER_CARS_UNLOCKED,       mbSilverCarsUnlocked);
    StoreImageField(lpaImage, SaveImage::KU_GOLD_CARS_UNLOCKED,         mbGoldCarsUnlocked);

    // FLAG (console defect, reproduced): the JUNK YARD drive-thru set is never serialised.
    // The X360 emits the body-shop copy TWICE, back to back with identical operands
    // (0x8237C570 and 0x8237C580, both memcpy(image+0xA638, live+0xA648, 0x60)), where the
    // first of the pair should have been memcpy(image+0xA608, live+0xA618, 0x30) -- the
    // junk-yard set. Deserialise mirrors the same duplicated pair (0x8237D604/0x8237D614),
    // so image+42504 is written by nobody and mJunkYardsDriveThruSet is lost across a
    // save/load. Reproduced exactly: this is observable behaviour ("visited junkyards"
    // discovery does not persist), not a transcription slip.
    CGS_STATIC_ASSERT((sizeof(mBodyShopsDriveThruSet)   == 96),  "X360 memcpy 0x60");
    CGS_STATIC_ASSERT((sizeof(mPaintShopsDriveThruSet)  == 48),  "X360 six-qword copy loop");
    CGS_STATIC_ASSERT((sizeof(mGasStationsDriveThruSet) == 120), "X360 memcpy 0x78");
    CGS_STATIC_ASSERT((sizeof(mCarParksDriveThruSet)    == 96),  "X360 memcpy 0x60");
    StoreImageBlock(lpaImage, SaveImage::KU_BODY_SHOPS_SET, &mBodyShopsDriveThruSet, sizeof(mBodyShopsDriveThruSet));
    StoreImageBlock(lpaImage, SaveImage::KU_BODY_SHOPS_SET, &mBodyShopsDriveThruSet, sizeof(mBodyShopsDriveThruSet));
    StoreImageBlock(lpaImage, SaveImage::KU_PAINT_SHOPS_SET,  &mPaintShopsDriveThruSet,  sizeof(mPaintShopsDriveThruSet));
    StoreImageBlock(lpaImage, SaveImage::KU_GAS_STATIONS_SET, &mGasStationsDriveThruSet, sizeof(mGasStationsDriveThruSet));
    StoreImageBlock(lpaImage, SaveImage::KU_CAR_PARKS_SET,    &mCarParksDriveThruSet,    sizeof(mCarParksDriveThruSet));

    CGS_STATIC_ASSERT((sizeof(maFreeBurnChallengeData) == 16008), "X360 memcpy 0x3E88");
    CGS_STATIC_ASSERT((sizeof(mabHitPropBitArray)      == 37504), "X360 memcpy 0x9280");
    CGS_STATIC_ASSERT((sizeof(maaiStuntCountsByCounty) == 30),    "X360 memcpy 0x1E");
    CGS_STATIC_ASSERT((sizeof(maNetworkChallengeData)  == 3584),  "X360 memcpy 0xE00");
    CGS_STATIC_ASSERT((sizeof(maChallengeData)         == 2560),  "X360 memcpy 0xA00");
    StoreImageBlock(lpaImage, SaveImage::KU_FREEBURN_CHALLENGE_DATA, &maFreeBurnChallengeData, sizeof(maFreeBurnChallengeData));
    StoreImageBlock(lpaImage, SaveImage::KU_HIT_PROP_BITS,           &mabHitPropBitArray,      sizeof(mabHitPropBitArray));
    StoreImageBlock(lpaImage, SaveImage::KU_STUNT_COUNTS_BY_COUNTY,  maaiStuntCountsByCounty,  sizeof(maaiStuntCountsByCounty));
    StoreImageBlock(lpaImage, SaveImage::KU_NETWORK_CHALLENGE_DATA,  maNetworkChallengeData,   sizeof(maNetworkChallengeData));
    StoreImageBlock(lpaImage, SaveImage::KU_CHALLENGE_DATA,          maChallengeData,          sizeof(maChallengeData));
    StoreImageField(lpaImage, SaveImage::KU_LAST_ROAD_RULES_RESET_TIME, muLastRoadRulesResetTime);

    // ---- licence picture --------------------------------------------------------------
    // PARK (x64 width, documented): the console copies the live CgsNetwork::NetworkTexture
    // whole into a 28-byte image slot (X360 seven-word loop at image+0x190CC). That record
    // is TWO raw console pointers plus five scalars; on x64 the live object is 40 bytes, so
    // copying it verbatim would run 12 bytes into the 9600-byte pixel block that follows at
    // +0x190E8 and corrupt the picture. The slot is therefore left at its memset zero and
    // Deserialise rebuilds the descriptor from the persisted pixels instead (see there).
    // Nothing is lost: the console's own stored pointers were only ever self-consistent
    // because the Profile object address does not move, and Profile::SetPlayerLicencePicture
    // @0x8235A020 -- the sole writer -- always Prepares the texture over
    // macPlayerLicenceTextureData with the fixed KI_PLAYERLICENCEPICTURE_* constants, so the
    // descriptor carries no information the pixels + the valid flag do not.
    CGS_STATIC_ASSERT((sizeof(macPlayerLicenceTextureData) == 9600), "X360 memcpy 0x2580");
    StoreImageBlock(lpaImage, SaveImage::KU_LICENCE_TEXTURE_DATA, macPlayerLicenceTextureData, sizeof(macPlayerLicenceTextureData));
    StoreImageField(lpaImage, SaveImage::KU_LICENCE_PICTURE_IS_VALID, mbPlayerLicencePictureIsValid);

    // ---- mugshot galleries -------------------------------------------------------------
    // The console copies all five galleries in one 5640-byte memcpy (5 * 1128). The copy is done
    // per gallery at the console's 1128-byte image stride, which keeps every gallery's count word
    // at image + gallery*1128 + 1120, exactly where the console put it (and where
    // BrnGuiProfile.cpp's stored-image constants expect it).
    // [progression wave 2026-09-06, lane profile] sizeof(Array<MugshotInfo,20>) is now 1128 on
    // x64 too -- MugshotInfo stopped being an align-1 opaque pad when its real members landed
    // (the 24-byte UniquePlayerID gives it the console's 8-byte alignment), so the live gallery
    // and the image slot are the same size again. The <= assert below still holds either way.
    CGS_STATIC_ASSERT((sizeof(maaMugshotInfo[0]) <= SaveImage::KU_MUGSHOT_GALLERY_STRIDE),
                  "a live gallery must fit the console's 1128-byte image slot");
    for (s32 liGallery = 0; liGallery < SaveImage::KI_MUGSHOT_GALLERY_COUNT; ++liGallery)
    {
        StoreImageBlock(lpaImage,
                        SaveImage::KU_MUGSHOT_GALLERIES
                            + static_cast<u32>(liGallery) * SaveImage::KU_MUGSHOT_GALLERY_STRIDE,
                        &maaMugshotInfo[liGallery], sizeof(maaMugshotInfo[liGallery]));
    }

    CGS_STATIC_ASSERT((sizeof(maAvailableMugshotFileIDs) == 40), "X360 five-qword copy loop");
    CGS_STATIC_ASSERT((sizeof(mafCarTypes)               == 12), "X360 three-word copy");
    CGS_STATIC_ASSERT((sizeof(maHasPlayerSeenTraining)   == 32), "X360 four-qword copy");
    StoreImageBlock(lpaImage, SaveImage::KU_MUGSHOT_FILE_ID_BITS, maAvailableMugshotFileIDs, sizeof(maAvailableMugshotFileIDs));
    StoreImageBlock(lpaImage, SaveImage::KU_CAR_TYPES,            mafCarTypes,               sizeof(mafCarTypes));
    StoreImageField(lpaImage, SaveImage::KU_CURRENT_CAR_TYPE,     meCurrentCarType);
    StoreImageBlock(lpaImage, SaveImage::KU_TRAINING_SEEN_BITS,   &maHasPlayerSeenTraining,  sizeof(maHasPlayerSeenTraining));

    // ---- online tallies, dates, completion flags ---------------------------------------
    CGS_STATIC_ASSERT((sizeof(CgsSystem::DateAndTime) == 12), "X360 three-word date copy");
    StoreImageField(lpaImage, SaveImage::KU_NUM_ONLINE_RACES_DONE,       miNumOnlineRacesDone);
    StoreImageField(lpaImage, SaveImage::KU_NUM_ONLINE_RACES_WON,        miNumOnlineRacesWon);
    StoreImageField(lpaImage, SaveImage::KU_NUM_MUGSHOTS_SENT,           miNumMugshotsSent);
    StoreImageField(lpaImage, SaveImage::KU_DATE_LICENCE_ISSUED,         mDateLicenceIssued);
    StoreImageField(lpaImage, SaveImage::KU_DATE_100_PERCENT_COMPLETED,  mDate100PercentCompleted);
    StoreImageField(lpaImage, SaveImage::KU_HIGHEST_ROAD_RAGE_TAKEDOWNS, miHighestNumberOfTakeDownsInRoadRage);
    StoreImageField(lpaImage, SaveImage::KU_SEEN_TROPHY_BITS,            mSeenTrophyAwardBitArray);

    // The completion-flag cluster. NOTE the two gaps the X360 leaves: mbRedundantBool4
    // (+118039) and the miPad1/miPad2 bytes are never copied, so image+118023..118027 stay
    // zero. (This is the block Profile::_AssertLayout pins on the live side.)
    StoreImageField(lpaImage, SaveImage::KU_SEEN_100_PERCENT_SEQUENCE,  mb100PercentCompletionSequenceShown);
    StoreImageField(lpaImage, SaveImage::KU_IS_NEW_PROFILE,             mbIsNewProfile);
    StoreImageField(lpaImage, SaveImage::KU_CREDITS_SEQUENCE_VIEWED,    mbCreditsSequenceViewed);
    StoreImageField(lpaImage, SaveImage::KU_ONE_HUNDRED_HUD_MSG_VIEWED, mbOneHundredHudMessageViewed);
    StoreImageField(lpaImage, SaveImage::KU_HAS_UNLOCKED_CREDITS,       mbHasUnlockedCredits);
    StoreImageField(lpaImage, SaveImage::KU_HAVE_SET_100_PERCENT_DATE,  mbHaveSet100PercentCompletedDate);
    StoreImageField(lpaImage, SaveImage::KU_SEEN_ELITE_SEQUENCE,        mbHaveSeenEliteCompletionSequence);

    StoreImageField(lpaImage, SaveImage::KU_ROAD_RULES_ID_LOW_BITS,   muRoadRulesIDLowBits);
    StoreImageField(lpaImage, SaveImage::KU_SEEN_ALL_EVENT_TYPE_BITS, mSeenCompleteAllEventTypeArray);
    StoreImageField(lpaImage, SaveImage::KU_REAL_TIME_PLAYED,         mfRealTimePlayed);
    StoreImageField(lpaImage, SaveImage::KU_REDUNDANT_FLOAT4,         mfRedundantFloat4);
    StoreImageField(lpaImage, SaveImage::KU_ROAD_RULES_ID_HIGH_BITS,  muRoadRulesIDHighBits);

    // ---- optional PROFILEUPG prop-hit re-index ------------------------------------------
    // A content patch can renumber the world's props. The PROFILEUPG resource is the
    // translation table: for each entry, the bit the LIVE profile holds at
    // muLiveBitIndex is republished into the IMAGE at muImageBitIndex (the image's array was
    // already copied verbatim above; this rewrites the moved entries on top).
    // Deserialise runs the same table the other way round.
    if (lpUpgrade != 0)
    {
        CgsContainers::BitArray<SaveImage::KU_HIT_PROP_BIT_COUNT>* lpImageHitProps =
            ImageObjectAt<CgsContainers::BitArray<SaveImage::KU_HIT_PROP_BIT_COUNT> >(
                lpaImage, SaveImage::KU_HIT_PROP_BITS);

        for (s32 liEntry = 0; liEntry < lpUpgrade->miCount; ++liEntry)
        {
            const ProfileUpgradeTable::Entry& lrEntry = lpUpgrade->mpaEntries[liEntry];

            CGS_ASSERT(lrEntry.muLiveBitIndex < SaveImage::KU_HIT_PROP_BIT_COUNT,
                       "luIndex < NUMBITS");                                  // CgsBitArray.h:203
            CGS_ASSERT(lrEntry.muImageBitIndex < SaveImage::KU_HIT_PROP_BIT_COUNT,
                       "luIndex < NUMBITS");                                  // CgsBitArray.h:222 / :241

            if (mabHitPropBitArray.IsBitSet(lrEntry.muLiveBitIndex))
            {
                lpImageHitProps->SetBit(lrEntry.muImageBitIndex);
            }
            else
            {
                lpImageHitProps->UnSetBit(lrEntry.muImageBitIndex);
            }
        }
    }

    // ---- the DLC1 image tail ------------------------------------------------------------
    const s32 liImageVersionDLC1 = BrnGuiSaveLoad::ProfileDLC1::KI_VERSION_CURRENT;   // 6
    StoreImageField(lpaImageDLC, SaveImageDLC1::KU_VERSION, liImageVersionDLC1);

    CGS_STATIC_ASSERT((sizeof(maTargetEventScores)   == 1968), "X360 memcpy(dlc1+8, +0x1CD38, 0x7B0)");
    CGS_STATIC_ASSERT((sizeof(maEventScoresToUpload) == 792),  "X360 memcpy(dlc1+0x7B8, +0x1D4E8, 0x318)");
    StoreImageBlock(lpaImageDLC, SaveImageDLC1::KU_TARGET_EVENT_SCORES,    &maTargetEventScores,   sizeof(maTargetEventScores));
    StoreImageBlock(lpaImageDLC, SaveImageDLC1::KU_EVENT_SCORES_TO_UPLOAD, &maEventScoresToUpload, sizeof(maEventScoresToUpload));

    // The developer-challenge bits are TRANSCRIBED, not copied: the live set is a
    // FastBitArray<15>, the image's is a FastBitArray<120>. The X360 clears both image
    // fields first (redundant after the memset, but it is the container's Clear) and then
    // walks the live set's iterator, setting each index it reports.
    CGS_STATIC_ASSERT((sizeof(DeveloperChallengeImageBits) == 16), "the DLC1 slot is 0x2628..0x2638");
    DeveloperChallengeImageBits* lpImageChallengeBits =
        ImageObjectAt<DeveloperChallengeImageBits>(lpaImageDLC, SaveImageDLC1::KU_DEVELOPER_CHALLENGE_BITS);
    lpImageChallengeBits->UnSetAll();

    for (CgsContainers::FastBitArray<15u>::Iterator lIt = mDeveloperChallengesCompleted.Begin();
         lIt != mDeveloperChallengesCompleted.End();
         ++lIt)
    {
        lpImageChallengeBits->SetBit(static_cast<u32>(lIt.GetIndex()));
    }
}

// =====================================================================================
// BrnProgression::Profile::Deserialise  @0x8237D308
//
// The exact inverse of Serialise: version-assert the image, wipe the live profile, then
// copy every field back. Afterwards, if the DLC1 image is version-current, its DLC runs
// are APPENDED onto the live arrays (that is the SplitArray inverse -- the base run is
// restored by the bulk copies, the DLC run is merged on top).
// =====================================================================================
void Profile::Deserialise(const BrnGuiSaveLoad::Profile* lpImage,
                          const ProfileUpgradeTable* lpUpgrade,
                          const BrnGuiSaveLoad::ProfileDLC1* lpImageDLC1)
{
    const u8* lpaImage    = reinterpret_cast<const u8*>(lpImage);
    const u8* lpaImageDLC = reinterpret_cast<const u8*>(lpImageDLC1);

    // FLAG (assert text): the X360 rodata string is truncated in the export at
    // "lpProfile->miVersionNumber == BrnGuiSav"... -- the tail is reconstructed from the
    // committed constant's name. The file/line (BrnProfile.cpp:2044) and the compared
    // value (cmpwi 0x1C) are exact. NOTE the parameter name: the console calls the SAVE
    // IMAGE `lpProfile` here, and the version it checks is the image's, not the live one's.
    s32 liImageVersion = 0;
    LoadImageField(lpaImage, SaveImage::KU_VERSION, liImageVersion);
    CGS_ASSERT(liImageVersion == BrnGuiSaveLoad::Profile::KI_VERSION_CURRENT,
               "lpProfile->miVersionNumber == BrnGuiSaveLoad::Profile::KI_VERSION_CURRENT");

    memset(this, 0, sizeof(*this));            // X360 memset(this, 0, 0x1D810)
    miVersionNumber = KI_VERSION_NUMBER;       // ...then re-stamp 28 (NOT read from the image)

    LoadImageBlock(lpaImage, SaveImage::KU_NAME, macName, sizeof(macName));
    LoadImageField(lpaImage, SaveImage::KU_CAR_POSITION,  mCarPosition);
    LoadImageField(lpaImage, SaveImage::KU_CAR_DIRECTION, mCarDirection);
    LoadImageField(lpaImage, SaveImage::KU_SPAWN_CAR_ID,   mSpawnCarId);
    LoadImageField(lpaImage, SaveImage::KU_SPAWN_WHEEL_ID, mSpawnWheelId);

    LoadImageField(lpaImage, SaveImage::KU_ROAD_RULES_DOWNLOAD_TIMESTAMP, muTimeStampOfLastRoadRulesDownload);
    LoadImageField(lpaImage, SaveImage::KU_DISTANCE_DRIVEN_ONLINE,        mfDistanceDrivenOnline);
    LoadImageField(lpaImage, SaveImage::KU_DISTANCE_DRIVEN_OFFLINE,       mfDistanceDrivenOffline);
    LoadImageField(lpaImage, SaveImage::KU_IN_CAR_TIME_PLAYED,            mfInCarTimePlayed);
    LoadImageField(lpaImage, SaveImage::KU_CURRENT_PROGRESSION_RANK,      mi8CurrentProgressionRank);
    LoadImageField(lpaImage, SaveImage::KU_POWER_PARKING_BEST,            mi8PowerParkingBestRating);
    LoadImageField(lpaImage, SaveImage::KU_POWER_PARKING_BETWEEN,         mi8PowerParkingBetweenOtherPlayersBestRating);
    LoadImageField(lpaImage, SaveImage::KU_BEST_NEW_BURNOUT_CHAIN,        muBestNewBurnoutChainScore);

    for (s32 liMode = 0; liMode < SaveImage::KI_SERIALISED_GAME_MODE_COUNT; ++liMode)
    {
        const u32 luWord = static_cast<u32>(liMode) * sizeof(s32);
        LoadImageField(lpaImage, SaveImage::KU_MODE_AMOUNT                + luWord, maGameModeTypeAmount[liMode]);
        LoadImageField(lpaImage, SaveImage::KU_MODE_DISCOVERED            + luWord, maGameModeTypeAmountDiscovered[liMode]);
        LoadImageField(lpaImage, SaveImage::KU_MODE_COMPLETED             + luWord, maGameModeTypeAmountCompleted[liMode]);
        LoadImageField(lpaImage, SaveImage::KU_MODE_COMPLETED_SINCE_START + luWord, maGameModeTypeAmountCompletedSinceTheStart[liMode]);
    }
    // The 18th (DLC island) slot always comes from the DLC1 image -- unconditionally, ahead
    // of the version gate further down (X360 0x8237D444..0x8237D46C).
    LoadImageField(lpaImageDLC, SaveImageDLC1::KU_MODE_AMOUNT_DLC_SLOT,          maGameModeTypeAmount[SaveImage::KI_DLC_GAME_MODE_SLOT]);
    LoadImageField(lpaImageDLC, SaveImageDLC1::KU_MODE_DISCOVERED_DLC_SLOT,      maGameModeTypeAmountDiscovered[SaveImage::KI_DLC_GAME_MODE_SLOT]);
    LoadImageField(lpaImageDLC, SaveImageDLC1::KU_MODE_COMPLETED_DLC_SLOT,       maGameModeTypeAmountCompleted[SaveImage::KI_DLC_GAME_MODE_SLOT]);
    LoadImageField(lpaImageDLC, SaveImageDLC1::KU_MODE_COMPLETED_START_DLC_SLOT, maGameModeTypeAmountCompletedSinceTheStart[SaveImage::KI_DLC_GAME_MODE_SLOT]);

    LoadImageField(lpaImage, SaveImage::KU_TOTAL_TAKEDOWN_COUNT,        miTotalTakedownCount);
    LoadImageField(lpaImage, SaveImage::KU_TOTAL_ONLINE_VERT_TAKEDOWNS, miTotalOnlineVerticleTakedownCount);
    LoadImageBlock(lpaImage, SaveImage::KU_TAKEDOWN_TYPE_COUNTS, maiTakedownTypeCounts,         sizeof(maiTakedownTypeCounts));
    LoadImageBlock(lpaImage, SaveImage::KU_WINS_PER_MODE,        maiWinsPerOfflineGameMode,     sizeof(maiWinsPerOfflineGameMode));
    LoadImageBlock(lpaImage, SaveImage::KU_RANK_WINS_PER_MODE,   maiRankWinsPerOfflineGameMode, sizeof(maiRankWinsPerOfflineGameMode));
    LoadImageBlock(lpaImage, SaveImage::KU_LOSSES_PER_MODE,      maiLossesPerOfflineGameMode,   sizeof(maiLossesPerOfflineGameMode));

    LoadImageField(lpaImage, SaveImage::KU_COMPLETED_BARREL_ROLLS,         miCompletedBarrelRolls);
    LoadImageField(lpaImage, SaveImage::KU_COMPLETED_AIR_SPIN_ANGLE,       mfCompletedAirSpinAngle);
    LoadImageField(lpaImage, SaveImage::KU_COMPLETED_HANDBRAKE_TURN_ANGLE, mfCompletedHandbreakTurnAngle);
    LoadImageField(lpaImage, SaveImage::KU_COMPLETED_DRIFT_DISTANCE,       mfCompletedDriftDistance);
    LoadImageField(lpaImage, SaveImage::KU_ONCOMING_DISTANCE,              mfOncomingDistance);
    LoadImageField(lpaImage, SaveImage::KU_AIR_MAXIMUM,                    mfAirMaximum);
    LoadImageField(lpaImage, SaveImage::KU_HIGHEST_SHOWTIME_SCORE,         miHighestShowTimeScore);
    LoadImageField(lpaImage, SaveImage::KU_BEST_STUNT_RUN_SCORE,           miBestStuntRunScore);

    // The record arrays and their counts come back from the BASE runs only; the DLC runs
    // are appended by the DLC1 block at the end (the SplitArray inverse).
    LoadImageField(lpaImage, SaveImage::KU_CAR_COUNT,    miCarCount);
    LoadImageField(lpaImage, SaveImage::KU_LIVERY_COUNT, miLiveryDataCount);
    LoadImageField(lpaImage, SaveImage::KU_RIVAL_COUNT,  miRivalCount);
    LoadImageField(lpaImage, SaveImage::KU_EVENT_COUNT,  miEventCount);

    CGS_STATIC_ASSERT((sizeof(maCars)          == 12288), "X360 memcpy 0x3000 (512 * 0x18)");
    CGS_STATIC_ASSERT((sizeof(maLiveryChoices) == 12288), "X360 memcpy 0x3000 (512 * 0x18)");
    CGS_STATIC_ASSERT((sizeof(maRivals)        == 3584),  "X360 memcpy 0xE00 (64 * 0x38)");
    CGS_STATIC_ASSERT((sizeof(maEvents)        == 1400),  "X360 memcpy 0x578 (175 * 8)");
    LoadImageBlock(lpaImage, SaveImage::KU_CARS,     maCars,          sizeof(maCars));
    LoadImageBlock(lpaImage, SaveImage::KU_LIVERIES, maLiveryChoices, sizeof(maLiveryChoices));
    LoadImageBlock(lpaImage, SaveImage::KU_RIVALS,   maRivals,        sizeof(maRivals));
    LoadImageBlock(lpaImage, SaveImage::KU_EVENTS,   maEvents,        sizeof(maEvents));

    LoadImageBlock(lpaImage, SaveImage::KU_STUNT_ELEMENT_SETS, maStuntElements, sizeof(maStuntElements));

    LoadImageField(lpaImage, SaveImage::KU_MEDAL_COUNT_FROM_THE_START, muMedalCountFromTheStart);
    LoadImageField(lpaImage, SaveImage::KU_SILVER_CARS_UNLOCKED,       mbSilverCarsUnlocked);
    LoadImageField(lpaImage, SaveImage::KU_GOLD_CARS_UNLOCKED,         mbGoldCarsUnlocked);

    // The mirrored duplicated body-shop restore (see Serialise): mJunkYardsDriveThruSet is
    // NOT restored, so it stays zeroed by the memset above. Console defect, reproduced.
    LoadImageBlock(lpaImage, SaveImage::KU_BODY_SHOPS_SET, &mBodyShopsDriveThruSet, sizeof(mBodyShopsDriveThruSet));
    LoadImageBlock(lpaImage, SaveImage::KU_BODY_SHOPS_SET, &mBodyShopsDriveThruSet, sizeof(mBodyShopsDriveThruSet));
    LoadImageBlock(lpaImage, SaveImage::KU_PAINT_SHOPS_SET,  &mPaintShopsDriveThruSet,  sizeof(mPaintShopsDriveThruSet));
    LoadImageBlock(lpaImage, SaveImage::KU_GAS_STATIONS_SET, &mGasStationsDriveThruSet, sizeof(mGasStationsDriveThruSet));
    LoadImageBlock(lpaImage, SaveImage::KU_CAR_PARKS_SET,    &mCarParksDriveThruSet,    sizeof(mCarParksDriveThruSet));

    LoadImageBlock(lpaImage, SaveImage::KU_FREEBURN_CHALLENGE_DATA, &maFreeBurnChallengeData, sizeof(maFreeBurnChallengeData));
    LoadImageBlock(lpaImage, SaveImage::KU_HIT_PROP_BITS,           &mabHitPropBitArray,      sizeof(mabHitPropBitArray));
    LoadImageBlock(lpaImage, SaveImage::KU_STUNT_COUNTS_BY_COUNTY,  maaiStuntCountsByCounty,  sizeof(maaiStuntCountsByCounty));
    LoadImageBlock(lpaImage, SaveImage::KU_NETWORK_CHALLENGE_DATA,  maNetworkChallengeData,   sizeof(maNetworkChallengeData));
    LoadImageBlock(lpaImage, SaveImage::KU_CHALLENGE_DATA,          maChallengeData,          sizeof(maChallengeData));
    LoadImageField(lpaImage, SaveImage::KU_LAST_ROAD_RULES_RESET_TIME, muLastRoadRulesResetTime);

    // The licence picture: the pixels and the valid flag come back from the image; the
    // descriptor is REBUILT rather than restored (see the PARK in Serialise). This lands
    // the live NetworkTexture in exactly the state the console's restored descriptor
    // described -- Prepared over this profile's own 9600-byte pixel block at the fixed
    // 160x120 DXT1 the sole writer (SetPlayerLicencePicture @0x8235A020) always uses.
    LoadImageBlock(lpaImage, SaveImage::KU_LICENCE_TEXTURE_DATA, macPlayerLicenceTextureData, sizeof(macPlayerLicenceTextureData));
    LoadImageField(lpaImage, SaveImage::KU_LICENCE_PICTURE_IS_VALID, mbPlayerLicencePictureIsValid);
    mPlayerLicencePicture.Construct();
    if (mbPlayerLicencePictureIsValid)
    {
        mPlayerLicencePicture.Prepare(&macPlayerLicenceTextureData[0],
                                      KI_PLAYERLICENCEPICTURE_TEXTURESIZEINBYTES,
                                      KI_PLAYERLICENCEPICTURE_WIDTH,
                                      KI_PLAYERLICENCEPICTURE_HEIGHT,
                                      renderengine::PIXELFORMAT_DXT1);
    }

    for (s32 liGallery = 0; liGallery < SaveImage::KI_MUGSHOT_GALLERY_COUNT; ++liGallery)
    {
        LoadImageBlock(lpaImage,
                       SaveImage::KU_MUGSHOT_GALLERIES
                           + static_cast<u32>(liGallery) * SaveImage::KU_MUGSHOT_GALLERY_STRIDE,
                       &maaMugshotInfo[liGallery], sizeof(maaMugshotInfo[liGallery]));
    }

    LoadImageBlock(lpaImage, SaveImage::KU_MUGSHOT_FILE_ID_BITS, maAvailableMugshotFileIDs, sizeof(maAvailableMugshotFileIDs));
    LoadImageBlock(lpaImage, SaveImage::KU_CAR_TYPES,            mafCarTypes,               sizeof(mafCarTypes));
    LoadImageField(lpaImage, SaveImage::KU_CURRENT_CAR_TYPE,     meCurrentCarType);
    LoadImageBlock(lpaImage, SaveImage::KU_TRAINING_SEEN_BITS,   &maHasPlayerSeenTraining,  sizeof(maHasPlayerSeenTraining));

    LoadImageField(lpaImage, SaveImage::KU_NUM_ONLINE_RACES_DONE,       miNumOnlineRacesDone);
    LoadImageField(lpaImage, SaveImage::KU_NUM_ONLINE_RACES_WON,        miNumOnlineRacesWon);
    LoadImageField(lpaImage, SaveImage::KU_NUM_MUGSHOTS_SENT,           miNumMugshotsSent);
    LoadImageField(lpaImage, SaveImage::KU_DATE_LICENCE_ISSUED,         mDateLicenceIssued);
    LoadImageField(lpaImage, SaveImage::KU_DATE_100_PERCENT_COMPLETED,  mDate100PercentCompleted);
    LoadImageField(lpaImage, SaveImage::KU_HIGHEST_ROAD_RAGE_TAKEDOWNS, miHighestNumberOfTakeDownsInRoadRage);
    LoadImageField(lpaImage, SaveImage::KU_SEEN_TROPHY_BITS,            mSeenTrophyAwardBitArray);

    LoadImageField(lpaImage, SaveImage::KU_SEEN_100_PERCENT_SEQUENCE,  mb100PercentCompletionSequenceShown);
    LoadImageField(lpaImage, SaveImage::KU_IS_NEW_PROFILE,             mbIsNewProfile);
    LoadImageField(lpaImage, SaveImage::KU_CREDITS_SEQUENCE_VIEWED,    mbCreditsSequenceViewed);
    LoadImageField(lpaImage, SaveImage::KU_ONE_HUNDRED_HUD_MSG_VIEWED, mbOneHundredHudMessageViewed);
    LoadImageField(lpaImage, SaveImage::KU_HAS_UNLOCKED_CREDITS,       mbHasUnlockedCredits);
    LoadImageField(lpaImage, SaveImage::KU_HAVE_SET_100_PERCENT_DATE,  mbHaveSet100PercentCompletedDate);
    LoadImageField(lpaImage, SaveImage::KU_SEEN_ELITE_SEQUENCE,        mbHaveSeenEliteCompletionSequence);

    LoadImageField(lpaImage, SaveImage::KU_ROAD_RULES_ID_LOW_BITS,   muRoadRulesIDLowBits);
    LoadImageField(lpaImage, SaveImage::KU_SEEN_ALL_EVENT_TYPE_BITS, mSeenCompleteAllEventTypeArray);
    LoadImageField(lpaImage, SaveImage::KU_REAL_TIME_PLAYED,         mfRealTimePlayed);
    LoadImageField(lpaImage, SaveImage::KU_REDUNDANT_FLOAT4,         mfRedundantFloat4);
    LoadImageField(lpaImage, SaveImage::KU_ROAD_RULES_ID_HIGH_BITS,  muRoadRulesIDHighBits);

    // ---- the PROFILEUPG prop-hit re-index, run the other way -----------------------------
    if (lpUpgrade != 0)
    {
        const CgsContainers::BitArray<SaveImage::KU_HIT_PROP_BIT_COUNT>* lpImageHitProps =
            ImageObjectAt<CgsContainers::BitArray<SaveImage::KU_HIT_PROP_BIT_COUNT> >(
                lpaImage, SaveImage::KU_HIT_PROP_BITS);

        for (s32 liEntry = 0; liEntry < lpUpgrade->miCount; ++liEntry)
        {
            const ProfileUpgradeTable::Entry& lrEntry = lpUpgrade->mpaEntries[liEntry];

            CGS_ASSERT(lrEntry.muImageBitIndex < SaveImage::KU_HIT_PROP_BIT_COUNT,
                       "luIndex < NUMBITS");                                  // CgsBitArray.h:203
            CGS_ASSERT(lrEntry.muLiveBitIndex < SaveImage::KU_HIT_PROP_BIT_COUNT,
                       "luIndex < NUMBITS");                                  // CgsBitArray.h:222 / :241

            if (lpImageHitProps->IsBitSet(lrEntry.muImageBitIndex))
            {
                mabHitPropBitArray.SetBit(lrEntry.muLiveBitIndex);
            }
            else
            {
                mabHitPropBitArray.UnSetBit(lrEntry.muLiveBitIndex);
            }
        }
    }

    // ---- merge the DLC1 image ------------------------------------------------------------
    // Everything below is gated on the DLC1 image being version-current: a save written by
    // a build without the DLC (or one Serialise never touched) leaves the live profile as
    // the base image restored it.
    s32 liImageVersionDLC1 = 0;
    LoadImageField(lpaImageDLC, SaveImageDLC1::KU_VERSION, liImageVersionDLC1);
    if (liImageVersionDLC1 != BrnGuiSaveLoad::ProfileDLC1::KI_VERSION_CURRENT)
    {
        return;
    }

    // The true spawn car (the base image may hold the non-DLC substitute Serialise picked).
    LoadImageField(lpaImageDLC, SaveImageDLC1::KU_SPAWN_CAR_ID, mSpawnCarId);
    mSpawnWheelId = 0;

    s32 liDlcCarCount = 0;
    LoadImageField(lpaImageDLC, SaveImageDLC1::KU_DLC_CAR_COUNT, liDlcCarCount);
    for (s32 liDlcCar = 0; liDlcCar < liDlcCarCount; ++liDlcCar)
    {
        LoadImageBlock(lpaImageDLC,
                       SaveImageDLC1::KU_DLC_CARS
                           + static_cast<u32>(liDlcCar) * sizeof(BrnGuiSaveLoad::CarData),
                       &maCars[miCarCount + liDlcCar], sizeof(BrnGuiSaveLoad::CarData));
    }
    miCarCount += liDlcCarCount;

    s32 liDlcLiveryCount = 0;
    LoadImageField(lpaImageDLC, SaveImageDLC1::KU_DLC_LIVERY_COUNT, liDlcLiveryCount);
    for (s32 liDlcLivery = 0; liDlcLivery < liDlcLiveryCount; ++liDlcLivery)
    {
        LoadImageBlock(lpaImageDLC,
                       SaveImageDLC1::KU_DLC_LIVERIES
                           + static_cast<u32>(liDlcLivery) * sizeof(BrnGuiSaveLoad::LiveryData),
                       &maLiveryChoices[miLiveryDataCount + liDlcLivery], sizeof(BrnGuiSaveLoad::LiveryData));
    }
    miLiveryDataCount += liDlcLiveryCount;

    // (The X360 interleaves these two block restores between the livery and rival merges;
    // order preserved.)
    LoadImageBlock(lpaImageDLC, SaveImageDLC1::KU_TARGET_EVENT_SCORES,    &maTargetEventScores,   sizeof(maTargetEventScores));
    LoadImageBlock(lpaImageDLC, SaveImageDLC1::KU_EVENT_SCORES_TO_UPLOAD, &maEventScoresToUpload, sizeof(maEventScoresToUpload));

    s32 liDlcRivalCount = 0;
    LoadImageField(lpaImageDLC, SaveImageDLC1::KU_DLC_RIVAL_COUNT, liDlcRivalCount);
    for (s32 liDlcRival = 0; liDlcRival < liDlcRivalCount; ++liDlcRival)
    {
        LoadImageBlock(lpaImageDLC,
                       SaveImageDLC1::KU_DLC_RIVALS
                           + static_cast<u32>(liDlcRival) * sizeof(BrnGuiSaveLoad::RivalData),
                       &maRivals[miRivalCount + liDlcRival], sizeof(BrnGuiSaveLoad::RivalData));
    }
    miRivalCount += liDlcRivalCount;

    // FLAG (console asymmetry, reproduced): there is NO event merge here. Serialise writes a
    // DLC ProfileEvent run (count @+0xAE4, records @+0x24A8, cap 48) that Deserialise never
    // reads back -- the X360 body references neither offset (verified by scanning the whole
    // 0x8237D308 disassembly for 0xAE4 / 0x24A8: no hits, while 0xAD8 / 0xADC / 0xAE0 all
    // appear three times each). DLC-only event progress is therefore write-only in the save.
    // Not a reconstruction gap: the console does not do it.

    // The developer-challenge bits, transcribed back from the image's FastBitArray<120>
    // into the live FastBitArray<15>.
    const DeveloperChallengeImageBits* lpImageChallengeBits =
        ImageObjectAt<DeveloperChallengeImageBits>(lpaImageDLC, SaveImageDLC1::KU_DEVELOPER_CHALLENGE_BITS);

    mDeveloperChallengesCompleted.UnSetAll();
    for (DeveloperChallengeImageBits::Iterator lIt = lpImageChallengeBits->Begin();
         lIt != lpImageChallengeBits->End();
         ++lIt)
    {
        // BrnProfile.cpp:2292 -- the only BrnProfile.cpp-level assert in the loop; the rest
        // of the machinery in the X360 body is CgsFastBitArray.h's own inlined guards.
        CGS_ASSERT(lIt.GetIndex() < 15,
                   "lIt.GetIndex() < GsmIO::E_DEVELOPER_CHALLENGE_COUNT");
        mDeveloperChallengesCompleted.SetBit(static_cast<u32>(lIt.GetIndex()));
    }
}

}  // namespace BrnProgression
