// ===================================================================================
// BrnGui::PreRaceFlyByState -- wave-J partfile 06: the compass description family.
//   SetRaceDescription         @0x824C6E90  (DWARF BrnPreRaceFlyBy.cpp:1128)
//   SetMarkedManDescription    @0x824C7470  (DWARF BrnPreRaceFlyBy.cpp:1346)
//   SetBurningRouteDescription @0x824C76D8  (DWARF BrnPreRaceFlyBy.cpp:1451)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the raw `assembly` listing of each address
// arbitrates over the Hex-Rays pseudocode throughout (the three listings are in
// scratchpad/waveJ/asm_setrace.txt, asm_g06_0x824C7470.txt and asm_g06_0x824C76D8.txt).
//
// 2026-08-03 RECONCILIATION: all three bodies were parked behind six missing names --
// SatNavIconInfo::GetCgsId(), Profile::{GetNumWinsForGameMode, GetCurrentProgressionRank,
// GetEventCount}, GuiCache::GetEventID() and the whole
// GameSource/GameState/Progression/BrnDerivedCars.h header. Every one has landed, so all
// three bodies are here.
//
// SIGNED-COMPARE NOTE (this is the one place the landed accessor set changes the code):
// Profile::GetEventCount() is `u32` (DWARF BrnProfile.h:1009; the committed header at
// BrnProfile.h:356 matches), but the X360's profile-event walk compares SIGNED --
// `cmpwi`/`ble` on the pre-guard and `cmpw`/`blt` on the bound. A raw
// `s32 i < lpProfile->GetEventCount()` would promote i to unsigned and invert that guard,
// so each walk hoists the count into an s32 first.
//
// FILE-SCOPE DATA THE GATE ALREADY VERIFIED: the `sizeof(GuiEventPreRaceDescription) == 40`
// static_assert passes, i.e. the host record really is byte-for-byte the 40-byte record
// the X360 posts.
//
// CORRECTIONS TO THE WAVE-J SPEC, measured from the asm (spec section 3 flagged both as
// "READ ASM"):
//   * The id-464 payload slot order is the SAME in all three workers: +8 is the compass
//     direction FindEventDirection returned, +12 is the game-mode ordinal (0 race, 8
//     marked man, 5 burning route). The spec's guesses "markedman {8, dir}" and
//     "burning {5, car-word}" are both wrong -- there is no car word in the payload.
//   * The per-mode profile tallies confirm those ordinals independently: the workers read
//     maiWinsPerOfflineGameMode/maiLossesPerOfflineGameMode at +468/+548 biased by
//     4*mode (0x1D4/0x224 for mode 0, 0x1F4/0x244 for mode 8, 0x1E8/0x238 for mode 5).
//   * SetMarkedManDescription's PART3 really is SnPrintf("%f") of the target time -- a
//     raw float print, not a clock format -- and its parameter format word is
//     E_FORMAT_INTEGER (11). SetBurningRouteDescription's PART3 uses the language
//     manager's clock formatter and passes E_FORMAT_TEXT (0).
// ===================================================================================

#include "GameSource/Gui/Flow/PreEvent/States/BrnPreRaceFlyBy.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsID / CgsIDConvertToString
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SnPrintf
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // ParameterFormatType + the clock formatter
#include "GameSource/GameState/BrnGameStateTypes.h"                       // BrnGameState::LandmarkIndex
#include "GameSource/GameState/Progression/BrnProfile.h"                  // Profile / ProfileEvent
#include "GameSource/GameState/Progression/BrnDerivedCars.h"              // BrnProgression::DerivedCarArray
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // SatNavIconInfo
#include "GameSource/Gui/BrnGuiShared.h"                                  // BrnGui::ECompassPoints
#include "GameSource/Gui/BrnGuiWorldDataController.h"                     // BrnGui::WorldDataController
#include "SharedClasses/Progression/BrnRaceEventData.h"                   // BrnProgression::RaceEventData

namespace GSM = BrnGameState::GameStateModuleIO;

namespace BrnGui
{

namespace
{
    typedef CgsLanguage::LanguageManager LM;

    // The DIRECTION_* localisation-id table @0x82F27820 (image read; dump at
    // scratchpad/waveJ/probe_PreRaceFlyBy_6/verify2.txt), indexed by ECompassPoints.
    //
    // DWARF-attested global, NOT this TU's data: dwarfdump GameSource/Gui/BrnGuiShared.cpp:31
    // declares `extern const char *[8] KAPC_COMPASS_POINT_STRINGIDS;` attributed to
    // BrnGuiShared.cpp:140 -- external linkage, owned by BrnGuiShared.cpp. The five
    // consecutive tables at 0x82F277E0..0x82F27868 reproduce that file's declaration order
    // exactly (POSITION x8, POSITION_LOWERCASE x8, DIRECTION x8 @0x82F27820, GAMEMODE x10,
    // GAMEMODE_PLURAL x10), which is how the attribution was confirmed from the image.
    // It carries the DWARF name here but stays file-local because its home does not exist
    // yet. DELETE-WHEN GameSource/Gui/BrnGuiShared.cpp lands (declare it in BrnGuiShared.h
    // and index the shared one).
    const char* const KAPC_COMPASS_POINT_STRINGIDS[E_COMPASS_POINTS_COUNT] =
    {
        "DIRECTION_N",  "DIRECTION_NW", "DIRECTION_W",  "DIRECTION_SW",
        "DIRECTION_S",  "DIRECTION_SE", "DIRECTION_E",  "DIRECTION_NE",
    };

    const s32 KI_GUI_OUT_EVENT_CHANNEL  = 40;
    const u32 KU_DESCRIPTION_BUFFER_LEN = 128;

    struct GuiEventPreRaceDescription : public CgsGui::GuiEvent<464>
    {
        alignas(8) CgsID mLandmarkId;   // +16
        s32 miDirection;                // +24
        s32 miGameMode;                 // +28
        s32 miTimesPlayed;              // +32
        u8  mbEventFlag;                // +36
        u8  mu8ProgressionRank;         // +37

        // Header words are DERIVED from the host record, not the console immediates:
        // word0 == the payload byte count (X360 0x18 == 24 == 40 - 16) and word2 == the
        // payload offset (X360 0x10 == 16). The sizeof pin below is what makes the two
        // agree with the console record.
        GuiEventPreRaceDescription()
            : CgsGui::GuiEvent<464>(
                  static_cast<u32>(sizeof(GuiEventPreRaceDescription)
                                   - offsetof(GuiEventPreRaceDescription, mLandmarkId)),
                  static_cast<u32>(offsetof(GuiEventPreRaceDescription, mLandmarkId)))
            , mLandmarkId(0)
            , miDirection(0)
            , miGameMode(0)
            , miTimesPlayed(0)
            , mbEventFlag(0)
            , mu8ProgressionRank(0)
        {
        }
    };
    static_assert(sizeof(GuiEventPreRaceDescription) == 40,
                  "the host record matches the X360 40-byte id-464 post");
}

// -------------------------------------------------------------------------------------
// SetBurningRouteDescription @ 0x824C76D8 (DWARF cpp:1451) -- the burning-route flavour:
// the compass line, the destination landmark, the target time as a clock, and the name of
// the alternate-livery car the route pays out. See the banner above for the asm notes.
// -------------------------------------------------------------------------------------
void PreRaceFlyByState::SetBurningRouteDescription()
{
    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:1456

    const ECompassPoints leEventDirection = FindEventDirection();
    maEventDescriptionText[0].SetLocalisedText("PRE_BURNINGROUTE_PART1", LM::E_FORMAT_ID_LOOKUP, 1,
                                               KAPC_COMPASS_POINT_STRINGIDS[leEventDirection],
                                               LM::E_FORMAT_ID_LOOKUP);

    GuiEventUpdateSatNav::SatNavIconInfo lLandmarkInfo;
    mpGuiCache->GetLandmarkInfoFromIndex(mpGuiCache->GetEventDestinationLandmarkIndex(),
                                         &lLandmarkInfo);

    char lacBuffer[KU_DESCRIPTION_BUFFER_LEN];
    CgsCore::SnPrintf(lacBuffer, KU_DESCRIPTION_BUFFER_LEN, "LM_%llu", lLandmarkInfo.GetCgsId());
    lacBuffer[KU_DESCRIPTION_BUFFER_LEN - 1] = 0;
    maEventDescriptionText[1].SetLocalisedText("PRE_BURNINGROUTE_PART2", LM::E_FORMAT_ID_LOOKUP, 1,
                                               lacBuffer, LM::E_FORMAT_ID_LOOKUP);

    // The route's target time is rendered as a clock by the language manager. PPC float
    // ABI: the time travels in f1 and SKIPS its GPR slot, so the console's r6 (128) is the
    // buffer size, not a fourth argument -- matching the committed
    // LanguageManager::FormatMinutesSecondsStringMediumText(char*, f32, s32) const.
    mpStateInterface->GetLanguageManager()->FormatMinutesSecondsStringMediumText(
        lacBuffer, mpGuiCache->GetTargetTimeInEvent(),
        static_cast<s32>(KU_DESCRIPTION_BUFFER_LEN));
    maEventDescriptionText[2].SetLocalisedText("PRE_BURNINGROUTE_PART3", LM::E_FORMAT_ID_LOOKUP, 1,
                                               lacBuffer, LM::E_FORMAT_TEXT);

    maEventDescriptionText[3].SetLocalisedText("PRE_BURNINGROUTE_PART4", LM::E_FORMAT_ID_LOOKUP);

    // ---- the alternate-livery car this route pays out ----
    CGS_ASSERT(mpGuiCache->GetWorldDataController() != 0, "lpWorldDataController");   // cpp:1521
    const WorldDataController* lpWorldDataController = mpGuiCache->GetWorldDataController();

    const BrnProgression::RaceEventData* lpEventData =
        lpWorldDataController->GetEventInfoFromEventId(mpGuiCache->GetEventID());
    CGS_ASSERT(lpEventData != 0, "lpEventData");                                     // cpp:1523

    // cpp:1524 -- the X360 inlines IsSpecialEvent() to "the record's +0x10 doubleword is
    // not null". It then loads that SAME doubleword a second time as the players-car id
    // and asserts it again at cpp:1526, so both asserts are kept, in source order.
    // (No kCGSID_NULL constant is committed anywhere in the tree yet; the literal 0 the
    // X360 compares against stands in for it.)
    CGS_ASSERT(lpEventData->GetEventInstanceId() != 0, "lpEventData->IsSpecialEvent()");
    const CgsID lPlayersCarId = lpEventData->GetEventInstanceId();
    CGS_ASSERT(lPlayersCarId != 0, "kCGSID_NULL != lPlayersCarId");                  // cpp:1526
    CGS_ASSERT(lpWorldDataController->GetVehicleList() != 0,
               "lpWorldDataController->GetVehicleList()");                           // cpp:1529

    BrnProgression::DerivedCarArray lCarVariants;
    lCarVariants.ConstructPatternLiveryList(lpWorldDataController->GetVehicleList(), lPlayersCarId);

    // Walk past the player's own car to the first pattern-livery variant. The X360
    // compares the index against the live count SIGNED (`cmpw`), hence the cast; and it
    // adds no end-of-array guard -- if every entry matches, the indexed accessor's own
    // bounds assert is what fires.
    s32 liVariant = 0;
    while (liVariant < static_cast<s32>(lCarVariants.GetLength())
           && lCarVariants.GetItem(liVariant) == lPlayersCarId)
    {
        ++liVariant;
    }
    CGS_ASSERT(lPlayersCarId != lCarVariants.GetItem(liVariant),
               "lPlayersCarId != lCarVariants.GetItem(liVariant)");                  // cpp:1538

    char lacCarId[KI_CGSID_STRING_LEN];
    CgsIDConvertToString(lCarVariants.GetItem(liVariant), lacCarId);
    lacCarId[KI_CGSID_STRING_LEN - 1] = 0;
    CgsCore::SnPrintf(lacBuffer, KU_DESCRIPTION_BUFFER_LEN, "CAR_CAPS_%s", lacCarId);
    lacBuffer[KU_DESCRIPTION_BUFFER_LEN - 1] = 0;
    maEventDescriptionText[4].SetLocalisedText("PRE_BURNINGROUTE_PART5", LM::E_FORMAT_ID_LOOKUP, 1,
                                               lacBuffer, LM::E_FORMAT_ID_LOOKUP);

    // ---- the id-464 medals-panel record ----
    const BrnProgression::Profile* lpProfile = mpGuiCache->GetProfile();

    GuiEventPreRaceDescription lDescription;
    lDescription.mLandmarkId        = lLandmarkInfo.GetCgsId();
    lDescription.miDirection        = leEventDirection;
    lDescription.miGameMode         = GSM::E_MODE_BURNING_ROUTE;
    lDescription.miTimesPlayed      = lpProfile->GetNumWinsForGameMode(GSM::E_MODE_BURNING_ROUTE)
                                    + lpProfile->GetNumLossesForGameMode(GSM::E_MODE_BURNING_ROUTE);
    lDescription.mu8ProgressionRank = static_cast<u8>(lpProfile->GetCurrentProgressionRank());

    const u32 luEventId = mpGuiCache->GetEventID();
    const BrnProgression::ProfileEvent* lpProfileEvent = 0;
    // Profile::GetEventCount() is u32; hoist it into an s32 so the walk keeps the
    // console's SIGNED cmpwi/ble pre-guard and cmpw/blt bound (see the banner).
    const s32 liEventCount = static_cast<s32>(lpProfile->GetEventCount());
    for (s32 liEvent = 0; liEvent < liEventCount; ++liEvent)
    {
        if (lpProfile->GetEvent(liEvent)->GetID() == luEventId)
        {
            lpProfileEvent = lpProfile->GetEvent(liEvent);
            break;
        }
    }
    // Same console null-dereference on no-match as SetRaceDescription -- reproduced.
    // This worker takes the SPECIAL-EVENT bit, not the rank-win bit.
    lDescription.mbEventFlag =
        (lpProfileEvent->GetFlags()
         & BrnProgression::ProfileEvent::E_FLAG_WON_SPECIAL_EVENT_BEFORE) != 0;

    mpStateInterface->GetOutputEventQueue()->AddEvent(
        &lDescription, KI_GUI_OUT_EVENT_CHANNEL, static_cast<s32>(sizeof(lDescription)));
}

// -------------------------------------------------------------------------------------
// SetMarkedManDescription @ 0x824C7470 (DWARF cpp:1346) -- the marked-man (survival)
// flavour: the compass line, the destination landmark, the survival timer and its caption.
// See the banner above for the per-instruction asm notes.
// -------------------------------------------------------------------------------------
void PreRaceFlyByState::SetMarkedManDescription()
{
    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:1351

    const ECompassPoints leEventDirection = FindEventDirection();
    maEventDescriptionText[0].SetLocalisedText("PRE_SURVIVAL_PART1", LM::E_FORMAT_ID_LOOKUP, 1,
                                               KAPC_COMPASS_POINT_STRINGIDS[leEventDirection],
                                               LM::E_FORMAT_ID_LOOKUP);

    GuiEventUpdateSatNav::SatNavIconInfo lLandmarkInfo;
    mpGuiCache->GetLandmarkInfoFromIndex(mpGuiCache->GetEventDestinationLandmarkIndex(),
                                         &lLandmarkInfo);

    char lacBuffer[KU_DESCRIPTION_BUFFER_LEN];
    CgsCore::SnPrintf(lacBuffer, KU_DESCRIPTION_BUFFER_LEN, "LM_%llu", lLandmarkInfo.GetCgsId());
    lacBuffer[KU_DESCRIPTION_BUFFER_LEN - 1] = 0;
    maEventDescriptionText[1].SetLocalisedText("PRE_SURVIVAL_PART2", LM::E_FORMAT_ID_LOOKUP, 1,
                                               lacBuffer, LM::E_FORMAT_ID_LOOKUP);

    // The survival target time goes out as a raw "%f" -- the X360 really does print the
    // float, then hand the resulting text to the field with an INTEGER parameter format.
    CgsCore::SnPrintf(lacBuffer, KU_DESCRIPTION_BUFFER_LEN, "%f",
                      mpGuiCache->GetTargetTimeInEvent());
    lacBuffer[KU_DESCRIPTION_BUFFER_LEN - 1] = 0;
    maEventDescriptionText[2].SetLocalisedText("PRE_SURVIVAL_PART3", LM::E_FORMAT_ID_LOOKUP, 1,
                                               lacBuffer, LM::E_FORMAT_INTEGER);

    maEventDescriptionText[3].SetLocalisedText("PRE_SURVIVAL_PART4", LM::E_FORMAT_ID_LOOKUP);
    maEventDescriptionText[4].SetText("");

    // ---- the id-464 medals-panel record ----
    const BrnProgression::Profile* lpProfile = mpGuiCache->GetProfile();

    GuiEventPreRaceDescription lDescription;
    lDescription.mLandmarkId        = lLandmarkInfo.GetCgsId();
    lDescription.miDirection        = leEventDirection;
    lDescription.miGameMode         = GSM::E_MODE_MARKED_MAN;
    lDescription.miTimesPlayed      = lpProfile->GetNumWinsForGameMode(GSM::E_MODE_MARKED_MAN)
                                    + lpProfile->GetNumLossesForGameMode(GSM::E_MODE_MARKED_MAN);
    lDescription.mu8ProgressionRank = static_cast<u8>(lpProfile->GetCurrentProgressionRank());

    const u32 luEventId = mpGuiCache->GetEventID();
    const BrnProgression::ProfileEvent* lpProfileEvent = 0;
    // Profile::GetEventCount() is u32; hoist it into an s32 so the walk keeps the
    // console's SIGNED cmpwi/ble pre-guard and cmpw/blt bound (see the banner).
    const s32 liEventCount = static_cast<s32>(lpProfile->GetEventCount());
    for (s32 liEvent = 0; liEvent < liEventCount; ++liEvent)
    {
        if (lpProfile->GetEvent(liEvent)->GetID() == luEventId)
        {
            lpProfileEvent = lpProfile->GetEvent(liEvent);
            break;
        }
    }
    // Same console null-dereference on no-match as SetRaceDescription -- the X360 reads the
    // flags half-word off a NULL record pointer. Reproduced rather than guarded.
    lDescription.mbEventFlag =
        (lpProfileEvent->GetFlags() & BrnProgression::ProfileEvent::E_FLAG_RANK_WIN) != 0;

    mpStateInterface->GetOutputEventQueue()->AddEvent(
        &lDescription, KI_GUI_OUT_EVENT_CHANNEL, static_cast<s32>(sizeof(lDescription)));
}

// -------------------------------------------------------------------------------------
// SetRaceDescription @ 0x824C6E90 (DWARF cpp:1128) -- fill the description lines for an
// offline race and publish the medals-panel record.
//
// Notes taken from the asm rather than the pseudocode:
//  * The parameterised SetLocalisedText calls are the POSITIONAL-parameter overload
//    (sub_824E7800): `(id, format, liNumParams, <text, format> ...)`, one pair each.
//  * The trailing three lines are blanked by a counted loop (r30 = 3, stride 0x128 from
//    this+0x4D8 == maEventDescriptionText[2]), not by three unrolled calls.
//  * Both profile tallies come off the SAME base (cache+0x405C) at +0x1D4 and +0x224 --
//    the mode-0 slots of maiWinsPerOfflineGameMode / maiLossesPerOfflineGameMode.
//  * The profile-event walk leaves a NULL record pointer when nothing matches and
//    dereferences it anyway; reproduced, see the comment at the site.
// -------------------------------------------------------------------------------------
void PreRaceFlyByState::SetRaceDescription()
{
    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:1133

    const ECompassPoints leEventDirection = FindEventDirection();
    maEventDescriptionText[0].SetLocalisedText("PRE_RACE_PART1", LM::E_FORMAT_ID_LOOKUP, 1,
                                               KAPC_COMPASS_POINT_STRINGIDS[leEventDirection],
                                               LM::E_FORMAT_ID_LOOKUP);

    GuiEventUpdateSatNav::SatNavIconInfo lLandmarkInfo;
    mpGuiCache->GetLandmarkInfoFromIndex(mpGuiCache->GetEventDestinationLandmarkIndex(),
                                         &lLandmarkInfo);

    char lacBuffer[KU_DESCRIPTION_BUFFER_LEN];
    CgsCore::SnPrintf(lacBuffer, KU_DESCRIPTION_BUFFER_LEN, "LM_%llu", lLandmarkInfo.GetCgsId());
    lacBuffer[KU_DESCRIPTION_BUFFER_LEN - 1] = 0;
    maEventDescriptionText[1].SetLocalisedText("PRE_RACE_PART2", LM::E_FORMAT_ID_LOOKUP, 1,
                                               lacBuffer, LM::E_FORMAT_ID_LOOKUP);

    // A race uses only the first two lines; blank the rest.
    for (s32 liLine = 2; liLine < KI_MAX_LINES_DESCRIPTION_TEXT; ++liLine)
        maEventDescriptionText[liLine].SetText("");

    // ---- the id-464 medals-panel record ----
    const BrnProgression::Profile* lpProfile = mpGuiCache->GetProfile();

    GuiEventPreRaceDescription lDescription;
    lDescription.mLandmarkId        = lLandmarkInfo.GetCgsId();
    lDescription.miDirection        = leEventDirection;
    lDescription.miGameMode         = GSM::E_MODE_OFFLINE_RACE;
    lDescription.miTimesPlayed      = lpProfile->GetNumWinsForGameMode(GSM::E_MODE_OFFLINE_RACE)
                                    + lpProfile->GetNumLossesForGameMode(GSM::E_MODE_OFFLINE_RACE);
    lDescription.mu8ProgressionRank = static_cast<u8>(lpProfile->GetCurrentProgressionRank());

    const u32 luEventId = mpGuiCache->GetEventID();
    const BrnProgression::ProfileEvent* lpProfileEvent = 0;
    // Profile::GetEventCount() is u32; hoist it into an s32 so the walk keeps the
    // console's SIGNED cmpwi/ble pre-guard and cmpw/blt bound (see the banner).
    const s32 liEventCount = static_cast<s32>(lpProfile->GetEventCount());
    for (s32 liEvent = 0; liEvent < liEventCount; ++liEvent)
    {
        if (lpProfile->GetEvent(liEvent)->GetID() == luEventId)
        {
            lpProfileEvent = lpProfile->GetEvent(liEvent);
            break;
        }
    }
    // NOTE: the X360 leaves the record pointer NULL when the walk finds no match (and when
    // the profile holds no events at all) and reads its flags half-word regardless -- a
    // genuine console null-dereference. Reproduced rather than guarded.
    lDescription.mbEventFlag =
        (lpProfileEvent->GetFlags() & BrnProgression::ProfileEvent::E_FLAG_RANK_WIN) != 0;

    mpStateInterface->GetOutputEventQueue()->AddEvent(
        &lDescription, KI_GUI_OUT_EVENT_CHANNEL, static_cast<s32>(sizeof(lDescription)));
}

}
