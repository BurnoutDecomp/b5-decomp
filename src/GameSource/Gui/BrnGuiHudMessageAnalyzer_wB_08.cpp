#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"   // brings BrnGuiEventTypeDefs + GuiHudMessage

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"            // CgsIDCompress / CgsIDConvertToString
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf
#include "GameSource/Gui/BrnGuiCache.h"                   // GetHudMessageDirector / GetFreeburnChallengeManager
#include "GameSource/Gui/BrnGuiHudMessageDirector.h"      // HudMessageDirector::IsMessageAllowed
#include "GameSource/Gui/BrnGuiFreeburnChallengeManager.h" // FreeburnChallengeManager::IsActive / GetCurrentChallenge

// BrnGui::HudMessageAnalyzer -- challenge / trophy deferred-fire handlers,
// reconstructed from BURNOUT_X360_ARTIST.XEX (wave-B group 08).
//
// Bodied here:
//   HudMessageAnalyzer::HandleChallengeEnded              @0x824F33E0
//   HudMessageAnalyzer::HandleTrophyCarUnlocked           @0x8251F8B0
//   HudMessageAnalyzer::TriggerChallengeTriggeredMessage  @0x8251F970

namespace BrnGui
{

// @ 0x824F33E0 -- park the just-ended challenge's 24-byte record so Update's deferred
// pass can fire the "challenge ended" message. Only the success/aborted/failure
// statuses arm it; the abort-before-start status is ignored, and anything else is an
// unhandled-status tripwire.
void HudMessageAnalyzer::HandleChallengeEnded(const GuiChallengeEndEvent* lpEvent)
{
    switch (lpEvent->meChallengeStatus)
    {
    case BrnGameState::E_CHALLENGE_STATUS_SUCCESS:                     // 1
    case BrnGameState::E_CHALLENGE_STATUS_ABORTED:                     // 2
    case BrnGameState::E_CHALLENGE_STATUS_ABORTED_DUE_TO_PLAYER_LEAVE: // 3
    case BrnGameState::E_CHALLENGE_STATUS_FAILURE:                     // 6
        mbChallengeEnded    = true;
        mChallengedEndedData = *lpEvent;
        break;

    case BrnGameState::E_CHALLENGE_STATUS_ABORTED_BEFORE_STARTING:     // 4
        break;

    default:
        // X360 streamed assert ("Unknown Challenge Status " << status << "\n"),
        // folded to a static tripwire (cpp:5280).
        CGS_ASSERT(false, "Unknown Challenge Status ");
        break;
    }
}

// @ 0x8251F8B0 -- "trophy car unlocked" announce; prints the unlocked car's id via
// the "CAR_CAPS_<id>" string-id convention. Called from Update's deferred trophy pass
// with &mTrophyCarUnlockedEvent.
void HudMessageAnalyzer::HandleTrophyCarUnlocked(const GuiEventTrophyCarUnlock* lpEvent)
{
    CGS_ASSERT(lpEvent != NULL, "lpTrophyUnlockedEvent");   // cpp:5190

    GuiHudMessage lMessage;
    lMessage.Construct("CarAward");

    char lacCarIdBuffer[16];
    CgsIDConvertToString(lpEvent->mTrophyCarID, lacCarIdBuffer);
    lacCarIdBuffer[12] = '\0';                              // asm @0x8251F910

    char lacCarNameBuffer[32];
    CgsCore::SPrintf(lacCarNameBuffer, 31, "CAR_CAPS_%s", lacCarIdBuffer);
    lacCarNameBuffer[31] = '\0';                            // asm @0x8251F938

    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 0, lacCarNameBuffer);
    TriggerMessage(&lMessage);
}

// @ 0x8251F970 -- fire the parked "challenge triggered" message. Gated on the message
// director still permitting the id and the freeburn-challenge manager being active
// (initialised / running / results); the current challenge's title string-id tags the
// message. The pending flag is always cleared once the id is allowed.
void HudMessageAnalyzer::TriggerChallengeTriggeredMessage()
{
    const CgsID lMessageIdHash = CgsIDCompress("FBChalTrig");

    if (mpGuiCache->GetHudMessageDirector()->IsMessageAllowed(lMessageIdHash))
    {
        if (mpGuiCache->GetFreeburnChallengeManager()->IsActive())
        {
            GuiHudMessage lMessage;
            lMessage.Construct(lMessageIdHash);

            const BrnResource::ChallengeListEntry* lpChallenge =
                mpGuiCache->GetFreeburnChallengeManager()->GetCurrentChallenge();
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 1,
                              lpChallenge->GetTitleStringID());
            TriggerMessage(&lMessage);
        }

        mbChallengeTriggered = false;
    }
}

} // namespace BrnGui
