#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"

#include <cstring>   // std::memset / std::strlen (the stunt-line buffer build)

#include "GameShared/GameClasses/Core/CgsAssert.h"              // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"         // CgsCore::StrCat (capped append)
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"            // CgsGui::GuiAccessPointers::mpLanguageManager
#include "GameShared/GameClasses/Language/CgsLanguageManager.h" // FindString / FormatTextFromInt / format enums
#include "GameSource/Gui/BrnGuiCache.h"                         // GuiCache (friend members + GetGameMode, wreck state machine)
#include "GameSource/GameState/BrnGameStateSharedIO.h"          // E_MODE_STUNT_ATTACK

// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (wave-C group wC_04, 3 ledger functions):
//   HudMessageAnalyzer::HandleTraitorousTakedown @0x8251B0E0
//   HudMessageAnalyzer::HandleWreckedEvent       @0x8251CA88
//   HudMessageAnalyzer::HandleStuntPerformed     @0x8251B608

namespace BrnGui
{

// @ 0x8251B0E0
// Online "traitorous takedown" (a team-mate slammed you): resolve the aggressor's
// online name and fire the "OnlRRTraitor" message with it -- but only when the name
// resolved (an invalid name adds an empty param and the message is suppressed).
void HudMessageAnalyzer::HandleTraitorousTakedown(const GuiTraitorousTakedownEvent* lpEvent) const
{
    CGS_ASSERT(lpEvent != NULL, "lpEvent");
    CGS_ASSERT(lpEvent->meAggActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "lpEvent->meAggActiveRaceCarIndex >= 0");
    CGS_ASSERT(lpEvent->meAggActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "lpEvent->meAggActiveRaceCarIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS");

    bool lbNameValid = true;

    GuiHudMessage lMessage;
    lMessage.Construct("OnlRRTraitor");

    const char* lpcOnlineName = GetOnlineName(lpEvent->meAggActiveRaceCarIndex, &lbNameValid);
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0, lpcOnlineName);

    if (lbNameValid)
    {
        TriggerMessage(&lMessage);
    }
}

// @ 0x8251CA88
// Wreck-duration state machine (E_WRECKED_STATE_*). The player-wrecked event carries
// no consumed field (the pointer is asserted then unused); the whole decision keys off
// the analyzer's own wreck-state member, the frame delta on the cache and -- on entry --
// the stunt-attack score:
//   NOT_WRECKED : stunt-attack mode with a positive last-stunt score arms the
//                 "StuntWrecked" lane, otherwise the ordinary "GameWrecked" lane;
//   WRECKING    : accumulate the wreck duration; hold until it passes the threshold;
//   WRECKED / WRECKED_STUNT : fire the message once the crash bar has dropped
//                 (meCrashEntryState == 0), then latch WRECK_HANDLED.
void HudMessageAnalyzer::HandleWreckedEvent(const GuiEventPlayerWrecked* lpEvent)
{
    CGS_ASSERT(lpEvent != NULL, "lpWreckedEvent");

    switch (mePlayersCurrentlyWreckState)
    {
        case E_WRECKED_STATE_NOT_WRECKED:
            if (mpGuiCache->GetGameMode() == BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK
                && mpGuiCache->miLastStuntScore > 0)
            {
                mePlayersCurrentlyWreckState = E_WRECKED_STATE_WRECKED_STUNT;
            }
            else
            {
                mePlayersCurrentlyWreckState = E_WRECKED_STATE_WRECKING;
            }
            return;

        case E_WRECKED_STATE_WRECKING:
            CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");
            mfCurrentWreckDuration += mpGuiCache->mfFrameDeltaTime;
            if (mfCurrentWreckDuration <= KF_REQUIRED_WRECK_DURATION)
            {
                return;
            }
            mePlayersCurrentlyWreckState = E_WRECKED_STATE_WRECKED;
            return;

        case E_WRECKED_STATE_WRECKED:
            if (meCrashEntryState != 0)
            {
                return;
            }
            {
                GuiHudMessage lMessage;
                lMessage.Construct("GameWrecked");
                TriggerMessage(&lMessage);
            }
            mePlayersCurrentlyWreckState = E_WRECKED_STATE_WRECK_HANDLED;
            return;

        case E_WRECKED_STATE_WRECKED_STUNT:
            if (meCrashEntryState != 0)
            {
                return;
            }
            {
                GuiHudMessage lMessage;
                lMessage.Construct("StuntWrecked");
                TriggerMessage(&lMessage);
            }
            mePlayersCurrentlyWreckState = E_WRECKED_STATE_WRECK_HANDLED;
            return;

        default:
            return;
    }
}

// @ 0x8251B608
// Skill/stunt chatter built from the stunt snapshot's flag masks. Two lanes:
//   * a failure lane (muStuntTypes bits 0x40000/0x80000 -> repetition / crashed;
//     either 0xC0000 bit set) fires a two-string "GenericBad" message;
//   * the scoring lane (positive multiplier) assembles a "StuntMulti" line by
//     concatenating one localised fragment per active awesome-stunt category
//     (air/spin/barrel-roll/leap/super-jump/smash/billboard/burnout/takedown/chain),
//     picking the singular key when the category count is <= 1 and the
//     FormatTextFromInt-rendered *_MULTIPLE key otherwise, then fires the composed
//     string plus the combined score as an int param.
// The 0x4000 "reverse" bit prefixes a "REVERSE" fragment onto the spin/roll/leap
// categories (and swaps AIR for REVERSE_AIR).
void HudMessageAnalyzer::HandleStuntPerformed(const GuiHUDMessageStuntPerformed* lpEvent)
{
    CgsLanguage::LanguageManager* lpLanguageManager = mpAccessPointers->mpLanguageManager;

    char lacBuffer[128];
    lacBuffer[0] = 0;
    std::memset(&lacBuffer[1], 0, 127);

    // Resolve a localisation key to its stored text, falling back to the raw key when
    // the database has no entry (FindString returns NULL).
    auto lResolveString = [lpLanguageManager](const char* lpcKey) -> const char*
    {
        const u8* lpcString = lpLanguageManager->FindString(lpcKey);
        return lpcString ? reinterpret_cast<const char*>(lpcString) : lpcKey;
    };

    GuiHudMessage lMessage;

    // ---- failure lane (repetition / crashed / plain fail) ----
    if ((lpEvent->mStuntInfo.muStuntTypes & 0xC0000) != 0)
    {
        lMessage.Construct("GenericBad");

        const char* lpcTitle = lResolveString("STUNT_RUN_MULTIPLIER_STUNT_FAILED");

        const char* lpcReason = lacBuffer;   // empty fragment unless a reason bit is set
        if ((lpEvent->mStuntInfo.muStuntTypes & 0x40000) != 0)
        {
            lpcReason = lResolveString("STUNT_RUN_MULTIPLIER_REPETITION");
        }
        else if ((lpEvent->mStuntInfo.muStuntTypes & 0x80000) != 0)
        {
            lpcReason = lResolveString("STUNT_RUN_MULTIPLIER_CRASHED");
        }

        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0, lpcTitle);
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1, lpcReason);
        TriggerMessage(&lMessage);
        return;
    }

    // ---- scoring lane ----
    if (lpEvent->mStuntInfo.miStuntMultiplier > 0)
    {
        lMessage.Construct("StuntMulti");

        const bool lbReverse = (lpEvent->mStuntInfo.muStuntTypes & 0x4000) != 0;

        char lacFormatBuffer[64];

        // air (awesome bit 0x4) -- a single fragment, reversed variant swaps the key
        if ((lpEvent->mStuntInfo.muAwesomeStuntTypes & 0x4) != 0)
        {
            CgsCore::StrCat(lacBuffer, 0x80,
                            lResolveString(lbReverse ? "STUNT_RUN_MULTIPLIER_REVERSE_AIR"
                                                     : "STUNT_RUN_MULTIPLIER_AIR"));
        }

        // spin (awesome bit 0x1) -- optional REVERSE prefix + singular/plural count
        if ((lpEvent->mStuntInfo.muAwesomeStuntTypes & 0x1) != 0)
        {
            if (lbReverse)
            {
                CgsCore::StrCat(lacBuffer, 0x80, lResolveString("STUNT_RUN_MULTIPLIER_REVERSE"));
            }

            if (lpEvent->mStuntInfo.muFlatSpins <= 1)
            {
                CgsCore::StrCat(lacBuffer, 0x80, lResolveString("STUNT_RUN_MULTIPLIER_SPIN_SINGLE"));
            }
            else
            {
                lpLanguageManager->FormatTextFromInt(
                    lacFormatBuffer, 63, "STUNT_RUN_MULTIPLIER_SPIN_MULTIPLE",
                    CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
                    lpEvent->mStuntInfo.muFlatSpins,
                    CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
                CgsCore::StrCat(lacBuffer, 0x80, lacFormatBuffer);
            }
        }

        // barrel roll (awesome bit 0x2)
        if ((lpEvent->mStuntInfo.muAwesomeStuntTypes & 0x2) != 0)
        {
            if (lbReverse)
            {
                CgsCore::StrCat(lacBuffer, 0x80, lResolveString("STUNT_RUN_MULTIPLIER_REVERSE"));
            }

            if (lpEvent->mStuntInfo.muBarrelRolls <= 1)
            {
                CgsCore::StrCat(lacBuffer, 0x80, lResolveString("STUNT_RUN_MULTIPLIER_BARREL_ROLL_SINGLE"));
            }
            else
            {
                lpLanguageManager->FormatTextFromInt(
                    lacFormatBuffer, 63, "STUNT_RUN_MULTIPLIER_BARREL_ROLL_MULTIPLE",
                    CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
                    lpEvent->mStuntInfo.muBarrelRolls,
                    CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
                CgsCore::StrCat(lacBuffer, 0x80, lacFormatBuffer);
            }
        }

        // leap car (awesome bit 0x10000)
        if ((lpEvent->mStuntInfo.muAwesomeStuntTypes & 0x10000) != 0)
        {
            if (lbReverse)
            {
                CgsCore::StrCat(lacBuffer, 0x80, lResolveString("STUNT_RUN_MULTIPLIER_REVERSE"));
            }

            if (lpEvent->mu16TailCount16 <= 1)
            {
                CgsCore::StrCat(lacBuffer, 0x80, lResolveString("STUNT_RUN_MULTIPLIER_LEAP_CAR_SINGLE"));
            }
            else
            {
                lpLanguageManager->FormatTextFromInt(
                    lacFormatBuffer, 63, "STUNT_RUN_MULTIPLIER_LEAP_CAR_MULTIPLE",
                    CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
                    lpEvent->mu16TailCount16,
                    CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
                CgsCore::StrCat(lacBuffer, 0x80, lacFormatBuffer);
            }
        }

        // super jump (awesome bit 0x10)
        if ((lpEvent->mStuntInfo.muAwesomeStuntTypes & 0x10) != 0)
        {
            CgsCore::StrCat(lacBuffer, 0x80, lResolveString("STUNT_RUN_MULTIPLIER_SUPER_JUMP"));
        }

        // smash (awesome bit 0x20)
        if ((lpEvent->mStuntInfo.muAwesomeStuntTypes & 0x20) != 0)
        {
            CgsCore::StrCat(lacBuffer, 0x80, lResolveString("STUNT_RUN_MULTIPLIER_SMASH"));
        }

        // billboard (awesome bit 0x40)
        if ((lpEvent->mStuntInfo.muAwesomeStuntTypes & 0x40) != 0)
        {
            CgsCore::StrCat(lacBuffer, 0x80, lResolveString("STUNT_RUN_MULTIPLIER_BILLBOARD"));
        }

        // burnout (awesome bit 0x80)
        if ((lpEvent->mStuntInfo.muAwesomeStuntTypes & 0x80) != 0)
        {
            CgsCore::StrCat(lacBuffer, 0x80, lResolveString("STUNT_RUN_MULTIPLIER_BURNOUT"));
        }

        // takedown (awesome bit 0x8000)
        if ((lpEvent->mStuntInfo.muAwesomeStuntTypes & 0x8000) != 0)
        {
            CgsCore::StrCat(lacBuffer, 0x80, lResolveString("STUNT_RUN_MULTIPLIER_TAKEDOWN"));
        }

        // chain (the tail count) -- always plural-formatted
        if (lpEvent->mu16TailCount14 != 0)
        {
            lpLanguageManager->FormatTextFromInt(
                lacFormatBuffer, 63, "STUNT_RUN_MULTIPLIER_CHAIN",
                CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
                lpEvent->mu16TailCount14,
                CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
            CgsCore::StrCat(lacBuffer, 0x80, lacFormatBuffer);
        }

        CGS_ASSERT(std::strlen(lacBuffer) > 3, "strlen(lacBuffer) > 3");

        const s32 liChainScore = lpEvent->mu16TailCount14 + lpEvent->mStuntInfo.miStuntMultiplier;
        lacBuffer[std::strlen(lacBuffer) - 3] = 0;   // trim the trailing category separator

        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 0, liChainScore);
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1, lacBuffer);
        TriggerMessage(&lMessage);
    }
}

}
