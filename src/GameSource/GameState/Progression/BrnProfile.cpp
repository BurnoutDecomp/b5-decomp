// ===================================================================================
// BrnProgression::Profile  -- the player's persisted progression record.
//   GameSource/Unity/../GameState/Progression/BrnProfile.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the ASM is the spine; offsets/constants are
// proven there) + the DecFIGS DWARF (member names/types/order) + the Feb-2007 idiom. The
// full byte-exact Profile layout lives in BrnProfile.h; every member here is reached BY NAME.
// ===================================================================================

#include "BrnProfile.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/System/Timer/PS3/CgsDateAndTimePS3.h"
#include "GameShared/GameClasses/Network/Texture/CgsNetworkTexture.h"
#include "GameShared/GameClasses/Network/Utilities/CgsNetworkImageConverter.h"
#include "pc/gcm/renderengine/pixelformat.h"

#include <string.h>   // memset

namespace BrnProgression
{

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

    // The four 17-entry game-mode-type tally arrays (X360 unrolls a 13-iteration zero loop
    // over the first words; the rest land in the wider zeroing below). Zero them whole.
    for (liIndex = 0; liIndex < 17; ++liIndex)
    {
        maGameModeTypeAmount[liIndex]                       = 0;
        maGameModeTypeAmountDiscovered[liIndex]             = 0;
        maGameModeTypeAmountCompleted[liIndex]              = 0;
        maGameModeTypeAmountCompletedSinceTheStart[liIndex] = 0;
    }

    miTotalTakedownCount               = 0;
    miTotalOnlineVerticleTakedownCount = 0;
    for (liIndex = 0; liIndex < 17; ++liIndex)
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

    // The flagged late tail (trophy/achievement bit arrays + remaining tail fields) is zeroed
    // wholesale -- the X360 late stores in this region (at +120032 / +120824 / +120832) are all
    // zero, matching this clear.
    memset(&mPad_PostHighRoadRage[0], 0, sizeof(mPad_PostHighRoadRage));
    memset(&mPad_LateTail[0],         0, sizeof(mPad_LateTail));
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

} // namespace BrnProgression
