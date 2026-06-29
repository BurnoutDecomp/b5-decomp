#ifndef BRN_PROFILE_H
#define BRN_PROFILE_H

#include "types.hpp"
#include "BrnCommonTypes.h"                               // CgsID, Vector3 (rw::math::vpu::Vector3)
#include "SharedClasses/Progression/BrnTrainingTypes.h"  // BrnProgression::ETrainingType (Profile training-flag accessors)
#include "SharedClasses/Trigger/BrnGenericRegion.h"      // BrnTrigger::GenericRegion::Type (drive-thru discovery accessors)
#include "GameShared/GameClasses/Containers/CgsSet.h"     // Set<CgsID,N> (stunt-element + drive-thru sets, embedded by value)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"// CgsContainers::BitArray<N> (training/trophy/achievement/mugshot bit sets)
#include "GameShared/GameClasses/Containers/CgsArray.h"   // CgsContainers::Array<T,N> (freeburn-challenge + mugshot arrays)
#include "GameShared/GameClasses/System/Timer/PS3/CgsDateAndTimePS3.h"  // CgsSystem::DateAndTime (licence / 100% dates)
#include "GameShared/GameClasses/Network/Texture/CgsNetworkTexture.h"   // CgsNetwork::NetworkTexture (player licence picture)
#include "SharedClasses/StreetData/BrnChallengeData.h"                  // BrnStreetData::ChallengePlayerScoreEntry
#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h" // BrnStreetData::ChallengeHighScoreEntry
#include "BrnProgressionCarData.h"        // BrnProgression::CarData (maCars element, full layout)
#include "BrnProgressionLiveryData.h"     // BrnProgression::LiveryData (maLiveryChoices element)
#include "BrnProgressionRivalData.h"      // BrnProgression::RivalData (maRivals element)

namespace BrnProgression
{

struct ProfileEvent
{
    // DWARF BrnProfile.h:296 (Flags enum).
    enum Flags
    {
        E_FLAG_UNDISCOVERED             = 0,
        E_FLAG_DISCOVERED               = 1,
        E_FLAG_FINISHED                 = 2,
        E_FLAG_RANK_WIN                 = 4,
        E_FLAG_NON_RANK_WIN             = 8,
        E_FLAG_WON_SPECIAL_EVENT_BEFORE = 16,
        E_FLAG_WON_EVENT_BEFORE         = 32
    };

    void Construct(u32 luEventID);
    u32  GetID() const;
    u16  GetFlags() const;
    void SetFlags(u16 lu16Flags);
    bool IsFlagSet(Flags leFlag) const;
    void EnableFlags(u16 lu16Flags);
    void ClearFlags(u16 lu16Flags);
    bool IsFound() const;
    void SetFound(bool lbFound);

private:
    u32 muEventID;   // BrnProfile.h:335  (+0)
    u16 muFlags;     // BrnProfile.h:336  (+4)
    // 2 bytes trailing pad -> 8-byte stride (X360 GetItem: 8*index)
};

// Minimal owning slice for BrnProgression::MugshotInfo -- the element type of
// Array<BrnProgression::MugshotInfo, 20>. The X360 BINARY proves N=20 and a 56-byte stride
// (Array<MugshotInfo,20>::GetItem returns `56*index + this`; live-count word at +1120 == 20*56;
// IsFull returns count==20). The full internal layout (incl. MugshotInfo::UniquePlayerID) is
// NOT yet reverse-engineered; modelled as an opaque 56-byte buffer so the container stride is
// exact without fabricating member names. The owning TU of MugshotInfo's real members must
// replace this pad.
struct MugshotInfo
{
    struct UniquePlayerID;   // declared-only: real layout not recovered here

private:
    u8 mPad_Body[56];        // sizeof(MugshotInfo) == 56 (X360 Array<MugshotInfo,20>::GetItem stride)
};

// ============================================================================================
// BrnProgression::Profile -- the player's persisted progression record (embedded by value
// inside ProgressionManager at +0x170).
//
// FULL BYTE-EXACT LAYOUT. Member names/types/order are the DecFIGS DWARF (BrnProfile.h:1171..);
// EVERY offset below is proven against BURNOUT_X360_ARTIST.XEX -- the count words
// (miCarCount@0x26C, miLiveryDataCount@0x270, miRivalCount@0x274, miEventCount@0x278), the
// per-record array bases (maCars@0x280 / maLiveryChoices@0x3280 / maRivals@0x6280 /
// maEvents@0x7080), the licence-picture region (NetworkTexture@102620, texture@102648, valid
// flag@112248), the mugshot region (maaMugshotInfo@112256, fileID bitfields@117896), the dates
// (licence@117996, 100%@118008) and the completion-sequence flags (seen100%@118032,
// seenElite@118038) are all read/written verbatim by this TU's functions and by
// Profile::Construct @0x823708A8 (cross-checked store-by-store).
//
// TWO DWARF-vs-X360 deltas were resolved from the binary (PS3 DWARF drift):
//   * maiTakedownTypeCounts is int32[17] on X360 (DWARF says [13]); the [17] size lands
//     maiWinsPerOfflineGameMode at +468 / maiCompletedBarrelRolls at +588 exactly (proven by
//     DEBUG_ClearMedals' win-array loop @0x1FC/-0x28 and SetBestStuntStats' +588 write).
//   * maNetworkChallengeData[64] (ChallengeHighScoreEntry, 56B each) + maChallengeData[64]
//     (ChallengePlayerScoreEntry, 40B each) sizes land mPlayerLicencePicture at +102620 exactly.
//
// The trophy/achievement bit arrays (DWARF lists them right after the dates) actually live in
// the X360 late tail (Construct's ByteClear<BitArray<60>> stores land at +120032 / +120824),
// not at +118024; that 8-byte gap + the late tail (through the highest proven store at
// +0x1D800==120832, an 8-byte write) is reserved as flagged padding rather than placed at a
// fabricated offset. sizeof(Profile) == 120840.
// ============================================================================================
class Profile
{
public:
    // The persisted profile-format version Construct stamps (X360 Construct stores 28 at +0).
    static const s32 KI_NAME_LENGTH               = 32;
    static const s32 KI_MAX_RIVAL_COUNT           = 64;
    static const s32 KI_MAX_EVENTS                = 175;
    static const s32 KI_MAX_PROFILE_CAR_COUNT     = 512;
    static const s32 KI_VERSION_NUMBER            = 28;   // Construct: *(this+0) = 28
    static const s32 KI_PLAYERLICENCEPICTURE_WIDTH               = 160;
    static const s32 KI_PLAYERLICENCEPICTURE_HEIGHT              = 120;
    static const s32 KI_PLAYERLICENCEPICTURE_TEXTURESIZEINBYTES  = 9600;

    // ------------------------------------------------------------------------
    // Functions reconstructed in this TU (BrnProfile.cpp).
    // ------------------------------------------------------------------------
    void Construct();

    CarData*   AddCar(CgsID lCarId, CarData::UnlockType leUnlockType);
    void       SetCarUnlockAlreadyShown(CgsID lCarId);
    RivalData* AddRival(CgsID lRivalId, CgsID lCarId);
    ProfileEvent* AddEvent(u32 luEventID);

    CarData*       FindCar(CgsID lCarId);
    RivalData*     FindRival(CgsID lCarId);
    LiveryData*    GetChosenLiveryDataForBaseCar(CgsID lBaseCarId);
    void           SetChosenLiveryIdForBaseCar(CgsID lBaseCarId, CgsID lChosenLiveryId);
    CgsID          GetChosenLiveryIdForBaseCar(CgsID lBaseCarId) const;

    // 1-arg convenience overload (declare-only; body in another Progression TU). Named by
    // CarSelectManager::EnterJunkyard with the active player car id -- pins the chosen livery so
    // base == chosen. Kept alongside the 2-arg form this TU implements.
    void           SetChosenLiveryIdForBaseCar(CgsID lBaseCarId);

    void SetBestStuntStats(s32 liCompletedBarrelRolls, f32 lfCompletedAirSpinAngle,
                           f32 lfCompletedHandbreakTurnAngle, f32 lfCompletedDriftDistance);

    // FLAG: leMugshotType is GameStateModuleIO::EImageGalleryType (E_IMAGE_GALLERY_TYPE_COUNT == 5,
    // from DWARF); that enum is not yet homed in the committed tree, so the parameter is the s32 the
    // X360 compares against 5. lUniqueID is MugshotInfo::UniquePlayerID (2 qwords on X360); its real
    // layout is unrecovered (see MugshotInfo), so it is taken as a 16-byte opaque struct here.
    struct MugshotUniqueIdArg { u64 mu64Lo; u64 mu64Hi; };
    s32  AddMugshot(s32 leMugshotType, MugshotUniqueIdArg lUniqueID,
                    CgsSystem::DateAndTime lDateTaken, s32 leWorldRegion);
    s32  GetNumMugshots(s32 leMugshotType);
    s32  GetNumAllMugshots();

    void SetPlayerLicencePicture(const CgsNetwork::NetworkTexture* lpNewPlayerImage);
    CgsSystem::DateAndTime GetLicenceIssuedDate() const;
    void SetLicenceIssuedDateAsNow();
    CgsSystem::DateAndTime Get100PercentCompletedDate() const;

    bool GetSeen100PercentCompletionSequence() const;
    void SetSeen100PercentCompletionSequence();
    bool GetSeenEliteCompletionSequence() const;
    void SetSeenEliteCompletionSequence();

    void DEBUG_ClearMedals();

    // ------------------------------------------------------------------------
    // ADDITIVE GROW (declare-only) -- bodies in other (not-yet-reconstructed) Progression TUs.
    // These were named by already-committed callers and are preserved here verbatim. The full
    // member layout above now backs every offset they reach.
    // ------------------------------------------------------------------------
    void ClearTrainingFlags();
    bool HasPlayerSeenTrainingType(ETrainingType leTrainingType) const;
    void SetTrainingAlreadySeen(ETrainingType leTrainingType);

    bool IsDriveThruDiscoverd(CgsID lId, BrnTrigger::GenericRegion::Type leType) const;
    s32  GetNumDriveThrusDiscovered(BrnTrigger::GenericRegion::Type leType) const;
    f32  GetPlayerBaseDeformAmount(CgsID lCarId) const;
    void IncrementNumDiscoveredEvents();
    ProfileEvent* FindProfileEventByRaceEventId(CgsID lEventId);

    s32             GetCarCount() const;
    const CarData*  GetCarData(s32 liIndex) const;
          CarData*  GetCarData(s32 liIndex);
    bool IsStartOfGameDeformActive() const;

    bool HasPlayerCompletedFreeburnChallenge(CgsID lChallengeID) const;
    u32  CompleteFreeburnChallenge(CgsID lChallengeID);

    bool IsDeveloperChallengeComplete(s32 liChallengeIndex) const;
    void SetDeveloperChallengeComplete(s32 liChallengeIndex);

private:
    // ----- byte-exact member layout (offsets in comments; all X360-proven) -----
    s32   miVersionNumber;                                   // +0
    char  macName[32];                                       // +4
    Vector3 mCarPosition;                                    // +0x30 (48)
    Vector3 mCarDirection;                                   // +0x40 (64)
    CgsID mSpawnCarId;                                       // +0x50 (80)
    CgsID mSpawnWheelId;                                     // +0x58 (88)
    u32   muTimeStampOfLastRoadRulesDownload;                // +96
    f32   mfDistanceDrivenOnline;                            // +100
    f32   mfDistanceDrivenOffline;                           // +104
    f32   mfInCarTimePlayed;                                 // +108
    s8    mi8CurrentProgressionRank;                         // +112 (Construct inits -2)
    s8    mi8PowerParkingBestRating;                         // +113
    s8    mi8PowerParkingBetweenOtherPlayersBestRating;      // +114
    u32   muBestNewBurnoutChainScore;                        // +116
    s32   maGameModeTypeAmount[17];                          // +120
    s32   maGameModeTypeAmountDiscovered[17];                // +188
    s32   maGameModeTypeAmountCompleted[17];                 // +256
    s32   maGameModeTypeAmountCompletedSinceTheStart[17];    // +324
    s32   miTotalTakedownCount;                              // +392
    s32   miTotalOnlineVerticleTakedownCount;                // +396
    s32   maiTakedownTypeCounts[17];                         // +400  (X360: [17], not DWARF [13])
    s32   maiWinsPerOfflineGameMode[10];                     // +468
    s32   maiRankWinsPerOfflineGameMode[10];                 // +508
    s32   maiLossesPerOfflineGameMode[10];                   // +548
    s32   miCompletedBarrelRolls;                            // +588
    f32   mfCompletedAirSpinAngle;                           // +592
    f32   mfCompletedHandbreakTurnAngle;                     // +596
    f32   mfCompletedDriftDistance;                          // +600
    f32   mfOncomingDistance;                                // +604
    f32   mfAirMaximum;                                      // +608
    s32   miHighestShowTimeScore;                            // +612
    s32   miBestStuntRunScore;                               // +616
    s32   miCarCount;                                        // +0x26C (620)
    s32   miLiveryDataCount;                                 // +0x270 (624)
    s32   miRivalCount;                                      // +0x274 (628)
    s32   miEventCount;                                      // +0x278 (632)
    CarData      maCars[512];                                // +0x280 (640)
    LiveryData   maLiveryChoices[512];                       // +0x3280 (12928)
    RivalData    maRivals[64];                               // +0x6280 (25216)
    ProfileEvent maEvents[175];                              // +0x7080 (28800)
    Set<CgsID, 512u> maStuntElements[3];                     // +30200
    u32   muMedalCountFromTheStart;                          // +42512
    bool  mbGoldCarsUnlocked;                                // +42516
    bool  mbSilverCarsUnlocked;                              // +42517
    Set<CgsID, 5u>  mJunkYardsDriveThruSet;                  // +42520 (len word +42560)
    Set<CgsID, 11u> mBodyShopsDriveThruSet;                  // +42568 (len word +42656)
    Set<CgsID, 5u>  mPaintShopsDriveThruSet;                 // +42664 (len word +42704)
    Set<CgsID, 14u> mGasStationsDriveThruSet;                // +42712 (len word +42824)
    Set<CgsID, 11u> mCarParksDriveThruSet;                   // +42832 (len word +42920)
    Array<CgsID, 2000u> maFreeBurnChallengeData;             // +42928
    CgsContainers::BitArray<300000u>   mabHitPropBitArray;   // +58936 (37504 bytes)
    s16   maaiStuntCountsByCounty[3][5];                     // +96440
    BrnStreetData::ChallengeHighScoreEntry   maNetworkChallengeData[64];  // +96472 (56B each)
    BrnStreetData::ChallengePlayerScoreEntry maChallengeData[64];         // +100056 (40B each)
    u32   muLastRoadRulesResetTime;                          // +102616
    CgsNetwork::NetworkTexture mPlayerLicencePicture;        // +102620 (28 bytes)
    char  macPlayerLicenceTextureData[9600];                 // +102648
    bool  mbPlayerLicencePictureIsValid;                     // +112248
    Array<MugshotInfo, 20u> maaMugshotInfo[5];               // +112256 (1128B each)
    CgsContainers::BitArray<30u> maAvailableMugshotFileIDs[5];// +117896 (8B each)
    f32   mafCarTypes[3];                                    // +117936
    s32   meCurrentCarType;                                  // +117948  (logical: BrnResource::ECarType; not yet homed)
    CgsContainers::BitArray<256u> maHasPlayerSeenTraining;   // +117952 (32 bytes)
    s32   miNumOnlineRacesDone;                              // +117984
    s32   miNumOnlineRacesWon;                               // +117988
    s32   miNumMugshotsSent;                                 // +117992
    CgsSystem::DateAndTime mDateLicenceIssued;               // +117996 (12 bytes)
    CgsSystem::DateAndTime mDate100PercentCompleted;         // +118008 (12 bytes)
    s32   miHighestNumberOfTakeDownsInRoadRage;              // +118020
    // FLAG: +118024..+118031 reserved. The trophy/achievement bit arrays (DWARF lists them
    // here) live in the X360 late tail instead (Construct stores them at +120032 / +120824);
    // this 8-byte gap is bridged rather than placed at a fabricated offset.
    u8    mPad_PostHighRoadRage[8];                          // +118024
    bool  mb100PercentCompletionSequenceShown;               // +118032
    bool  mbIsNewProfile;                                    // +118033 (Construct inits true)
    bool  mbCreditsSequenceViewed;                           // +118034
    bool  mbOneHundredHudMessageViewed;                      // +118035
    bool  mbHasUnlockedCredits;                              // +118036
    bool  mbHaveSet100PercentCompletedDate;                  // +118037
    bool  mbHaveSeenEliteCompletionSequence;                 // +118038
    bool  mbRedundantBool4;                                  // +118039
    s8    miPad1;                                            // +118040
    s16   miPad2;                                            // +118042
    u32   muRoadRulesIDLowBits;                              // +118044
    CgsContainers::BitArray<6u> mSeenCompleteAllEventTypeArray; // +118048 (8 bytes)
    f32   mfRealTimePlayed;                                  // +118056
    f32   mfRedundantFloat4;                                 // +118060
    u32   muRoadRulesIDHighBits;                             // +118064
    s16   miPad3;                                            // +118068
    s8    miPad4;                                            // +118070
    // FLAG: X360 tail beyond miPad4 (the PS3 DWARF member list stops here). On X360 the trophy/
    // achievement bit arrays + remaining tail fields live in this region; Construct's late stores
    // (at X360 +120032 / +120824 / +120832, all zero) fall inside it. Reserved as a fixed block;
    // this TU's functions reach it only through the wholesale zero in Construct.
    //
    // NOTE: this struct is NOT byte-size-asserted. The X360 member offsets quoted throughout are
    // the 32-bit-pointer ABI offsets (proven from the XEX); the PC reconstruction compiles 64-bit,
    // so any embedded pointer-bearing member (e.g. CgsNetwork::NetworkTexture, the Array<>/Set<>
    // count words, BitArray storage) is naturally wider here. Every function reaches its members BY
    // NAME, so behaviour is identical regardless of the exact byte offset on the PC target.
    u8    mPad_LateTail[2769];                               // X360: +118071 .. +120839 (reserve)
};

} // namespace BrnProgression

#endif // BRN_PROFILE_H
