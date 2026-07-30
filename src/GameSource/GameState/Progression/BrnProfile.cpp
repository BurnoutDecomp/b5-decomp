// ===================================================================================
// BrnProgression::Profile  -- the player's persisted progression record.
//   GameSource/Unity/../GameState/Progression/BrnProfile.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the ASM is the spine; offsets/constants are
// proven there) + the DecFIGS DWARF (member names/types/order) + the Feb-2007 idiom. The
// full byte-exact Profile layout lives in BrnProfile.h; every member here is reached BY NAME.
// ===================================================================================

#include "BrnProfile.h"
#include "GameSource/Gui/SaveLoad/BrnGuiSaveLoadProfileDLC1.h"   // BrnGuiSaveLoad::ProfileDLC1::IsDLCCarId (SplitArray DLC test)
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/System/Timer/PS3/CgsDateAndTimePS3.h"
#include "GameShared/GameClasses/Network/Texture/CgsNetworkTexture.h"
#include "GameShared/GameClasses/Network/Utilities/CgsNetworkImageConverter.h"
#include "pc/gcm/renderengine/pixelformat.h"

#include <string.h>   // memset

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
    CGS_ASSERT((u32)liColour < 256, "luColourIndex < 256");
    mu8ColourIndex = (u8)liColour;
}

// ------------------------------------------------------------------------------------
// CarData::SetPaletteIndex -- X360 0x823548F0. Range-assert the index fits in a byte,
// then store the low byte to mu8PaletteIndex (+0x09).
// ------------------------------------------------------------------------------------
void CarData::SetPaletteIndex(s32 liPalette)
{
    CGS_ASSERT((u32)liPalette < 256, "luPaletteIndex < 256");
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
    mCarDirection.x = 1.0f;                          // (1,0,0,0)
    mCarDirection.y = 0.0f;
    mCarDirection.z = 0.0f;
    mCarDirection.w = 0.0f;

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
    mbGoldCarsUnlocked   = false;
    mbSilverCarsUnlocked = false;

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

    // The five mugshot galleries: each is an Array<MugshotInfo,20> (Construct -> empty) plus a
    // 30-bit "available file id" bit array (all bits clear). The X360 walks the five galleries
    // and asserts the running index stays <= E_IMAGE_GALLERY_TYPE_COUNT (== 5).
    for (liIndex = 0; liIndex < 5; ++liIndex)
    {
        maaMugshotInfo[liIndex].Construct();
        memset(&maAvailableMugshotFileIDs[liIndex], 0, sizeof(maAvailableMugshotFileIDs[liIndex]));
        CGS_ASSERT(liIndex + 1 <= 5, "leEnumIndex <= E_IMAGE_GALLERY_TYPE_COUNT");
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
    CGS_ASSERT(miCarCount < KI_MAX_PROFILE_CAR_COUNT, "miCarCount < KI_MAX_PROFILE_CAR_COUNT");

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
    CGS_ASSERT(miEventCount < KI_MAX_EVENTS, "miEventCount < KI_MAX_EVENTS");

    s32 liIndex = miEventCount;
    ProfileEvent& lEvent = maEvents[liIndex];
    ++miEventCount;

    lEvent.Construct(luEventID);   // X360 open-codes: id = luEventID, flags = 0
    return &lEvent;
}

// ====================================================================================
// Profile::AddRival  @ 0x82359F40
// Append a new rival record (the two ids, everything else zeroed).
// ====================================================================================
RivalData* Profile::AddRival(CgsID lRivalId, CgsID lCarId)
{
    CGS_ASSERT(miRivalCount < KI_MAX_RIVAL_COUNT - 1, "miRivalCount < KI_MAX_RIVAL_COUNT - 1");

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
    s32 liCount = miCarCount;
    if (liCount <= 0)
        return 0;

    for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
    {
        if (maCars[liIndex].mId == lCarId)
            return &maCars[liIndex];
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
    s32 liCount = miRivalCount;
    if (liCount <= 0)
        return 0;

    for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
    {
        if (maRivals[liIndex].mRivalId == lRivalId)
            return &maRivals[liIndex];
    }
    return 0;
}

// ====================================================================================
// Profile::GetChosenLiveryDataForBaseCar  @ 0x82359DC8
// The livery record keyed on lBaseCarId, or NULL.
// ====================================================================================
LiveryData* Profile::GetChosenLiveryDataForBaseCar(CgsID lBaseCarId)
{
    s32 liCount = miLiveryDataCount;
    if (liCount <= 0)
        return 0;

    for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
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

        CGS_ASSERT(liIndex < KI_MAX_PROFILE_CAR_COUNT, "liIndex < KI_MAX_PROFILE_CAR_COUNT");
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

    CGS_ASSERT(lpCarData, "lpCarData");
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
    CGS_ASSERT(leMugshotType < 5, "leMugshotType < GsmIO::E_IMAGE_GALLERY_TYPE_COUNT");

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
    CGS_ASSERT(lpNewPlayerImage, "lpNewPlayerImage");

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
// Profile::AddMugshot  @ 0x82370D70
// Add (or replace the oldest non-locked) mugshot in gallery leMugshotType. Returns the
// assigned file id, or -1 if no slot is free. The X360 first claims a free file-id from
// the gallery's available-id bit array; if the gallery is full it evicts the first
// non-locked entry and reuses its file id. (Pseudocode for this function suffered a local-
// allocation failure in the decompiler; reconstructed to its observable behaviour with the
// real Array<MugshotInfo,20> / BitArray<30> members reached by name.)
// ====================================================================================
s32 Profile::AddMugshot(s32 leMugshotType, MugshotUniqueIdArg /*lUniqueID*/,
                        CgsSystem::DateAndTime /*lDateTaken*/, s32 /*leWorldRegion*/)
{
    CGS_ASSERT(leMugshotType != 5, "leMugshotType != GsmIO::E_IMAGE_GALLERY_TYPE_COUNT");

    CgsContainers::BitArray<30u>& lFileIds = maAvailableMugshotFileIDs[leMugshotType];

    // INCOMPLETE (dependency-blocked on MugshotInfo's internal layout: locked flag @+0x32,
    // file-id @+0x30, UniquePlayerID region -- currently an opaque pad). Both X360 paths build a
    // MugshotInfo record and call MugshotInfo_20_::Append (free-id path @0x82370F08; full-gallery
    // path evicts the first non-locked entry, reuses its +0x30 file id, then Append). This body
    // claims/returns the file id but does NOT append the record; reconstruct the Append paths once
    // MugshotInfo is recovered.
    // Claim the lowest available (still-clear) file id, if any remain.
    s32 liFileId = -1;
    for (u32 luBit = 0; luBit < 20u; ++luBit)
    {
        if (!lFileIds.IsBitSet(luBit))
        {
            liFileId = static_cast<s32>(luBit);
            lFileIds.SetBit(luBit);
            break;
        }
    }

    if (liFileId < 0)
    {
        // Gallery exhausted: the X360 evicts the first non-locked entry and reuses its id.
        CGS_ASSERT(maaMugshotInfo[leMugshotType].IsFull(),
                   "maaMugshotInfo[leMugshotType].IsFull()");
        // The per-entry "locked" flag + UniquePlayerID copy live in MugshotInfo's (not-yet-
        // recovered) body; without that layout the eviction copy cannot be reconstructed
        // byte-faithfully, so on a full gallery we report no free slot.
        return -1;
    }

    return liFileId;
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
// Profile::AddGameModeTypeCompleted  @ 0x82354B10
// Bump the completed (and completed-since-the-start) tallies for one game-mode type.
// ====================================================================================
void Profile::AddGameModeTypeCompleted(GsmIO::EGameModeType lEGameModeType)
{
    CGS_ASSERT(lEGameModeType > GsmIO::E_MODE_NONE, "lEGameModeType > GsmIO::E_MODE_NONE");

    ++maGameModeTypeAmountCompleted[lEGameModeType];
    ++maGameModeTypeAmountCompletedSinceTheStart[lEGameModeType];
}

// ====================================================================================
// Profile::AddGameModeTypeToDiscovered  @ 0x82354AA0
// Bump the discovered tally for one game-mode type.
// ====================================================================================
void Profile::AddGameModeTypeToDiscovered(GsmIO::EGameModeType lEGameModeType)
{
    CGS_ASSERT(lEGameModeType > GsmIO::E_MODE_NONE, "lEGameModeType > GsmIO::E_MODE_NONE");

    ++maGameModeTypeAmountDiscovered[lEGameModeType];
}

// ====================================================================================
// Profile::GetGameModeTypeAmount  @ 0x82354A38
// ====================================================================================
s32 Profile::GetGameModeTypeAmount(GsmIO::EGameModeType lEGameModeType) const
{
    CGS_ASSERT(lEGameModeType > GsmIO::E_MODE_NONE, "lEGameModeType > GsmIO::E_MODE_NONE");

    return maGameModeTypeAmount[lEGameModeType];
}

// ====================================================================================
// Profile::GetGameModeTypeDiscovered  @ 0x8240E940
// ====================================================================================
s32 Profile::GetGameModeTypeDiscovered(GsmIO::EGameModeType lEGameModeType) const
{
    CGS_ASSERT(lEGameModeType > GsmIO::E_MODE_NONE, "lEGameModeType > GsmIO::E_MODE_NONE");

    return maGameModeTypeAmountDiscovered[lEGameModeType];
}

// ====================================================================================
// Profile::AddWinForGameMode  @ 0x82354C80
// Bump both the win and the rank-win tallies for one OFFLINE game-mode.
// ====================================================================================
void Profile::AddWinForGameMode(GsmIO::EGameModeType leGameModeType)
{
    CGS_ASSERT((leGameModeType < GsmIO::E_MODE_OFFLINE_COUNT) && (leGameModeType > GsmIO::E_MODE_NONE),
               "( leGameModeType < GsmIO::E_MODE_OFFLINE_COUNT ) && ( leGameModeType > GsmIO::E_MODE_NONE )");

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
               "( leGameModeType < GsmIO::E_MODE_OFFLINE_COUNT ) && ( leGameModeType > GsmIO::E_MODE_NONE )");

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
               "( leGameModeType < GsmIO::E_MODE_OFFLINE_COUNT ) && ( leGameModeType > GsmIO::E_MODE_NONE )");

    return maiRankWinsPerOfflineGameMode[leGameModeType];
}

// ====================================================================================
// Profile::GetNumLossesForGameMode  @ 0x8230FAB0
// ====================================================================================
s32 Profile::GetNumLossesForGameMode(GsmIO::EGameModeType leGameModeType) const
{
    CGS_ASSERT((leGameModeType < GsmIO::E_MODE_OFFLINE_COUNT) && (leGameModeType > GsmIO::E_MODE_NONE),
               "( leGameModeType < GsmIO::E_MODE_OFFLINE_COUNT ) && ( leGameModeType > GsmIO::E_MODE_NONE )");

    return maiLossesPerOfflineGameMode[leGameModeType];
}

// ====================================================================================
// Profile::AddTakedown  @ 0x82354C00
// Bump the total takedown count and the per-type tally.
// ====================================================================================
void Profile::AddTakedown(BrnGameState::ETakedownType leTakedownType)
{
    CGS_ASSERT(leTakedownType > BrnGameState::E_TAKEDOWN_NONE,
               "leTakedownType > BrnGameState::E_TAKEDOWN_NONE");

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
               "leStuntElementType < BrnGameState::E_STUNT_ELEMENT_TYPE_COUNT");

    if (maStuntElements[leStuntElementType].Find(lId) == Set<CgsID, 512u>::KU_INVALID &&
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
    return maStuntElements[leStuntElementType].Find(lId) != Set<CgsID, 512u>::KU_INVALID;
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
               "leStuntElementType < BrnGameState::E_STUNT_ELEMENT_TYPE_COUNT");
    CGS_ASSERT(leCounty < BrnWorld::E_COUNTY_VALID_COUNT,
               "leCounty < BrnWorld::E_COUNTY_VALID_COUNT");

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
               "liCarIndex >= 0 && liCarIndex < miCarCount");

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
               "luIndex < static_cast<uint32_t>(miEventCount)");

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

    CGS_ASSERT(lpCarData, "lpCarData");
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
    CGS_ASSERT(leModeType > -1, "leModeType > RaceEventData::E_MODE_INVALID");   // :3320
    CGS_ASSERT(leModeType < 6,  "leModeType < RaceEventData::E_MODE_COUNT");     // :3321
    CGS_ASSERT(static_cast<u32>(leModeType) < 6u, "invalid index : < 6");        // CgsBitArray.h:203 Get guard

    return mSeenCompleteAllEventTypeArray.IsBitSet(static_cast<u32>(leModeType));
}

// ====================================================================================
// Profile::SetSeenAllEventTypeWonMessage  @ 0x823620A8
// ====================================================================================
void Profile::SetSeenAllEventTypeWonMessage(s32 leModeType)
{
    CGS_ASSERT(leModeType > -1, "leModeType > RaceEventData::E_MODE_INVALID");   // :3336
    CGS_ASSERT(leModeType < 6,  "leModeType < RaceEventData::E_MODE_COUNT");     // :3337
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
    CGS_ASSERT(leUnlockType > 0,  "leUnlockType > BrnProgression::TrophyUnlockData::E_UNLOCKTYPE_NONE");  // :3287
    CGS_ASSERT(leUnlockType < 35, "leUnlockType < BrnProgression::TrophyUnlockData::E_UNLOCKTYPE_COUNT"); // :3288
    CGS_ASSERT(static_cast<u32>(leUnlockType) < 35u, "invalid index : < 35");    // CgsBitArray.h:203 Get guard

    return mSeenTrophyAwardBitArray.IsBitSet(static_cast<u32>(leUnlockType));
}

// ====================================================================================
// Profile::SetSeenTrophyUnlockSequence  @ 0x824BAA30
// ====================================================================================
void Profile::SetSeenTrophyUnlockSequence(s32 leUnlockType)
{
    CGS_ASSERT(leUnlockType > 0,  "leUnlockType > BrnProgression::TrophyUnlockData::E_UNLOCKTYPE_NONE");  // :3303
    CGS_ASSERT(leUnlockType < 35, "leUnlockType < BrnProgression::TrophyUnlockData::E_UNLOCKTYPE_COUNT"); // :3304
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
               "leTrainingType >= 0 && leTrainingType < E_TRAINING_TYPE_COUNT");  // :2761
    CGS_ASSERT(static_cast<u32>(leTrainingType) < 256u, "Index: ");               // CgsBitArray.h:222 SetBit guard

    maHasPlayerSeenTraining.SetBit(static_cast<u32>(leTrainingType));
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
        CGS_ASSERT(lpTargetEventScore, "lpTargetEventScore");   // BrnProfile.cpp:1604
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
        CGS_ASSERT(lpEventScoreToUpload, "lpEventScoreToUpload");   // BrnProfile.cpp:1714
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

} // namespace BrnProgression
