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
#include "pc/gcm/renderengine/pixelformat.h"                     // renderengine::PIXELFORMAT_DXT1

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
    const u32 KU_GOLD_CARS_UNLOCKED             = 42500;    // 0xA604
    const u32 KU_SILVER_CARS_UNLOCKED           = 42501;    // 0xA605
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
    static_assert(sizeof(macName) == 32, "X360 memcpy(image+4, name, 0x20)");
    StoreImageBlock(lpaImage, SaveImage::KU_NAME, macName, sizeof(macName));

    static_assert(sizeof(Vector3) == 16, "X360 stores each Vector3 as two std");
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

    static_assert(sizeof(maiTakedownTypeCounts) == 52, "X360 memcpy(image+0x190, +0x1A0, 0x34)");
    StoreImageBlock(lpaImage, SaveImage::KU_TAKEDOWN_TYPE_COUNTS, maiTakedownTypeCounts, sizeof(maiTakedownTypeCounts));

    static_assert(sizeof(maiWinsPerOfflineGameMode) == 40, "X360 10-word copy loop");
    static_assert(sizeof(maiRankWinsPerOfflineGameMode) == 40, "X360 10-word copy loop");
    static_assert(sizeof(maiLossesPerOfflineGameMode) == 40, "X360 10-word copy loop");
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
    static_assert(sizeof(maStuntElements) == 12312, "X360 memcpy(image+0x75E8, +0x75F8, 0x3018)");
    StoreImageBlock(lpaImage, SaveImage::KU_STUNT_ELEMENT_SETS, maStuntElements, sizeof(maStuntElements));

    StoreImageField(lpaImage, SaveImage::KU_MEDAL_COUNT_FROM_THE_START, muMedalCountFromTheStart);
    StoreImageField(lpaImage, SaveImage::KU_GOLD_CARS_UNLOCKED,         mbGoldCarsUnlocked);
    StoreImageField(lpaImage, SaveImage::KU_SILVER_CARS_UNLOCKED,       mbSilverCarsUnlocked);

    // FLAG (console defect, reproduced): the JUNK YARD drive-thru set is never serialised.
    // The X360 emits the body-shop copy TWICE, back to back with identical operands
    // (0x8237C570 and 0x8237C580, both memcpy(image+0xA638, live+0xA648, 0x60)), where the
    // first of the pair should have been memcpy(image+0xA608, live+0xA618, 0x30) -- the
    // junk-yard set. Deserialise mirrors the same duplicated pair (0x8237D604/0x8237D614),
    // so image+42504 is written by nobody and mJunkYardsDriveThruSet is lost across a
    // save/load. Reproduced exactly: this is observable behaviour ("visited junkyards"
    // discovery does not persist), not a transcription slip.
    static_assert(sizeof(mBodyShopsDriveThruSet)   == 96,  "X360 memcpy 0x60");
    static_assert(sizeof(mPaintShopsDriveThruSet)  == 48,  "X360 six-qword copy loop");
    static_assert(sizeof(mGasStationsDriveThruSet) == 120, "X360 memcpy 0x78");
    static_assert(sizeof(mCarParksDriveThruSet)    == 96,  "X360 memcpy 0x60");
    StoreImageBlock(lpaImage, SaveImage::KU_BODY_SHOPS_SET, &mBodyShopsDriveThruSet, sizeof(mBodyShopsDriveThruSet));
    StoreImageBlock(lpaImage, SaveImage::KU_BODY_SHOPS_SET, &mBodyShopsDriveThruSet, sizeof(mBodyShopsDriveThruSet));
    StoreImageBlock(lpaImage, SaveImage::KU_PAINT_SHOPS_SET,  &mPaintShopsDriveThruSet,  sizeof(mPaintShopsDriveThruSet));
    StoreImageBlock(lpaImage, SaveImage::KU_GAS_STATIONS_SET, &mGasStationsDriveThruSet, sizeof(mGasStationsDriveThruSet));
    StoreImageBlock(lpaImage, SaveImage::KU_CAR_PARKS_SET,    &mCarParksDriveThruSet,    sizeof(mCarParksDriveThruSet));

    static_assert(sizeof(maFreeBurnChallengeData) == 16008, "X360 memcpy 0x3E88");
    static_assert(sizeof(mabHitPropBitArray)      == 37504, "X360 memcpy 0x9280");
    static_assert(sizeof(maaiStuntCountsByCounty) == 30,    "X360 memcpy 0x1E");
    static_assert(sizeof(maNetworkChallengeData)  == 3584,  "X360 memcpy 0xE00");
    static_assert(sizeof(maChallengeData)         == 2560,  "X360 memcpy 0xA00");
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
    static_assert(sizeof(macPlayerLicenceTextureData) == 9600, "X360 memcpy 0x2580");
    StoreImageBlock(lpaImage, SaveImage::KU_LICENCE_TEXTURE_DATA, macPlayerLicenceTextureData, sizeof(macPlayerLicenceTextureData));
    StoreImageField(lpaImage, SaveImage::KU_LICENCE_PICTURE_IS_VALID, mbPlayerLicencePictureIsValid);

    // ---- mugshot galleries -------------------------------------------------------------
    // The console copies all five galleries in one 5640-byte memcpy (5 * 1128). On x64 an
    // Array<MugshotInfo,20> is 1124 bytes (no trailing pad after the count word), so the
    // copy is done per gallery at the console's 1128-byte image stride -- which keeps every
    // gallery's count word at image + gallery*1128 + 1120, exactly where the console put it
    // (and where BrnGuiProfile.cpp's stored-image constants expect it).
    static_assert(sizeof(maaMugshotInfo[0]) <= SaveImage::KU_MUGSHOT_GALLERY_STRIDE,
                  "a live gallery must fit the console's 1128-byte image slot");
    for (s32 liGallery = 0; liGallery < SaveImage::KI_MUGSHOT_GALLERY_COUNT; ++liGallery)
    {
        StoreImageBlock(lpaImage,
                        SaveImage::KU_MUGSHOT_GALLERIES
                            + static_cast<u32>(liGallery) * SaveImage::KU_MUGSHOT_GALLERY_STRIDE,
                        &maaMugshotInfo[liGallery], sizeof(maaMugshotInfo[liGallery]));
    }

    static_assert(sizeof(maAvailableMugshotFileIDs) == 40, "X360 five-qword copy loop");
    static_assert(sizeof(mafCarTypes)               == 12, "X360 three-word copy");
    static_assert(sizeof(maHasPlayerSeenTraining)   == 32, "X360 four-qword copy");
    StoreImageBlock(lpaImage, SaveImage::KU_MUGSHOT_FILE_ID_BITS, maAvailableMugshotFileIDs, sizeof(maAvailableMugshotFileIDs));
    StoreImageBlock(lpaImage, SaveImage::KU_CAR_TYPES,            mafCarTypes,               sizeof(mafCarTypes));
    StoreImageField(lpaImage, SaveImage::KU_CURRENT_CAR_TYPE,     meCurrentCarType);
    StoreImageBlock(lpaImage, SaveImage::KU_TRAINING_SEEN_BITS,   &maHasPlayerSeenTraining,  sizeof(maHasPlayerSeenTraining));

    // ---- online tallies, dates, completion flags ---------------------------------------
    static_assert(sizeof(CgsSystem::DateAndTime) == 12, "X360 three-word date copy");
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

    static_assert(sizeof(maTargetEventScores)   == 1968, "X360 memcpy(dlc1+8, +0x1CD38, 0x7B0)");
    static_assert(sizeof(maEventScoresToUpload) == 792,  "X360 memcpy(dlc1+0x7B8, +0x1D4E8, 0x318)");
    StoreImageBlock(lpaImageDLC, SaveImageDLC1::KU_TARGET_EVENT_SCORES,    &maTargetEventScores,   sizeof(maTargetEventScores));
    StoreImageBlock(lpaImageDLC, SaveImageDLC1::KU_EVENT_SCORES_TO_UPLOAD, &maEventScoresToUpload, sizeof(maEventScoresToUpload));

    // The developer-challenge bits are TRANSCRIBED, not copied: the live set is a
    // FastBitArray<15>, the image's is a FastBitArray<120>. The X360 clears both image
    // fields first (redundant after the memset, but it is the container's Clear) and then
    // walks the live set's iterator, setting each index it reports.
    static_assert(sizeof(DeveloperChallengeImageBits) == 16, "the DLC1 slot is 0x2628..0x2638");
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

    static_assert(sizeof(maCars)          == 12288, "X360 memcpy 0x3000 (512 * 0x18)");
    static_assert(sizeof(maLiveryChoices) == 12288, "X360 memcpy 0x3000 (512 * 0x18)");
    static_assert(sizeof(maRivals)        == 3584,  "X360 memcpy 0xE00 (64 * 0x38)");
    static_assert(sizeof(maEvents)        == 1400,  "X360 memcpy 0x578 (175 * 8)");
    LoadImageBlock(lpaImage, SaveImage::KU_CARS,     maCars,          sizeof(maCars));
    LoadImageBlock(lpaImage, SaveImage::KU_LIVERIES, maLiveryChoices, sizeof(maLiveryChoices));
    LoadImageBlock(lpaImage, SaveImage::KU_RIVALS,   maRivals,        sizeof(maRivals));
    LoadImageBlock(lpaImage, SaveImage::KU_EVENTS,   maEvents,        sizeof(maEvents));

    LoadImageBlock(lpaImage, SaveImage::KU_STUNT_ELEMENT_SETS, maStuntElements, sizeof(maStuntElements));

    LoadImageField(lpaImage, SaveImage::KU_MEDAL_COUNT_FROM_THE_START, muMedalCountFromTheStart);
    LoadImageField(lpaImage, SaveImage::KU_GOLD_CARS_UNLOCKED,         mbGoldCarsUnlocked);
    LoadImageField(lpaImage, SaveImage::KU_SILVER_CARS_UNLOCKED,       mbSilverCarsUnlocked);

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
