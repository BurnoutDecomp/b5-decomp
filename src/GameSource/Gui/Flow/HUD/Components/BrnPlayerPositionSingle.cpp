#include "GameSource/Gui/Flow/HUD/Components/BrnPlayerPositionSingle.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf/SnPrintf
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // GetLanguageManager
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // FormatSecondsAndHundredsString + formats
#include "GameSource/Gui/BrnGuiCache.h"                                   // GuiCache (mode/road-rule/skills reads)
#include "GameSource/Gui/BrnGuiFreeburnChallengeManager.h"                // FreeburnChallengeManager
#include "GameSource/Gui/BrnGuiBurnoutSkillsManager.h"                    // BurnoutSkillsManager::GetCurrentSkill
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"                         // FileRef::FindComponent
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h"               // MovieClipInstance::ResetTimeline
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponentUtils.h" // AttachToTextFieldComponent

// BrnGui::PlayerPositionSingleComponent -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (3 ledger functions, DWARF primary file
// GameSource/Gui/Flow/Hud/Components/BrnPlayerPositionSingle.cpp):
//   PlayerPositionSingleComponent::Construct   @0x82413C58
//   PlayerPositionSingleComponent::Prepare     @0x82421D78
//   PlayerPositionSingleComponent::RenderValue @0x82421F78
// plus IsChallengeWow / IsTodaysBestWow (DWARF cpp:868/cpp:889; no standalone X360
// symbols -- recovered from their inlined instances inside RenderValue).
//
// The two "wow" limit tables are read from the XEX .rodata (KAF_CHALLENGE_WOW_LIMITS
// @0x82F25680, KAF_TODAYS_BEST_WOW_LIMITS @0x82F2564C; FLT_MAX entries mean "never
// wow" for that type).

namespace BrnGui
{
namespace
{
    // The value-format selectors RenderValue passes (raw
    // CgsLanguage::LanguageManager::ParameterFormatType integers, per the
    // TextFieldRef house style): 2 = minutes:seconds.hundredths, 11 = integer,
    // 13 = percentage, 14 = money, 15 = auto distance, 17 = small distance.
    const s32 KI_FORMAT_MINS_SECS_HUNDREDTHS = 2;
    const s32 KI_FORMAT_INTEGER              = 11;
    const s32 KI_FORMAT_PERCENTAGE           = 13;
    const s32 KI_FORMAT_MONEY                = 14;
    const s32 KI_FORMAT_AUTO_DISTANCE        = 15;
    const s32 KI_FORMAT_SMALL_DISTANCE       = 17;

    // The loc-string id type every lookup here uses.
    const s32 KI_STRINGID_TYPE = 9;

    // Values below this show as "no value" (XEX flt @0x82004014).
    const f32 KF_ZERO_FLOAT_TOLERANCE = 0.1f;
}

const f32 PlayerPositionSingleComponent::KAF_CHALLENGE_WOW_LIMITS[24] =
{
    100.0f, 1000.0f, 205920.0f, 88000.0f, 3600.0f, 109120.0f, 1000.0f, 36000.0f,
    100.0f, 3.4028235e38f, 3.4028235e38f, 1000.0f, 1000.0f, 3.4028235e38f,
    3.4028235e38f, 100.0f, 1000.0f, 3600.0f, 205920.0f, 205920.0f, 42.0f, 1000.0f,
    3.4028235e38f, 1000.0f,
};

const f32 PlayerPositionSingleComponent::KAF_TODAYS_BEST_WOW_LIMITS[12] =
{
    3600.0f, 1000.0f, 1000.0f, 88000.0f, 36000.0f, 109120.0f, 1000.0f, 205920.0f,
    3.4028235e38f, 3.4028235e38f, 3.4028235e38f, 3.4028235e38f,
};

// DWARF cpp:868 -- recovered from the inline instance @0x8242212C.
bool PlayerPositionSingleComponent::IsChallengeWow(s32 leDataType, f32 lfValue) const
{
    return lfValue > KAF_CHALLENGE_WOW_LIMITS[leDataType];
}

// DWARF cpp:889 -- recovered from the inline instance @0x8242241C.
bool PlayerPositionSingleComponent::IsTodaysBestWow(s32 leSkillType, f32 lfValue) const
{
    return lfValue > KAF_TODAYS_BEST_WOW_LIMITS[leSkillType];
}

// @ 0x82413C58
void PlayerPositionSingleComponent::Construct(const char* /*lpacName*/,
                                              CgsGui::StateInterface* lpStateInterface,
                                              const char* /*lpacParentName*/)
{
    // The inlined BrnFlaptComponent construct (assert BrnGuiFlaptComponent.h:113,
    // non-gating; store the interface; drop the clip handle).
    CGS_ASSERT(lpStateInterface != NULL, "lpStateInterface");
    mpStateInterface = lpStateInterface;
    mAptRef.SetInvalid();

    mePlayerType         = E_PLAYERTYPES_INVISIBLE;
    meActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(-1);
    meHeadsetStatus      = E_HEADSETSTATUS_INVISIBLE;
    mePlayerColour       = 0;
    meRevengeStatus      = E_REVENGESTATUS_INVISIBLE;
    meAwardState         = E_AWARDSTATUS_NONE;
    mbValueChanged       = true;
    mpCache              = NULL;

    mpLanguageManager = mpStateInterface->GetLanguageManager();
    CGS_ASSERT(mpLanguageManager != NULL, "mpLanguageManager");   // cpp:202, non-gating

    // The two colour animators (virtual Construct; the X360 passes no names).
    mPlayerColourAnimator.Construct(NULL, mpStateInterface, NULL);
    mPlayerTextColourAnimator.Construct(NULL, mpStateInterface, NULL);

    mRevenge.SetInvalid();
    mAudio.SetInvalid();
    mName.SetInvalid();
    mValue.SetInvalid();
}

// @ 0x82421D78
void PlayerPositionSingleComponent::Prepare(const char* lacName, const BrnFlapt::FileRef& lFile)
{
    // Non-gating tripwire (the inlined flapt-component prepare head,
    // BrnGuiFlaptComponent.h:133).
    CGS_ASSERT(lacName != NULL, "lacName != NULL");

    // Bind the row's own clip and rewind it.
    BrnFlapt::MovieClipRef lFound;
    mAptRef = *lFile.FindComponent(&lFound, lacName);
    CGS_ASSERT(mAptRef.mpMovieClipInst != NULL, "mpMovieClipInst");  // BrnFlaptMovieClipRef.h:272
    mAptRef.mpMovieClipInst->ResetTimeline();

    // The two colour animators live under "<row>_PlayerColour_anim" /
    // "<row>_TextColour_anim" (127-char scratch, hard NUL at the end).
    char lacChildName[128];
    CgsCore::SnPrintf(lacChildName, 127, "%s_%s", lacName, "PlayerColour_anim");
    lacChildName[127] = 0;
    mPlayerColourAnimator.Prepare(lacChildName, lFile, NULL);

    CgsCore::SnPrintf(lacChildName, 127, "%s_%s", lacName, "TextColour_anim");
    lacChildName[127] = 0;
    mPlayerTextColourAnimator.Prepare(lacChildName, lFile, NULL);

    // The two icons on the row's current frame.
    mRevenge = *mAptRef.FindChildMovieClipOnFrame(&lFound, "RevengeIcon_mc");
    mAudio   = *mAptRef.FindChildMovieClipOnFrame(&lFound, "AudioIcon_mc");

    // The two text fields (each resolved through its "_mc" wrapper).
    BrnFlapt::TextFieldRef lTextField;
    mName  = *AttachToTextFieldComponent(&lTextField, "PlayerText_txt", "PlayerText_mc",
                                         lacName, lFile);
    mValue = *AttachToTextFieldComponent(&lTextField, "DistanceText_txt", "DistanceText_mc",
                                         lacName, lFile);
}

// @ 0x82421F78 -- format mfValue into the value text field, per game mode.
void PlayerPositionSingleComponent::RenderValue()
{
    if (!mbValueChanged)
        return;
    mbValueChanged = false;

    const s32 liGameMode = mpCache->GetGameMode();

    // The two freeburn-challenge game modes (15/16) show either the live challenge
    // value or the today's-best skill value.
    if (liGameMode == 15 || liGameMode == 16)
    {
        const FreeburnChallengeManager* lpChallengeManager =
            mpCache->GetFreeburnChallengeManager();

        // A live road rule reroutes the row to the today's-best display.
        if (mpCache->GetActiveRoadRule() == 0 &&
            (lpChallengeManager->IsRunning() || lpChallengeManager->IsShowingResults()))
        {
            // ---- the live-challenge display ----
            if (mfValue < KF_ZERO_FLOAT_TOLERANCE)
            {
                mValue.ClearText();
                return;
            }
            if (!(lpChallengeManager->GetTargetsCount() > 0))
                return;

            const s32 leTargetType = lpChallengeManager->GetCurrentTargetType();
            if (IsChallengeWow(leTargetType, mfValue))
            {
                mValue.SetLocalisedText("BURNOUT_PLAYER_LIST_WOW", KI_STRINGID_TYPE);
                return;
            }

            switch (leTargetType)
            {
            case 0:   // crashes
                if (mfValue == 1.0f)
                    mValue.SetLocalisedText("BURNOUT_SKILLS_CRASH", KI_STRINGID_TYPE);
                else
                    mValue.SetLocalisedText("BURNOUT_SKILLS_CRASH_X", KI_STRINGID_TYPE,
                                            mfValue, KI_FORMAT_INTEGER);
                break;
            case 1:   // near misses
                if (mfValue == 1.0f)
                    mValue.SetLocalisedText("BURNOUT_SKILLS_MISS", KI_STRINGID_TYPE);
                else
                    mValue.SetLocalisedText("BURNOUT_SKILLS_MISS_X", KI_STRINGID_TYPE,
                                            mfValue, KI_FORMAT_INTEGER);
                break;
            case 6:   // barrel rolls
                if (mfValue == 1.0f)
                    mValue.SetLocalisedText("BURNOUT_SKILLS_ROLL", KI_STRINGID_TYPE);
                else
                    mValue.SetLocalisedText("BURNOUT_SKILLS_ROLL_X", KI_STRINGID_TYPE,
                                            mfValue, KI_FORMAT_INTEGER);
                break;
            case 8:   // cars taken down
                if (mfValue == 1.0f)
                    mValue.SetLocalisedText("BURNOUT_SKILLS_CAR", KI_STRINGID_TYPE);
                else
                    mValue.SetLocalisedText("BURNOUT_SKILLS_CARS_X", KI_STRINGID_TYPE,
                                            mfValue, KI_FORMAT_INTEGER);
                break;
            case 9:
                mValue.SetLocalisedText(mfValue, KI_FORMAT_MINS_SECS_HUNDREDTHS);
                break;
            case 10:
                mValue.SetLocalisedText(mfValue, KI_FORMAT_MONEY);
                break;
            case 11: case 16: case 18: case 20: case 21: case 22: case 23:
                mValue.SetLocalisedText(mfValue, KI_FORMAT_INTEGER);
                break;
            case 12:  // burnouts
                if (mfValue == 1.0f)
                    mValue.SetLocalisedText("BURNOUT_SKILLS_BURNOUT", KI_STRINGID_TYPE);
                else
                    mValue.SetLocalisedText("STAT_BURNOUTS", KI_STRINGID_TYPE,
                                            mfValue, KI_FORMAT_INTEGER);
                break;
            case 13:
                mValue.SetLocalisedText("BURNOUT_SKILLS_RATING", KI_STRINGID_TYPE,
                                        mfValue, KI_FORMAT_INTEGER);
                break;
            case 14:
                mValue.SetLocalisedText(mfValue, KI_FORMAT_PERCENTAGE);
                break;
            case 2: case 3: case 5: case 19:
                mValue.SetLocalisedText(mfValue, KI_FORMAT_SMALL_DISTANCE);
                break;
            case 4: case 17:
            {
                char lacTime[128];
                mpLanguageManager->FormatSecondsAndHundredsString(lacTime, mfValue, 64);
                mValue.SetLocalisedText("STAT_SECONDS", KI_STRINGID_TYPE, 1, lacTime, 0);
                break;
            }
            case 7:
                mValue.SetLocalisedText("STAT_DEGREES", KI_STRINGID_TYPE,
                                        mfValue, KI_FORMAT_INTEGER);
                break;
            default:  // case 15 + out-of-range
                mValue.ClearText();
                break;
            }
            return;
        }

        // ---- the today's-best display ----
        const s32 leSkillType = mpCache->GetBurnoutSkillsManager()->GetCurrentSkill();
        if (leSkillType <= 11 && IsTodaysBestWow(leSkillType, mfValue))
        {
            mValue.SetLocalisedText("BURNOUT_PLAYER_LIST_WOW", KI_STRINGID_TYPE);
            return;
        }

        switch (leSkillType)
        {
        case 0:
        {
            char lacTime[128];
            mpLanguageManager->FormatSecondsAndHundredsString(lacTime, mfValue, 64);
            mValue.SetLocalisedText("STAT_SECONDS", KI_STRINGID_TYPE, 1, lacTime, 0);
            break;
        }
        case 1:   // barrel rolls
            if (mfValue == 1.0f)
                mValue.SetLocalisedText("BURNOUT_SKILLS_ROLL", KI_STRINGID_TYPE);
            else
                mValue.SetLocalisedText("BURNOUT_SKILLS_ROLL_X", KI_STRINGID_TYPE,
                                        mfValue, KI_FORMAT_INTEGER);
            break;
        case 2:   // burnouts
            if (mfValue == 1.0f)
                mValue.SetLocalisedText("BURNOUT_SKILLS_BURNOUT", KI_STRINGID_TYPE);
            else
                mValue.SetLocalisedText("STAT_BURNOUTS", KI_STRINGID_TYPE,
                                        mfValue, KI_FORMAT_INTEGER);
            break;
        case 3: case 5: case 7:
            mValue.SetLocalisedText(mfValue, KI_FORMAT_SMALL_DISTANCE);
            break;
        case 4:
            mValue.SetLocalisedText("STAT_DEGREES", KI_STRINGID_TYPE,
                                    mfValue, KI_FORMAT_INTEGER);
            break;
        case 6:   // near misses
            if (mfValue == 1.0f)
                mValue.SetLocalisedText("BURNOUT_SKILLS_MISS", KI_STRINGID_TYPE);
            else
                mValue.SetLocalisedText("BURNOUT_SKILLS_MISS_X", KI_STRINGID_TYPE,
                                        mfValue, KI_FORMAT_INTEGER);
            break;
        case 8:
            mValue.SetLocalisedText("BURNOUT_SKILLS_RATING", KI_STRINGID_TYPE,
                                    mfValue, KI_FORMAT_INTEGER);
            break;
        case 9: case 10:
            mValue.SetLocalisedText(mfValue, KI_FORMAT_INTEGER);
            break;
        case 11:
        {
            if (mfValue <= KF_ZERO_FLOAT_TOLERANCE)
            {
                mValue.SetLocalisedText("KEEP_TRYING", KI_STRINGID_TYPE);
                break;
            }
            char lacCount[128];
            CgsCore::SPrintf(lacCount, 127, "%.f", static_cast<f64>(mfValue));
            lacCount[127] = 0;
            mValue.SetLocalisedText("AHEAD_IN_X", KI_STRINGID_TYPE, 1,
                                    lacCount, KI_FORMAT_INTEGER);
            break;
        }
        case 12:
            if (mfValue < KF_ZERO_FLOAT_TOLERANCE)
                mValue.SetLocalisedText("NO_TIME_SET", KI_STRINGID_TYPE);
            else
                mValue.SetLocalisedText(mfValue, KI_FORMAT_MINS_SECS_HUNDREDTHS);
            break;
        case 13:
            if (mfValue < KF_ZERO_FLOAT_TOLERANCE)
                mValue.SetLocalisedText("NO_RECORD_SET", KI_STRINGID_TYPE);
            else
                mValue.SetLocalisedText(mfValue, KI_FORMAT_MONEY);
            break;
        default:  // road-rule score types
            if (mfValue == 1.0f)
                mValue.SetLocalisedText("BURNOUT_SKILLS_RULE", KI_STRINGID_TYPE);
            else
                mValue.SetLocalisedText("BURNOUT_SKILLS_RULE_X", KI_STRINGID_TYPE,
                                        mfValue, KI_FORMAT_INTEGER);
            break;
        }
        return;
    }

    // ---- the plain game modes ----
    switch (liGameMode)
    {
    case 12: case 14: case 17:
        mValue.SetLocalisedText(mfValue, KI_FORMAT_INTEGER);
        break;

    case 13:  // hunter/chased: chasers show the label, others "laps done/total"
        if (mpCache->GetCurrentOnlinePlayerTeam(meActiveRaceCarIndex) == 1)
        {
            mValue.SetLocalisedText("BHR_CHASER", KI_STRINGID_TYPE);
        }
        else
        {
            char lacProgress[128];
            CgsCore::SPrintf(lacProgress, 127, "%.f/%d", static_cast<f64>(mfValue),
                             static_cast<s32>(mpCache->GetCheckpointsInEvent()));
            lacProgress[127] = 0;
            mValue.SetText(lacProgress, false);
        }
        break;

    default:
        if (mfValue < KF_ZERO_FLOAT_TOLERANCE)
            mValue.ClearText();
        else
            mValue.SetLocalisedText(mfValue, KI_FORMAT_AUTO_DISTANCE);
        break;
    }
}
}
