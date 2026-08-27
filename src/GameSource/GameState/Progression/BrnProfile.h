#ifndef BRN_PROFILE_H
#define BRN_PROFILE_H

#include <cstddef>  // offsetof (Profile::_AssertLayout)
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
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"   // CgsContainers::FastBitArray<15> (developer-challenge bits)
#include "GameSource/GameState/BrnGameStateTypes.h"              // BrnGameState::StuntElementType (stunt-element tallies)
#include "GameSource/GameState/BrnTakedownType.h"                // BrnGameState::ETakedownType (AddTakedown)
#include "GameSource/GameState/BrnGameStateSharedIO.h"           // GameStateModuleIO::EGameModeType (game-mode tallies)
#include "SharedClasses/World/BrnWorldRegion.h"                  // BrnWorld::ECounty (per-county stunt tallies)
#include "GameSource/GameState/SharedIO/BrnTargetEventScore.h"   // GameStateModuleIO::TargetEventScore (maTargetEventScores element)
#include "GameSource/Network/Managers/BrnEventScoresManager.h"   // BrnNetwork::LocalEventScoreUploadData (maEventScoresToUpload element)
#include "BrnProgressionCarData.h"        // BrnProgression::CarData (maCars element, full layout)
#include "BrnProgressionLiveryData.h"     // BrnProgression::LiveryData (maLiveryChoices element)
#include "BrnProgressionRivalData.h"      // BrnProgression::RivalData (maRivals element)
#include "GameSource/Gui/SaveLoad/BrnGuiSaveLoadProfileRecords.h"  // BrnGuiSaveLoad::{Car,Livery,Rival,ProfileEvent}Data (SplitArray save-image twins)

// The two serialised save images Profile::Serialise / ::Deserialise codec between.
// FORWARD-DECLARED (documented exception (b)): the use here is pointer-only, and this
// header is included by most of the Progression/Gui tree -- pulling the GameSource/Gui
// SaveLoad headers in from it would cascade the whole save-image surface into every
// Profile consumer. The codec TU (BrnProfile_SaveImage.cpp) includes them for real.
namespace BrnGuiSaveLoad { class Profile; class ProfileDLC1; }

namespace BrnProgression
{

// ------------------------------------------------------------------------------------
// ProfileUpgradeTable -- the fixed-up PROFILEUPG resource body.
//
// The "profile upgrade" resource (BrnProgression::ProfileUpgradeResourceType, whose
// GetSerialisedResourceDescriptor sizes the block at (count + 1) * 8 and whose FixUp
// rebases the self-relative table offset at +4) is a prop-hit bit-index TRANSLATION table:
// when a content patch renumbers the world's props, it maps each moved bit between the
// index the SAVE IMAGE uses and the index THIS BUILD's live profile uses.
// Profile::Serialise / ::Deserialise are its only consumers -- ProfileManager::
// ReadProfileData @0x824FF298 and ::ReportTaskCompleted @0x82513EC0 pass it straight
// through (NULL unless the DLC entitlement gate passes) -- so it is homed here.
//
// X360-attested shape: the head is read as `lwz 0(r24)` / `lwz 4(r24)` (count, table) and
// each record as `lwz 0(r11)` / `lwz 4(r11)` at an 8-byte stride. The head's pointer slot
// widens on x64; only the RECORDS are a serialised layout, and they are pointer-free.
// ------------------------------------------------------------------------------------
struct ProfileUpgradeTable
{
    struct Entry
    {
        u32 muImageBitIndex;   // +0x00 the bit's index in the SAVE IMAGE's prop-hit array
        u32 muLiveBitIndex;    // +0x04 the bit's index in the LIVE profile's prop-hit array
    };

    s32          miCount;      // +0x00 number of Entry records
    const Entry* mpaEntries;   // +0x04 on console (self-relative until ProfileUpgradeResourceType::FixUp)
};

// ------------------------------------------------------------------------------------
// SplitArray<TSrc, TDst>  -- Profile::Serialise's save-image splitter.
//
// Walk liCount live progression records (lpSrc) and copy each into either the "base game"
// run (lpBase / *lpiBaseCount) or the "DLC" run (lpDlc / *lpiDlcCount), preserving order
// within each run. A record is classified as DLC by BrnGuiSaveLoad::ProfileDLC1::IsDLCCarId
// (its packed id at +0) for the id-keyed records, or by its id threshold for events; the DLC
// run is capped at liMaxDlcCount (asserted "liDLCIndex < liMaxDlcCount", BrnProfile.cpp:56).
// Each of the four instantiations is a distinct X360 body (different stride/predicate), so
// each is provided as an explicit specialisation in BrnProfile.cpp; the primary template is
// intentionally left undefined.
// ------------------------------------------------------------------------------------
template<typename TSrc, typename TDst>
void SplitArray(s32 liCount, const TSrc* lpSrc,
                s32* lpiBaseCount, TDst* lpBase,
                s32* lpiDlcCount,  TDst* lpDlc, s32 liMaxDlcCount);

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

// The four explicit SplitArray specialisations (bodies in BrnProfile.cpp). Declared here so
// the save-image codec TU that calls them binds to the specialisation rather than implicitly
// instantiating the (deliberately undefined) primary template.
template<> void SplitArray<CarData, BrnGuiSaveLoad::CarData>(
        s32 liCount, const CarData* lpSrc,
        s32* lpiBaseCount, BrnGuiSaveLoad::CarData* lpBase,
        s32* lpiDlcCount,  BrnGuiSaveLoad::CarData* lpDlc, s32 liMaxDlcCount);
template<> void SplitArray<LiveryData, BrnGuiSaveLoad::LiveryData>(
        s32 liCount, const LiveryData* lpSrc,
        s32* lpiBaseCount, BrnGuiSaveLoad::LiveryData* lpBase,
        s32* lpiDlcCount,  BrnGuiSaveLoad::LiveryData* lpDlc, s32 liMaxDlcCount);
template<> void SplitArray<RivalData, BrnGuiSaveLoad::RivalData>(
        s32 liCount, const RivalData* lpSrc,
        s32* lpiBaseCount, BrnGuiSaveLoad::RivalData* lpBase,
        s32* lpiDlcCount,  BrnGuiSaveLoad::RivalData* lpDlc, s32 liMaxDlcCount);
template<> void SplitArray<ProfileEvent, BrnGuiSaveLoad::ProfileEvent>(
        s32 liCount, const ProfileEvent* lpSrc,
        s32* lpiBaseCount, BrnGuiSaveLoad::ProfileEvent* lpBase,
        s32* lpiDlcCount,  BrnGuiSaveLoad::ProfileEvent* lpDlc, s32 liMaxDlcCount);

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

    // ADDITIVE GROW (declare-only) for the BrnGameStateImageManagerBase TU. That manager builds the
    // gallery "image info" GUI event (X360 type-290 record) field-for-field out of a MugshotInfo:
    // the X360 reads a 16-byte unique-id block (+0x00), three image words (+0x18/+0x1C/+0x20), an
    // 8-byte date (+0x24), one word (+0x2C), a 4-byte file id (+0x30) and a 1-byte locked flag (+0x32).
    // The internal member layout/semantics of MugshotInfo are NOT yet reverse-engineered, so rather
    // than fabricate member names the touched sub-blocks are exposed as named, declared-only getters
    // (offsets/widths X360-attested); their bodies + the real members land with MugshotInfo's own TU.
    // Returned by const-ref/value so the ImageManager accesses every field BY NAME (no raw offsets).
    // 16-byte unique-player-id block (the X360 only ever moves it as 16 opaque bytes -- two qwords).
    struct UniquePlayerIDImage { u64 mu64Lo; u64 mu64Hi; };
    UniquePlayerIDImage GetUniquePlayerID() const;  // +0x00 (16 bytes)
    u64  GetGamerCardXuid() const;                   // +0x10 (8-byte gamercard XUID posted by ProcessShowGamerCardRequest)
    s32  GetImageWord0() const;                      // +0x18
    s32  GetImageWord1() const;                      // +0x1C
    s32  GetImageWord2() const;                      // +0x20
    u64  GetDateTaken() const;                       // +0x24 (8-byte date image)
    s32  GetImageWord2C() const;                     // +0x2C
    s32  GetFileID() const;                           // +0x30
    u8   GetLockedFlag() const;                       // +0x32

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
// DWARF-vs-X360 deltas resolved from the binary (PS3 DWARF drift; the shipped X360 build is
// DLC-era and grew the layout):
//   * The FOUR game-mode tally arrays are int32[18] on X360 (DWARF says [17]) -- the DLC
//     island mode adds an 18th slot. PROVEN: AddGameModeTypeToDiscovered @0x82354AA0 and
//     GetGameModeTypeDiscovered @0x8240E940 hit base +192 (== 120 + 18*4), AddGameModeType-
//     Completed @0x82354B10 hits bases +264 / +336, AddTakedown @0x82354C00 pins
//     miTotalTakedownCount at +408 and maiTakedownTypeCounts at +416, and Deserialise
//     @0x8237D308 restores each array's element 17 (+188/+260/+332/+404) from the DLC blob.
//   * maiTakedownTypeCounts stays the DWARF int32[13] (Deserialise memcpy's exactly 52 bytes
//     into +416). Net size of the block is unchanged, so maiWinsPerOfflineGameMode still
//     lands at +468 and miCompletedBarrelRolls at +588 (DEBUG_ClearMedals' win-array loop
//     @0x1FC/-0x28 and SetBestStuntStats' +588 write).
//   * maNetworkChallengeData[64] (ChallengeHighScoreEntry, 56B each) + maChallengeData[64]
//     (ChallengePlayerScoreEntry, 40B each) sizes land mPlayerLicencePicture at +102620 exactly.
//   * mSeenTrophyAwardBitArray (DWARF BrnProfile.h:1279, BitArray<35u>) sits at +118024 on
//     X360 too (Get/SetSeenTrophyUnlockSequence @0x82475A30/@0x824BAA30 bit-op the qword at
//     8*(idx>>6 + 14753) == +118024). The DWARF's FOLLOWING member mAchievementsEarnt
//     (BitArray<60u>) is NOT at +118032 on X360 (the completion booleans live there); its
//     X360 home is unrecovered and it is deliberately NOT placed here.
//   * The X360 DLC-era tail (absent from the PS3 DWARF member list, which stops at miPad4):
//     maTargetEventScores Array<TargetEventScore,49> @+118072 (count word +120032, proven by
//     Get/Set/RemoveTargetEventScore), maEventScoresToUpload Array<LocalEventScoreUpload-
//     Data,49> @+120040 (count word +120824, proven by Set/RemoveEventScoreToUpload) and
//     mDeveloperChallengesCompleted FastBitArray<15> @+120832 (SetDeveloperChallengeComplete
//     @0x823621F0). sizeof(Profile) == 120840.
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

    // ------------------------------------------------------------------------
    // The save-image codec (bodies in BrnProfile_SaveImage.cpp -- a per-function split of
    // this class's BrnProfile.cpp, the console's own home for both).
    //
    //   Serialise   @0x8237C1F0 -- live profile -> the two console save images.
    //   Deserialise @0x8237D308 -- the exact inverse.
    //
    // Argument order/roles are the X360 asm's (r3 = this, r4 = the progression image,
    // r5 = the optional PROFILEUPG table, r6 = the DLC1 image); ProfileManager::
    // ReadProfileData @0x824FF298 and ::ReportTaskCompleted @0x82513EC0 are the callers.
    // ------------------------------------------------------------------------
    void Serialise(BrnGuiSaveLoad::Profile* lpImage,
                   const ProfileUpgradeTable* lpUpgrade,
                   BrnGuiSaveLoad::ProfileDLC1* lpImageDLC1);
    void Deserialise(const BrnGuiSaveLoad::Profile* lpImage,
                     const ProfileUpgradeTable* lpUpgrade,
                     const BrnGuiSaveLoad::ProfileDLC1* lpImageDLC1);

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

    // ADDITIVE GROW (declare-only) for the BrnGameStateImageManagerBase TU. The image-gallery
    // manager queries / mutates the persisted mugshot records through these (X360-asm-attested):
    //   GetMugshotInfo (Profile +112256 region): the leMugshotType'th gallery's liImageIndex'th
    //     mugshot record, or NULL. leMugshotType is a GameStateModuleIO::EImageGalleryType (the
    //     X360 validates < 5); taken as the s32 the X360 compares against 5.
    //   DeleteMugshot: removes the liImageIndex'th mugshot of that gallery type (returns >=0 on success).
    //   LockOrUnlockMugshot: toggles that mugshot's locked-for-deletion flag (returns true if found).
    // Bodies + the real members land with the Progression mugshot TU.
    MugshotInfo* GetMugshotInfo(s32 leMugshotType, s32 liImageIndex);
    s32          DeleteMugshot(s32 leMugshotType, s32 liImageIndex);
    bool         LockOrUnlockMugshot(s32 leMugshotType, s32 liImageIndex);

    void SetPlayerLicencePicture(const CgsNetwork::NetworkTexture* lpNewPlayerImage);
    CgsSystem::DateAndTime GetLicenceIssuedDate() const;
    void SetLicenceIssuedDateAsNow();
    CgsSystem::DateAndTime Get100PercentCompletedDate() const;

    bool GetSeen100PercentCompletionSequence() const;
    void SetSeen100PercentCompletionSequence();
    bool GetSeenEliteCompletionSequence() const;
    void SetSeenEliteCompletionSequence();

    // ADDITIVE GROW (BrnGuiHudMessageAnalyzer::Update): the "100%-complete HUD message
    // seen" flag pair, DWARF-attested shapes (BrnProfile.h:556 / :560). The X360 image
    // has no standalone symbols for either (no ledger rows; Update @0x82525FC0 inlines
    // the +118035 lbz/stb at both callsites), so they are defined inline here.
    bool GetOneHundredHudMessageViewed() const   { return mbOneHundredHudMessageViewed; }
    void SetOneHundredHudMessageViewed(bool lbViewed) { mbOneHundredHudMessageViewed = lbViewed; }

    // ADDITIVE GROW: the "this profile has never played" flag pair, DWARF-attested shapes
    // (BrnProfile.h:1129 SetIsNewProfile / :1132 GetIsNewProfile). Construct seeds it true;
    // it gates the first-boot intro (licence / photo-booth) sequence:
    //   BrnGui::InGame::Update  @0x824E0ED0 -- `lbz +118033` -> "TO_INTRO" + command 476
    //   BrnGui::Intro::Update   @0x824DF0B0 -- case WAIT_FOR_FLYBY_FINISH picks
    //                                          "ADVANCE" vs "GO_BACK" on it
    //   BrnGui::Intro::OnLeave  @0x824D1640 -- `stb 0, +118033` (clears it)
    // The X360 image has no standalone symbol for either accessor (every callsite inlines
    // the +118033 lbz/stb), so they are defined inline here -- same as the pair above.
    bool GetIsNewProfile() const               { return mbIsNewProfile; }
    void SetIsNewProfile(bool lbIsNewProfile)  { mbIsNewProfile = lbIsNewProfile; }

    // ADDITIVE GROW: the cached "type of car the player is currently in" word at +117948
    // (meCurrentCarType, logical BrnResource::ECarType). The X360 emits no accessor symbol --
    // GameStateModule::OnSpecialEventPlayerCarChange @0x8238FB40 open-codes the store as
    // `stwx r29, profile, 0x1CCBC` after reading the VehicleListEntry's car-type byte (+0xE8).
    // Defined inline here, same precedent as the GetIsNewProfile pair above.
    s32  GetCurrentCarType() const             { return meCurrentCarType; }
    void SetCurrentCarType(s32 leCarType)      { meCurrentCarType = leCarType; }

    // ADDITIVE GROW (BrnGui::PreRaceFlyByState::Set*Description): the progression-rank byte
    // at +112 (mi8CurrentProgressionRank, Construct seeds it -2). DWARF BrnProfile.h:553
    // gives the shape (`int8_t GetCurrentProgressionRank() const`). No standalone X360
    // symbol -- every callsite inlines the byte load `lbz r10, 0x70(profile)` and re-stores
    // it as a byte into the description record (0x824C6FCC / 0x824C762C / 0x824C7AC4) -- so
    // it is defined inline here, same precedent as the pairs above.
    s8   GetCurrentProgressionRank() const     { return mi8CurrentProgressionRank; }

    // ADDITIVE GROW: the persisted "car the player is in / where they left it" pair at +80/+88
    // (mSpawnCarId / mSpawnWheelId). The X360 emits no accessor symbols -- ProgressionManager::
    // OnPlayerCarChange @0x8237AC38 open-codes the two stores as `std r29, 0x1C0(progMgr)` /
    // `std r5, 0x1C8(progMgr)` (i.e. Profile+80/+88), and GameStateModule::OnProfileLoaded
    // @0x82397310 reads them back as `ld 0x50(profile)` / `ld 0x58(profile)` with the
    // "mSpawnCarId != 0" assert between. Defined inline, same precedent as the pairs above.
    CgsID GetSpawnCarId() const                { return mSpawnCarId; }
    void  SetSpawnCarId(CgsID lCarId)          { mSpawnCarId = lCarId; }
    CgsID GetSpawnWheelId() const              { return mSpawnWheelId; }
    void  SetSpawnWheelId(CgsID lWheelId)      { mSpawnWheelId = lWheelId; }

    // ⭐⭐ THE PAIR WAS SWAPPED (fixed 2026-08-27). The two derived-car unlock flags at
    // +42516/+42517 were named in DWARF DECLARATION ORDER (mbGoldCarsUnlocked first), and the
    // ARTIST asm says that order is wrong. ProgressionManager::CheckForSpecialCarUnlocks
    // @0x82396058 prints each flag next to its own accessor name, and the load is right there:
    //     ori  r28, r10, 0xA784      ; 42884 == Profile+42516
    //     lbzx r29, r31, r28         ; @0x823960A4
    //     addi r4, r11, aMprofileAresil@l   ; "mProfile.AreSilverCarsUnlocked(): "
    // ...and +42885 (Profile+42517) is the one printed "mProfile.AreGoldCarsUnlocked() : ".
    // The GATES corroborate it independently, and match the shipped game:
    //     +42516  <- GetCurrentProgressionRank() >= ProgressionData+0x14   (a licence rank) = SILVER
    //     +42517  <- ComputeCompletionPercentage() >= 100.0f               (game complete)  = GOLD
    // Rung 1 arbitrates over rung 2; DecFIGS additionally declares the accessor pair as
    // AreChromeCarsUnlocked/AreGoldCarsUnlocked (BrnProfile.h:757/:760), i.e. by ARTIST the
    // first flag's accessor had been renamed Chrome -> Silver, which is how the member names
    // and the accessor names drifted apart in the first place.
    // ⚠️ NOTHING MOVES: this is a rename only. Both bytes keep their offsets, and
    // BrnProfile_SaveImage.cpp stores them at image+42500/+42501 in the same offset order, so
    // the save layout is byte-identical before and after (the two KU_ names are renamed with
    // them so the pairing stays honest).
    // The X360 tests the SILVER one inline in ProgressionManager::AddCar @0x8237A970
    // (`lwz *(progMgr + 42884)`, i.e. Profile+42516) to decide whether to fan a new car out to
    // its derived variants -- which is why that leg's own comment already called itself the
    // "derived-(silver)-car fan-out" while testing a member spelled Gold.
    bool GetSilverCarsUnlocked() const         { return mbSilverCarsUnlocked; }
    bool GetGoldCarsUnlocked() const           { return mbGoldCarsUnlocked; }

    // ADDITIVE GROW: the two profile reads BrnGui::LicenseComponent inlines. Neither has a
    // standalone symbol in the X360 image -- every call site open-codes the load pair -- so
    // both are defined inline here, exactly as the GetIsNewProfile pair above.
    //   GetPlayerLicencePicture: `mbPlayerLicencePictureIsValid ? &mPlayerLicencePicture : NULL`
    //     (the lbz +112248 / addi +102620 pair in LicenseComponent::OnLoad @0x82440AC0,
    //      ShowLicense @0x82440C98, SetVisible @0x82440E38, SendPlayerPictureEvent @0x8243CB90).
    //   GetHaveSet100PercentCompletedDate: the +118037 byte LicenseComponent::SetPlayerInfo
    //     @0x8243C380 gates the completion-date fields on.
    const CgsNetwork::NetworkTexture* GetPlayerLicencePicture() const
    { return mbPlayerLicencePictureIsValid ? &mPlayerLicencePicture : 0; }
    bool GetHaveSet100PercentCompletedDate() const { return mbHaveSet100PercentCompletedDate; }

    // ---- [drive-thru wave 2026-08-27] the three setters the completion chain needs.
    // The X360 emits no standalone symbol for any of them -- ProgressionManager::
    // SendGameCompletionResults @0x82395C28 and ::CheckForSpecialCarUnlocks @0x82396058 poke
    // the bytes directly through the manager (manager+118405, manager+42884/+42885) while
    // inlining -- so they are defined inline here, the same precedent as the getters above.
    //   Set100PercentCompletedDateAsNow: `CgsSystem::DateAndTime::Update(Profile + 118008)`
    //     followed by `stbx 1 -> Profile + 118037` (0x82395C9C / 0x82395CA8) -- ONE operation
    //     in the console, so it is one method here rather than two that can be called apart.
    void Set100PercentCompletedDateAsNow()
    {
        mDate100PercentCompleted.Update();
        mbHaveSet100PercentCompletedDate = true;
    }
    void SetSilverCarsUnlocked(bool lbUnlocked) { mbSilverCarsUnlocked = lbUnlocked; }  // +42516
    void SetGoldCarsUnlocked(bool lbUnlocked)   { mbGoldCarsUnlocked   = lbUnlocked; }  // +42517

    void DEBUG_ClearMedals();

    // ------------------------------------------------------------------------
    // Tally / query / persistence-tail functions reconstructed in this TU (BrnProfile.cpp);
    // method shapes (param enum types, constness, returns) follow the DecFIGS DWARF
    // (BrnProfile.h:709..1171) where attested, the X360 asm otherwise.
    // ------------------------------------------------------------------------

    // Drive-thru discovery. AddDriveThru returns the TrophyUnlockData::UnlockType awarded when
    // a category completes (0 == E_UNLOCKTYPE_NONE); the DWARF return type is
    // BrnProgression::TrophyUnlockData::UnlockType, whose full enum home is not yet
    // reconstructed (only NONE=0/COUNT=35 are committed), so the s32 underlying value is used.
    s32  AddDriveThru(CgsID lId, BrnTrigger::GenericRegion::Type leType);           // 0x82374DB8
    bool AreAllDriveThrusCompleted();                                               // 0x82361870 (DWARF: non-const)
    s32  GetDriveThrusFound() const;                                                // 0x82361778 (car parks excluded!)

    // Game-mode tallies (indexed by GsmIO::EGameModeType; the X360 [18] arrays below).
    // ⭐ [D1 profile-event-list wave, 2026-08-27] AddGameModeTypeToTotals is DWARF-attested
    // (BrnProfile.h:751 `void AddGameModeTypeToTotals(EGameModeType)`) and has no standalone X360
    // symbol -- ProgressionManager::AddEventTypeToEventTotals @0x82366628 INLINES it as
    // `++*(4*(mode+30) + profile)`, i.e. `++maGameModeTypeAmount[mode]`, immediately after firing
    // the range assert whose baked location is BrnProfile.h:2047 (the same "lEGameModeType >
    // GsmIO::E_MODE_NONE" string its three siblings below carry). It is the "how many events of
    // this type exist at all" tally -- the denominator of the HUD's "Races 3/12".
    void AddGameModeTypeToTotals(BrnGameState::GameStateModuleIO::EGameModeType lEGameModeType);  // inlined @0x82366628
    void AddGameModeTypeCompleted(BrnGameState::GameStateModuleIO::EGameModeType lEGameModeType);    // 0x82354B10
    void AddGameModeTypeToDiscovered(BrnGameState::GameStateModuleIO::EGameModeType lEGameModeType); // 0x82354AA0
    s32  GetGameModeTypeAmount(BrnGameState::GameStateModuleIO::EGameModeType lEGameModeType) const;     // 0x82354A38
    s32  GetGameModeTypeDiscovered(BrnGameState::GameStateModuleIO::EGameModeType lEGameModeType) const; // 0x8240E940

    // ---- [drive-thru wave 2026-08-27] the four tallies ProgressionManager::OnTrophyUnlock
    // @0x82389740 reads. The X360 emits NO standalone symbol for any of them -- that function
    // reaches each one as a raw interior offset off the MANAGER while inlining the accessor
    // (manager+776/+780/+482, i.e. Profile+408/+412/+114, and Profile+304/+308/+316 for the
    // completed tallies) -- so they are defined inline here, the same precedent as
    // GetNumWinsForGameMode / GetIsNewProfile above. Named-member access replaces the console's
    // offset arithmetic; nothing is widened or moved.
    s32  GetTotalTakedownCount() const                  { return miTotalTakedownCount; }
    s32  GetTotalOnlineVerticleTakedownCount() const    { return miTotalOnlineVerticleTakedownCount; }  // sic -- DWARF spelling
    // Profile+114. OnTrophyUnlock loads it with `lbz` + `extsb` (a SIGNED byte) for the
    // E_UNLOCKTYPE_NUM_PERCENTAGE_PARALLELPARK_ONLINE trophy.
    s8   GetPowerParkingBetweenOtherPlayersBestRating() const { return mi8PowerParkingBetweenOtherPlayersBestRating; }
    s32  GetGameModeTypeCompleted(BrnGameState::GameStateModuleIO::EGameModeType lEGameModeType) const
    {
        return maGameModeTypeAmountCompleted[lEGameModeType];
    }

    // Offline win/loss tallies.
    void AddWinForGameMode(BrnGameState::GameStateModuleIO::EGameModeType leGameModeType);   // 0x82354C80
    void AddLossForGameMode(BrnGameState::GameStateModuleIO::EGameModeType leGameModeType);  // 0x8230FB20
    s32  GetNumRankWinsForGameMode(BrnGameState::GameStateModuleIO::EGameModeType leGameModeType) const; // 0x8230FA40
    s32  GetNumLossesForGameMode(BrnGameState::GameStateModuleIO::EGameModeType leGameModeType) const;   // 0x8230FAB0

    // ADDITIVE GROW (BrnGui::PreRaceFlyByState::Set*Description). The PLAIN offline win
    // tally -- maiWinsPerOfflineGameMode @+468 (0x1D4), NOT the rank-only array at +508.
    // Which array is which is settled by AddWinForGameMode @0x82354C80, which increments
    // BOTH `4*mode + 0x1D4` and `4*mode + 0x1FC`; the out-of-line GetNumRankWinsForGameMode
    // @0x8230FA40 reads `4*mode + 0x1FC` (+508), so +468 is the one this accessor reads.
    // Shape is the DWARF's (BrnProfile.h:574 -- int32_t, const, EGameModeType param).
    // The X360 image has NO standalone symbol for it (no `Profile::GetNumWinsForGameMode`
    // in identity.json): every caller inlines the single indexed load --
    //   0x824C6FC8 `lwz r11, 0x1D4(profile)` (SetRaceDescription,          mode 0)
    //   0x824C7AC0 `lwz r11, 0x1E8(profile)` (SetBurningRouteDescription,  mode 5)
    //   0x824C7628 `lwz r11, 0x1F4(profile)` (SetMarkedManDescription,     mode 8)
    // so it is defined inline here, the same precedent as the accessor pairs above.
    // FLAG: the three out-of-line siblings (rank-wins/losses/AddWin) each open with a
    // `leGameModeType < GsmIO::E_MODE_OFFLINE_*` range assert; this one has no symbol, so
    // its assert text/line is unrecoverable and is deliberately NOT fabricated here (and
    // this header pulls in no assert facility). Every attested callsite passes a constant.
    s32  GetNumWinsForGameMode(BrnGameState::GameStateModuleIO::EGameModeType leGameModeType) const
    { return maiWinsPerOfflineGameMode[leGameModeType]; }

    // Takedown tallies.
    void AddTakedown(BrnGameState::ETakedownType leTakedownType);                   // 0x82354C00

    // Stunt-element sets + per-county tallies.
    void AddStuntElement(BrnGameState::StuntElementType leStuntElementType, CgsID lId,
                         BrnWorld::ECounty leCounty);                               // 0x8236AB00
    bool IsStuntElementDone(BrnGameState::StuntElementType leStuntElementType, CgsID lId) const; // 0x823619B0
    s32  GetStuntElementCount(BrnGameState::StuntElementType leStuntElementType) const;          // 0x82361950
    s32  GetStuntElementCountByCounty(BrnGameState::StuntElementType leStuntElementType,
                                      BrnWorld::ECounty leCounty) const;            // 0x82354D10

    // Event records / medals.
    const ProfileEvent* GetEvent(u32 luIndex) const;                                // 0x82354DA0

    // ⭐ [D1 profile-event-list wave, 2026-08-27] THE id->record lookup, DWARF-attested as a
    // const/non-const overload pair (BrnProfile.h:709 / :713 -- `const ProfileEvent* FindEvent
    // (uint32_t) const` / `ProfileEvent* FindEvent(uint32_t)`), matching the FindCar / FindRival
    // pair above. No standalone X360 symbol: every console caller inlines the same open-coded
    // scan (`lwz r9, 0x278(profile)` as the bound, `addi r10, profile, 0x7080` as the base, 8-byte
    // stride, `cmplw` against muEventID @+0) -- ProgressionManager::UnlockToProgressionRank
    // @0x8239DEDC, GameStateModule::CheckIfPlayerIsAtJunctionWithAnEvent @0x823906B0,
    // ProgressionManager::OnEventFinishUpdateProfile @0x823A0084 and
    // HasEventBeenWonPreviously @0x82366B30 are four copies of it. NULL when the id is unknown
    // (the console's own "not found" answer -- its callers then fire their own assert).
    const ProfileEvent* FindEvent(u32 luEventID) const;                             // inlined (see banner)
          ProfileEvent* FindEvent(u32 luEventID);                                   // inlined (see banner)

    // ADDITIVE GROW (BrnGui::PreRaceFlyByState::Set*Description). The live length of maEvents.
    // DWARF BrnProfile.h:833 gives the shape -- `uint32_t GetEventCount() const` -- while the
    // backing member miEventCount @+0x278 is a signed word (already committed below).
    // No standalone X360 symbol: every caller inlines `lwz r11, 0x278(profile)` as the bound of
    // the maEvents walk (0x824C6FDC / 0x824C763C / 0x824C7AD4), so it is defined inline here.
    // FLAG: those callsites compare the value SIGNED -- `cmpwi r11, 0 / ble` for the zero-trip
    // guard and `cmpw r7, r11 / blt` for the loop bound -- i.e. the original callers took the
    // count into an s32 before comparing. The DWARF return type is kept (it is the declaration
    // authority); reconstructed callers cast back to s32 at the comparison to match the asm.
    u32  GetEventCount() const                        { return static_cast<u32>(miEventCount); }

    s32  GetMedalAchievedForEventWithID(s32 liEventID) const;                       // 0x82354EB0 (0 gold / 1 silver / 2 bronze / -1 none)
    u32  GetTotalWinCount(u32& lruRankWins, u32& lruNonRankWins,
                          u32& lruSpecialEventWins) const;                          // 0x82354E10
    s32  GetTotalCarsToShutDown() const;                                            // 0x823549D0 (rivals in E_STATE_UNLOCKED)

    // Prop-hit bit array.
    void RecordPropHit(s32 liZoneIndex, s32 liPropIndex);                           // 0x82361C48

    // "Seen all events of a mode won" HUD-message bits (leModeType is the not-yet-homed
    // BrnProgression::RaceEventData::EModeType; taken as the s32 the X360 compares against 6).
    bool GetSeenAllEventTypeWonMessage(s32 leModeType) const;                       // 0x82361F58
    void SetSeenAllEventTypeWonMessage(s32 leModeType);                             // 0x823620A8

    // Trophy-unlock-sequence-seen bits (leUnlockType is BrnProgression::TrophyUnlockData::
    // UnlockType per the DWARF; taken as the s32 the X360 compares against 35).
    bool GetSeenTrophyUnlockSequence(s32 leUnlockType) const;                       // 0x82475A30
    void SetSeenTrophyUnlockSequence(s32 leUnlockType);                             // 0x824BAA30

    // Target-event-score records (Burning Route / Stunt Attack targets; X360 DLC-era tail).
    BrnGameState::GameStateModuleIO::TargetEventScore* GetTargetEvent(CgsID lEventId);          // 0x82371458
    void SetTargetEventScore(BrnGameState::GameStateModuleIO::TargetEventScore::OpaqueHead lHead,
                             CgsID lEventId, s32 liScore);                          // 0x823714F8
    void RemoveTargetEventScore(CgsID lEventId);                                    // 0x82371600

    // Pending online event-score uploads (X360 DLC-era tail).
    void SetEventScoreToUpload(CgsID lEventId, s32 liScore,
                               BrnGameState::GameStateModuleIO::EGameModeType leGameMode);      // 0x82371740
    void RemoveEventScoreToUpload(CgsID lEventId);                                  // 0x823718F0

    // ------------------------------------------------------------------------
    // ADDITIVE GROW (declare-only) -- bodies in other (not-yet-reconstructed) Progression TUs,
    // EXCEPT where marked: SetTrainingAlreadySeen / GetPlayerBaseDeformAmount / GetCarData /
    // SetDeveloperChallengeComplete / RepairUnlockedVehicle now have their X360 bodies in
    // BrnProfile.cpp. These were named by already-committed callers and are preserved here
    // verbatim. The full member layout above now backs every offset they reach.
    // ------------------------------------------------------------------------
    void ClearTrainingFlags();
    bool HasPlayerSeenTrainingType(ETrainingType leTrainingType) const;
    void SetTrainingAlreadySeen(ETrainingType leTrainingType);   // 0x82361B20 (body in BrnProfile.cpp)

    bool IsDriveThruDiscoverd(CgsID lId, BrnTrigger::GenericRegion::Type leType) const;
    s32  GetNumDriveThrusDiscovered(BrnTrigger::GenericRegion::Type leType) const;
    f32  GetPlayerBaseDeformAmount(CgsID lCarId) const;
    // ⛔ RETIRED 2026-08-27 (drive-thru link-closure wave): `void IncrementNumDiscoveredEvents()`.
    // Its RETIRE-WHEN is discharged -- DriveThruManager::UnlockCarChallengeForCar, its only
    // caller, now spells the store as AddGameModeTypeToDiscovered(E_MODE_BURNING_ROUTE), which
    // is the DWARF-attested owner of the word the console increments (ProgressionManager+0x244
    // == Profile+212 == 192 + 4*5). The alias's name claimed a generic "events discovered"
    // counter that does not exist; leaving an unreferenced wrong-named forward is how it gets
    // picked up again. Do not re-mint it.
    // ⚠️ [D1 profile-event-list wave, 2026-08-27] NAME NOTE -- this is the SAME scan as the
    // DWARF-attested Profile::FindEvent pair declared above (:709/:713), under an invented name
    // and a widened key, named by the not-yet-mounted BrnDriveThruManager TU. FindEvent is now
    // bodied; when DriveThruManager is mounted, retire this declaration in favour of it rather
    // than writing a second body. Same for ProfileEvent::IsFound / SetFound above, which are the
    // DWARF's EnableFlags / ClearFlags / IsFlagSet(E_FLAG_DISCOVERED) under invented names.
    ProfileEvent* FindProfileEventByRaceEventId(CgsID lEventId);

    s32             GetCarCount() const;
    const CarData*  GetCarData(s32 liIndex) const;
          CarData*  GetCarData(s32 liIndex);
    bool IsStartOfGameDeformActive() const;

    bool HasPlayerCompletedFreeburnChallenge(CgsID lChallengeID) const;
    u32  CompleteFreeburnChallenge(CgsID lChallengeID);

    bool IsDeveloperChallengeComplete(s32 liChallengeIndex) const;
    void SetDeveloperChallengeComplete(s32 liChallengeIndex);

    // ------------------------------------------------------------------------
    // ADDITIVE GROW (declare-only) for the BrnProgression::ProgressionManager TU.
    // These three Profile methods are the ones the X360 ProgressionManager wrappers call
    // through the embedded Profile sub-object (each wrapper asserts its arg, then the inlined
    // Profile body asserts again at BrnProfile.h:2958/2976 -- the duplicate assert visible in
    // the X360 ProgressionManager::SetRoadRule* disassembly proves the wrapper delegates here).
    // Bodies live with the Profile TU; declaration-only suffices for the ProgressionManager
    // `cl /c` gate.
    // ------------------------------------------------------------------------

    // X360: ProgressionManager::AreRoadRulesAvailable reads this medal-progress count (Profile +42512,
    // the X360 a1[10720] read) and compares it >= 4. Trivial named-member getter; body in the Profile TU.
    u32 GetMedalCountFromTheStart() const;

    // ADDITIVE GROW [stuntrace waveB / agent 10] -- the SETTER half of the pair above.
    // ProgressionManager::OnEventFinishUpdateProfile @0x823A0040 open-codes the increment
    // (`addis r11, profile, 1 / addi r11, r11, -0x59F0` == Profile+0xA610 == +42512, then
    // lwz / addi 1 / stw, asm 0x823A0450..0x823A0460) -- every event won for the first time
    // bumps this tally, and ProgressionManager::AreRoadRulesAvailable's `>= 4` test is its
    // reader. The X360 emits no standalone symbol for either half (both are header-inline),
    // so this is defined inline here, exactly as the GetIsNewProfile / GetNumWinsForGameMode
    // pairs above.
    void SetMedalCountFromTheStart(u32 luCount) { muMedalCountFromTheStart = luCount; }

    // ===========================================================================================
    // [stuntrace waveB fix round, 2026-08-26] THE TWO POST-EVENT HIGH-WATER MARKS. Both members
    // already sit below `private:` at their console-proven offsets (miBestStuntRunScore +616 ==
    // Profile+0x268, miHighestNumberOfTakeDownsInRoadRage +118020), and both are read-modify-write
    // "if (new > stored) stored = new" legs the wave had to DROP for want of an accessor:
    //   * ModeManager::ShowModeResults @0x823436D0 raises the best stunt-run score;
    //   * ModeManager::FinishCurrentMode @0x8234B978 raises the road-rage takedown record
    //     (offset re-derived: progMgr+0x170 + 0x20000 - 0x32FC == Profile+118020).
    // X360-INLINED on both halves (no standalone symbols), so inline bodies are the faithful form --
    // same precedent as GetIsNewProfile / GetNumWinsForGameMode / SetMedalCountFromTheStart above.
    // ===========================================================================================
    s32  GetBestStuntRunScore() const                        { return miBestStuntRunScore; }
    void SetBestStuntRunScore(s32 liScore)                   { miBestStuntRunScore = liScore; }
    s32  GetHighestNumberOfTakeDownsInRoadRage() const       { return miHighestNumberOfTakeDownsInRoadRage; }
    void SetHighestNumberOfTakeDownsInRoadRage(s32 liTakedowns) { miHighestNumberOfTakeDownsInRoadRage = liTakedowns; }

    // [tut-ticker] X360 raw read of Profile+108 (mfInCarTimePlayed): TrainingManager::Update /
    // RequestTraining / TriggerAnyFollowOnTrainingTips all `lfs` it for the timed-tip and
    // "seconds since last tip" gates. Trivial named-member getter; body in the Profile TU.
    // ⚠️ FLAG: nothing on this build ACCUMULATES the member yet (its console writer is the
    // un-reconstructed progression time tick), so it reads as the loaded/Construct value.
    f32 GetInCarTimePlayed() const;

    // X360: ProgressionManager::RepairUnlockedVehicle delegates to Profile::RepairUnlockedVehicle
    // (this+0x170, lCarId) -- clears the just-repaired car's stored deform/damage. Returns the
    // updated CarData record (the X360 hands a pointer back in r3).
    CarData* RepairUnlockedVehicle(CgsID lCarId);

    // X360: ProgressionManager::SetRoadRuleNetworkHighScores -> Profile::SetRoadRuleNetworkHighScores.
    // Wholesale 3584-byte copy of the 64-entry ChallengeHighScoreEntry table into maNetworkChallengeData.
    void SetRoadRuleNetworkHighScores(const BrnStreetData::ChallengeHighScoreEntry* lpaChallengeHighScores);

    // X360: ProgressionManager::SetRoadRuleChallengeData -> Profile::SetRoadRuleChallengeData.
    // Wholesale 2560-byte copy of the 64-entry ChallengePlayerScoreEntry table into maChallengeData.
    void SetRoadRuleChallengeData(const BrnStreetData::ChallengePlayerScoreEntry* lpaChallengeScores);

    // ------------------------------------------------------------------------
    // ADDITIVE GROW (StreetManager wave-C keystone) -- the road-rules profile accessors the
    // DWARF declares at BrnProfile.h:795-830 (+ the showtime pair at :476/:479). All are
    // header-inline in the original (the X360 folds each into its StreetManager caller:
    // OnProfileLoaded @0x82349E20, ProcessConnectedOnlineEvent @0x8234A148,
    // UpdateUserScoresFromServerRecords @0x82348FC0, ProcessNewRoadScore @0x823496C8), so the
    // bodies are the raw member reads/writes those callsites attest. No layout change.
    // ------------------------------------------------------------------------

    // DWARF :795/:798. Row accessors into the persisted road-rules tables. The OnProfileLoaded
    // memcpy sources are the row-0 pointers (the X360 reads the table bases directly).
    const BrnStreetData::ChallengeHighScoreEntry* GetNetworkChallengeData(s32 liIndex) const
    {
        return &maNetworkChallengeData[liIndex];
    }
    const BrnStreetData::ChallengePlayerScoreEntry* GetUserChallengeData(s32 liIndex) const
    {
        return &maChallengeData[liIndex];
    }

    // DWARF :812/:817. muTimeStampOfLastRoadRulesDownload (+96).
    u32  GetNetworkChallengeDownloadTimestamp() const { return muTimeStampOfLastRoadRulesDownload; }
    void SetNetworkChallengeDownloadTimestamp(u32 luTimeStamp) { muTimeStampOfLastRoadRulesDownload = luTimeStamp; }

    // DWARF :821/:826. muLastRoadRulesResetTime (+102616).
    u32  GetLastRoadRulesResetTime() const { return muLastRoadRulesResetTime; }
    void SetLastRoadRulesResetTime(u32 luResetTime) { muLastRoadRulesResetTime = luResetTime; }

    // DWARF :830 (non-const there; kept verbatim). The persisted road-rules session id,
    // assembled from its two persisted halves (+118064 high, +118044 low) exactly as the
    // X360 folds it at every score-summary payload build.
    u64 GetRoadRulesID()
    {
        return (static_cast<u64>(muRoadRulesIDHighBits) << 32) + muRoadRulesIDLowBits;
    }

    // DWARF :476/:479. miHighestShowTimeScore (+612). Plain get/set; the "only if greater"
    // guard is the ProcessNewRoadScore callsite's own attested branch.
    s32  GetNewHighShowtimeScore() const { return miHighestShowTimeScore; }
    void SetNewHighShowtimeScore(s32 liScore) { miHighestShowTimeScore = liScore; }

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
    // The four game-mode tally arrays are [18] on X360 (PS3 DWARF [17]; the DLC island mode
    // adds slot 17, which Serialise/Deserialise round-trip through the DLC side-blob instead
    // of the main save image). Bases proven by the tally functions: +120 (GetGameModeType-
    // Amount), +192 (Add/GetGameModeTypeDiscovered), +264/+336 (AddGameModeTypeCompleted).
    s32   maGameModeTypeAmount[18];                          // +120
    s32   maGameModeTypeAmountDiscovered[18];                // +192
    s32   maGameModeTypeAmountCompleted[18];                 // +264
    s32   maGameModeTypeAmountCompletedSinceTheStart[18];    // +336
    s32   miTotalTakedownCount;                              // +408 (AddTakedown ++(this+408))
    s32   miTotalOnlineVerticleTakedownCount;                // +412
    s32   maiTakedownTypeCounts[13];                         // +416 (DWARF [13]; AddTakedown writes +416+4*type)
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
    // ⭐⭐ ORDER IS THE X360's, NOT THE DWARF's -- see the accessor pair above. DecFIGS declares
    // mbGoldCarsUnlocked first (BrnProfile.h:1228/:1229); CheckForSpecialCarUnlocks @0x82396058
    // prints +42516 as AreSilverCarsUnlocked() and gates it on the progression rank, and prints
    // +42517 as AreGoldCarsUnlocked() and gates it on 100% completion.
    bool  mbSilverCarsUnlocked;                              // +42516  (rank-gated)
    bool  mbGoldCarsUnlocked;                                // +42517  (100%-completion-gated)
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
    // Trophy-unlock-sequence-seen bits (DWARF BrnProfile.h:1279). One 64-bit field; the X360
    // Get/SetSeenTrophyUnlockSequence bit-op the qword at +118024 exactly where the DWARF
    // orders it. (The DWARF's FOLLOWING member, mAchievementsEarnt BitArray<60u>, is NOT here
    // on X360 -- the completion booleans below sit at +118032 -- and stays unplaced.)
    CgsContainers::BitArray<35u> mSeenTrophyAwardBitArray;   // +118024 (8 bytes)
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
    // ----- X360 DLC-era tail (the PS3 DWARF member list stops at miPad4) -----
    // Member names are this TU's (the tail is absent from the DWARF); offsets/strides are
    // X360-proven by the target-score / upload-score / developer-challenge functions. The
    // X360 Construct's three late zero stores (+120032 / +120824 / +120832) are exactly the
    // two Array count-word Clear()s and the FastBitArray zero.
    //
    // NOTE: this struct is NOT byte-size-asserted. The X360 member offsets quoted throughout are
    // the 32-bit-pointer ABI offsets (proven from the XEX); the PC reconstruction compiles 64-bit,
    // so any embedded pointer-bearing member (e.g. CgsNetwork::NetworkTexture, the Array<>/Set<>
    // count words, BitArray storage) is naturally wider here. Every function reaches its members BY
    // NAME, so behaviour is identical regardless of the exact byte offset on the PC target.
    Array<BrnGameState::GameStateModuleIO::TargetEventScore, 49> maTargetEventScores;   // X360 +118072 (count @ +120032 == +0x7A8 into the array)
    Array<BrnNetwork::LocalEventScoreUploadData, 49>             maEventScoresToUpload; // X360 +120040 (count @ +120824 == +0x310 into the array)
    CgsContainers::FastBitArray<15u>                             mDeveloperChallengesCompleted; // X360 +120832 (one u64 field; 15 == GsmIO::E_DEVELOPER_CHALLENGE_COUNT)
    // X360 end: +120840 == sizeof(Profile)

    // Never-called compile-time attestation of the completion-sequence flag cluster (X360
    // accessor asm: Get/SetSeen100PercentCompletionSequence lbzx/stbx @ +0x1CD10 (118032),
    // Get/SetSeenEliteCompletionSequence @ +0x1CD16 (118038)). The ABSOLUTE X360 offsets are
    // not pinnable on the x64 gate host (pointer-bearing members earlier in the image widen --
    // see the NOTE above), so pin the pointer-width-INVARIANT facts the four accessors depend
    // on: the flags are single bytes, and the elite flag sits exactly 6 bytes after the
    // 100%-completion flag (the bool run between them is attested by Construct's byte stores
    // @ +118032..+118039).
    static void _AssertLayout()
    {
        static_assert(sizeof(mb100PercentCompletionSequenceShown) == 1,
                      "completion flags are single bytes (X360 lbzx/stbx)");
        static_assert(offsetof(Profile, mbHaveSeenEliteCompletionSequence) -
                      offsetof(Profile, mb100PercentCompletionSequenceShown) == 6,
                      "X360: elite flag @ +0x1CD16 == 100%-completion flag @ +0x1CD10 + 6");
        static_assert(offsetof(Profile, mbRedundantBool4) -
                      offsetof(Profile, mb100PercentCompletionSequenceShown) == 7,
                      "X360: the eight-bool completion cluster is contiguous (+118032..+118039)");
    }
};

} // namespace BrnProgression

#endif // BRN_PROFILE_H
