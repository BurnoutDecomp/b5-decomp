#include "GameSource/Gui/Flow/HUD/Components/BrnPlayerPositionTable.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::OutputGuiEvent
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiAudioTriggerEvent
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"                    // MovieClipRef::FindChildMovieClipOnFrame / FindChildTextField
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"                    // TextFieldRef::SetText

// ============================================================================
// GameSource/Gui/Flow/HUD/Components/BrnPlayerPositionTable_wP0_01.cpp
//
// PlayerPositionTableComponent -- the two title-bar text setters, split out of
// the base TU so the base stays the reviewed 17/17 slice. Both are called from
// the base TU's SetupGameMode (and SetSkillsText additionally from
// ProcessOnlineFreeburnTable), which is why they are the mount's class-A holes.
//
// Both do the same thing: re-resolve a named child clip of mTitleBarMCR on its
// CURRENT frame, take the named text field inside it, latch that ref into the
// component's own member, then push the caller's string through it. The re-lookup
// is not redundant on console -- the title bar's frame changes with the game mode,
// so the field handle is only valid for the frame that is playing now.
// ============================================================================

namespace BrnGui
{
    // ------------------------------------------------ SetTitleText
    // Rebind mTitleText from mTitleBarMCR's "TitleText" child clip ("titleText_txt"
    // inside it), fire the "CodeTodaysBestFlips" GUI audio trigger, then set the text.
    //
    // The audio post is the compiler-inlined StateInterface::OutputGuiEvent<
    // GuiAudioTriggerEvent> (the console stack-builds the 112-byte record
    // {100, 457, 12} + the 100-byte payload and hands it to the out queue's AddEvent
    // on channel 40 -- exactly what the committed template emits for this type).
    // The component/movie names are the shared empty string in .rdata; the action
    // byte is 7, the same value every GuiAudioTriggerEvent::Construct site uses.
    void PlayerPositionTableComponent::SetTitleText(const char* lpcText)
    {
        BrnFlapt::MovieClipRef lTitleTextClipRef;
        const BrnFlapt::MovieClipRef lTitleTextClip =
            *mTitleBarMCR.FindChildMovieClipOnFrame(&lTitleTextClipRef, "TitleText");

        BrnFlapt::TextFieldRef lTitleTextFieldRef;
        mTitleText = *lTitleTextClip.FindChildTextField(&lTitleTextFieldRef, "titleText_txt");

        GuiAudioTriggerEvent lAudio;
        lAudio.Construct(7, "", "CodeTodaysBestFlips", "");
        mpStateInterface->OutputGuiEvent<GuiAudioTriggerEvent>(lAudio);

        mTitleText.SetText(lpcText, false);
    }

    // ------------------------------------------------ SetSkillsText
    // The same rebind against mTitleBarMCR's "SkillzText" child clip ("skillz_txt"
    // inside it) into mSkillzText, then set the text. No audio trigger on this one.
    void PlayerPositionTableComponent::SetSkillsText(const char* lpcText)
    {
        BrnFlapt::MovieClipRef lSkillsTextClipRef;
        const BrnFlapt::MovieClipRef lSkillsTextClip =
            *mTitleBarMCR.FindChildMovieClipOnFrame(&lSkillsTextClipRef, "SkillzText");

        BrnFlapt::TextFieldRef lSkillsTextFieldRef;
        mSkillzText = *lSkillsTextClip.FindChildTextField(&lSkillsTextFieldRef, "skillz_txt");

        mSkillzText.SetText(lpcText, false);
    }
}
