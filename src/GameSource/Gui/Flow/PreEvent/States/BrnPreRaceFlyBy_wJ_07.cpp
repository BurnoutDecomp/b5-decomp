// ===================================================================================
// BrnGui::PreRaceFlyByState -- wave-J partfile 07: the "%d target" description pair.
//   SetRoadRageDescription  @0x824C7078  (DWARF BrnPreRaceFlyBy.cpp:1203)
//   SetFreestyleDescription @0x824C7230  (DWARF BrnPreRaceFlyBy.cpp:1266)
//
// Both bodies were reconstructed instruction by instruction from the raw X360 assembly
// (dumped to scratchpad/waveJ/asm_g07_roadrage.txt and asm_g07_freestyle.txt from
// .ida-exports/BURNOUT_X360_ARTIST.XEX/0x824C7078.json / 0x824C7230.json). Both
// pseudocode listings carry the Hex-Rays "local variable allocation has failed" warning
// and are wrong about the SPrintf parameter and the whole event payload, so the asm
// arbitrated everything.
//
// 2026-08-03 RECONCILIATION: both bodies were parked behind six missing names --
// GuiCache::{GetRequiredScoreForMedal, GetEventID},
// BrnGameState::E_CURRENT_MEDAL_TARGET_TIME_GOLD, and Profile::{GetNumWinsForGameMode,
// GetCurrentProgressionRank, GetEventCount}. Every one has landed, so both bodies are
// here. Note GetNumWinsForGameMode reads the +468 array and is NOT the committed
// GetNumRankWinsForGameMode, which reads the different +508 array.
//
// SIGNED-COMPARE NOTE: Profile::GetEventCount() is `u32` (DWARF BrnProfile.h:1009; the
// committed BrnProfile.h:356 matches), but the X360's profile-event walk compares SIGNED:
// `cmpwi cr6, r10, 0` / `ble` @0x824C7194 for the pre-guard and `cmpw cr6, r9, r10` /
// `blt` @0x824C71B4 for the bound (the same pair at 0x824C73D0 / 0x824C73F0 in the
// freestyle body). A raw `s32 i < lpProfile->GetEventCount()` promotes i to unsigned and
// inverts that guard, so both walks hoist the count into an s32 first. The compile gate
// cannot catch this -- both spellings build clean.
//
// CONSOLE-LITERAL NOTE: no X360 displacement is reproduced as a number anywhere here --
// the description lines are reached by INDEX (so the host's own TextField stride applies,
// not the console's 0x128) and the profile/cache far words through accessors. The only
// numeric literals that carry over are byte counts of char buffers (the 128 SPrintf cap
// and its 127 terminator index); the event-record header words are derived with
// sizeof/offsetof off the host record, not baked from the console immediates.
// ===================================================================================

#include "GameSource/Gui/Flow/PreEvent/States/BrnPreRaceFlyBy.h"
#include "GameSource/Gui/BrnGuiCache.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"                   // GSM::EGameModeType
#include "GameSource/GameState/Progression/BrnProfile.h"                 // Profile / ProfileEvent
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"   // BrnGameState::ECurrentMedalTargetTime
#include "GameSource/Gui/BrnGuiShared.h"                          // BrnGui::ECompassPoints

// House file-local alias (same spelling as the sibling PreEvent partfiles).
namespace GSM = BrnGameState::GameStateModuleIO;

namespace BrnGui
{

namespace
{
    // The AddEvent CHANNEL (`li r5, 0x28` at the freestyle post, 0x824C7418). The record
    // size argument the console pairs with it is also 0x28 there, but that is a
    // coincidence of this record being 40 bytes -- the size below is a host sizeof.
    const s32 KI_CHANNEL_GUI_OUT = 40;

    // `li r9, 8` @0x824C7390, stored into the payload's direction word. That 8 is exactly
    // BrnGui::ECompassPoints' E_COMPASS_POINTS_COUNT (BrnGuiShared.h) -- one past the
    // 8-entry DIRECTION_* table, i.e. "this event has no compass direction" -- so the
    // enumerator is named rather than the measured literal repeated. FLAG: the ROLE name
    // below is this partfile's; the value is the header's.
    const s32 KI_COMPASS_DIRECTION_NONE = E_COMPASS_POINTS_COUNT;

    // The id-464 pre-race description record. Same shape as the copy in partfile 06 (both
    // are file-local wire views of one console record; there is no shared home for it).
    // MEASURED at the freestyle post 0x824C7380..0x824C7458: the payload is staged in a
    // 24-byte scratch -- 8-byte landmark id at +0x00 (`std r27`), direction at +0x08,
    // game mode at +0x0C, times-played at +0x10, the event flag byte at +0x14 and the
    // progression-rank byte at +0x15 -- then copied as three doublewords into record+0x10
    // behind the header { size 0x18, id 0x1D0 == 464, payload offset 0x10 }, and posted
    // at 40 bytes on channel 40.
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

// -------------------------------------------------------------------------------
// SetFreestyleDescription  @0x824C7230   (cpp:1266)
// The stunt-run blurb: the "%d" score target on line 1, the target time as a mid-text
// minutes/seconds string on line 2, three blank lines, and the same id-464 description
// record the other four description workers post.
//
// Notes taken from the raw asm rather than the pseudocode:
//  * Line 1 is identical in shape to the road-rage one: the GuiCache FLOAT
//    mafTargetScores[0] (X360 +0x9F34) through `lfsx`/`fctiwz` (0x824C7290..0x824C7298)
//    into "%d", capped at 128 with a manual terminator at index 127, then
//    SetLocalisedText("PRE_STUNTRACE_PART1", 9, 1, buffer, 11).
//  * The target time is read through the committed GetTargetTimeInEvent(): the X360
//    inlines it here (`addis r30, r11, 1` / `addi r30, r30, -0x60D0` == cache + 0x9F30),
//    INCLUDING its own "0.0f <= mfTargetTime" assert (BrnGuiCache.h:2979, fired at
//    0x824C72EC when the `fcmpu`/`bge` at 0x824C72E4 falls through). The committed
//    accessor body already carries that assert, so calling it reproduces the console.
//  * FormatMinutesSecondsStringMediumText's PPC argument slots are r3 (this), r4 (the
//    buffer), f1 (the time) and r6 (128): the float travels in an FPR and SKIPS r5, so
//    r6 is the buffer size, not a fourth argument. That matches the committed
//    LanguageManager::FormatMinutesSecondsStringMediumText(char*, f32, s32) const.
//  * PART2's PARAMETER format is 0 == E_FORMAT_TEXT (`li r8, 0` at 0x824C7330) -- the
//    string is already formatted -- while the source id itself still resolves under 9.
//    That single immediate is the only shape difference from line 1.
//  * The blanking loop is `r29 = this + 0x4D8, r30 = 3, stride 0x128` -- description
//    fields 2..4 (0 and 1 were just filled). Reached by INDEX, so the host's own
//    TextField stride applies (0x128 is a console stride, comment only).
//  * The profile tallies are again INLINED array reads: `lwz r11, 0x1F0` == the wins
//    array (base 468) at index 7 and `lwz r7, 0x240` == the losses array (base 548) at
//    index 7 -- both E_MODE_STUNT_ATTACK.
//  * The profile-event walk, the NULL dereference when nothing matches, the
//    `(muFlags >> 2) & 1` == E_FLAG_RANK_WIN test and the AddEvent(record, 40, 40) post
//    are identical to the road-rage body; see its banner.
//
// The only float comparison reachable from this body is the >= 0.0f assert inside the
// committed GetTargetTimeInEvent(), so there is no NaN-polarity decision to make here.
// -------------------------------------------------------------------------------
void PreRaceFlyByState::SetFreestyleDescription()
{
    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:1271

    char lacParameterText[128];
    CgsCore::SPrintf(lacParameterText, sizeof(lacParameterText), "%d",
                     static_cast<s32>(mpGuiCache->GetRequiredScoreForMedal(
                         BrnGameState::E_CURRENT_MEDAL_TARGET_TIME_GOLD)));
    lacParameterText[sizeof(lacParameterText) - 1] = 0;

    maEventDescriptionText[0].SetLocalisedText(
        "PRE_STUNTRACE_PART1",
        CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
        1,
        lacParameterText,
        CgsLanguage::LanguageManager::E_FORMAT_INTEGER);

    mpStateInterface->GetLanguageManager()->FormatMinutesSecondsStringMediumText(
        lacParameterText, mpGuiCache->GetTargetTimeInEvent(),
        static_cast<s32>(sizeof(lacParameterText)));

    maEventDescriptionText[1].SetLocalisedText(
        "PRE_STUNTRACE_PART2",
        CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
        1,
        lacParameterText,
        CgsLanguage::LanguageManager::E_FORMAT_TEXT);

    for (s32 liLine = 2; liLine < KI_MAX_LINES_DESCRIPTION_TEXT; ++liLine)
    {
        maEventDescriptionText[liLine].SetText("");
    }

    const BrnProgression::Profile* lpProfile = mpGuiCache->GetProfile();

    GuiEventPreRaceDescription lDescription;
    lDescription.mLandmarkId        = 0;                          // no destination landmark
    lDescription.miDirection        = KI_COMPASS_DIRECTION_NONE;
    lDescription.miGameMode         = GSM::E_MODE_STUNT_ATTACK;
    lDescription.miTimesPlayed      = lpProfile->GetNumWinsForGameMode(GSM::E_MODE_STUNT_ATTACK)
                                    + lpProfile->GetNumLossesForGameMode(GSM::E_MODE_STUNT_ATTACK);
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
    // The console dereferences this unconditionally -- a NULL read when the running event
    // has no profile record.
    lDescription.mbEventFlag = static_cast<u8>(
        (lpProfileEvent->GetFlags() & BrnProgression::ProfileEvent::E_FLAG_RANK_WIN) != 0);

    mpStateInterface->GetOutputEventQueue()->AddEvent(&lDescription, KI_CHANNEL_GUI_OUT,
                                                     static_cast<s32>(sizeof(lDescription)));
}

// -------------------------------------------------------------------------------
// SetRoadRageDescription  @0x824C7078   (cpp:1203)
// Road rage's pre-race blurb: one line of "beat <N> takedowns", four blank lines, and
// the id-464 description record for the medals panel.
//
// Notes taken from the raw asm rather than the pseudocode:
//  * The "%d" parameter is the GuiCache FLOAT mafTargetScores[0] (X360 +0x9F34) run
//    through `fctiwz` (0x824C70D4..0x824C70DC: `lfsx f0, r10, r9` / `fctiwz` / `stfiwx`),
//    a round-toward-zero float->s32 conversion -- exactly what a C cast to s32 does. It
//    is NOT the s32 miScoreTarget (+0x9FC8) that the committed GetTargetScoreInEvent
//    returns; the lfsx/fctiwz pair proves the float.
//  * SPrintf's cap (128, `li r4, 0x80`) and the manual terminator index (127, `stb r27,
//    var_31` where var_31 == buffer + 0x7F) are BYTE COUNTS of a char buffer, not
//    pointer-width quantities, so they carry to the host unchanged.
//  * SetLocalisedText's immediates are 9 (E_FORMAT_ID_LOOKUP, the source id) / 1 (one
//    positional parameter) / 11 (E_FORMAT_INTEGER, the parameter's format).
//  * The blanking loop is `r29 = this + 0x3B0, r30 = 4, stride 0x128` -- description
//    fields 1..4, i.e. every line but the one just filled. Reached by INDEX here, so the
//    host's own TextField stride applies (the 0x128 is a console stride, comment only).
//  * The two profile tallies are INLINED array reads, not calls: `lwz r11, 0x1E0` == the
//    wins array (base 468) at index 3 and `lwz r7, 0x230` == the losses array (base 548)
//    at index 3 -- both E_MODE_ROAD_RAGE. The console adds losses + wins; the sum is
//    order-independent.
//  * The profile-event search is an inlined linear walk of maEvents (base +0x7080, stride
//    8) bounded by miEventCount (+0x278) -- the same walk the committed GetEvent's bounds
//    assert guards. When NO record matches, the X360 keeps a NULL pointer and dereferences
//    it (`mr r10, r27` with r27 == 0, then `lhz r8, 4(r10)`); that is reproduced, not
//    guarded -- adding a guard would add behaviour the binary does not have.
//  * The flag byte is `(muFlags >> 2) & 1` (srwi 2 / clrlwi 31) == the E_FLAG_RANK_WIN
//    (4) test.
//  * The post is AddEvent(record, 40, 40): channel 40 and record size 40 happen to share
//    a value here. Record = {24 payload bytes, id 464, payload at +16}.
//
// There are no float comparisons in this body, so there is no NaN-polarity decision.
// -------------------------------------------------------------------------------
void PreRaceFlyByState::SetRoadRageDescription()
{
    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:1208

    char lacParameterText[128];
    CgsCore::SPrintf(lacParameterText, sizeof(lacParameterText), "%d",
                     static_cast<s32>(mpGuiCache->GetRequiredScoreForMedal(
                         BrnGameState::E_CURRENT_MEDAL_TARGET_TIME_GOLD)));
    lacParameterText[sizeof(lacParameterText) - 1] = 0;

    maEventDescriptionText[0].SetLocalisedText(
        "PRE_ROADRAGE_PART1",
        CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
        1,
        lacParameterText,
        CgsLanguage::LanguageManager::E_FORMAT_INTEGER);

    for (s32 liLine = 1; liLine < KI_MAX_LINES_DESCRIPTION_TEXT; ++liLine)
    {
        maEventDescriptionText[liLine].SetText("");
    }

    const BrnProgression::Profile* lpProfile = mpGuiCache->GetProfile();

    GuiEventPreRaceDescription lDescription;
    lDescription.mLandmarkId        = 0;                          // no destination landmark
    lDescription.miDirection        = KI_COMPASS_DIRECTION_NONE;
    lDescription.miGameMode         = GSM::E_MODE_ROAD_RAGE;
    lDescription.miTimesPlayed      = lpProfile->GetNumWinsForGameMode(GSM::E_MODE_ROAD_RAGE)
                                    + lpProfile->GetNumLossesForGameMode(GSM::E_MODE_ROAD_RAGE);
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
    // The console dereferences this unconditionally -- a NULL read when the running event
    // has no profile record (see the note above).
    lDescription.mbEventFlag = static_cast<u8>(
        (lpProfileEvent->GetFlags() & BrnProgression::ProfileEvent::E_FLAG_RANK_WIN) != 0);

    mpStateInterface->GetOutputEventQueue()->AddEvent(&lDescription, KI_CHANNEL_GUI_OUT,
                                                     static_cast<s32>(sizeof(lDescription)));
}
}
