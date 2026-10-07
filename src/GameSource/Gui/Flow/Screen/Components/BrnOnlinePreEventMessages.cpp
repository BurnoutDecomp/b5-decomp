#include "BrnOnlinePreEventMessages.h"

#include <cstring>                                                  // std::strstr / std::strcmp
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"             // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // CgsGui::StateInterface / GuiAccessPointers
#include "GameSource/Gui/BrnGuiCache.h"                             // BrnGui::GuiCache
#include "GameSource/GameState/BrnGameStateSharedIO.h"             // EGameModeType

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnGui::OnlinePreEventMessages::SelectScreenKeyFrameForGameMode @ 0x8241A010
//   Construct / Show / Hide / HandleLoadNotification
//
// Walks the component's state interface -> access pointers -> gui cache to read the
// active game-mode type, picks the apt key-frame name, and pushes it onto the apt
// output view-state. The X360 re-reads / re-asserts the chain at each hop (the
// GetAccessPointers / GetGuiCache asserts are inlined into the caller); reproduced here
// through the by-name accessors with one assert per hop.

namespace BrnGui
{
const char OnlinePreEventMessages::KAC_STRING_TEXTFIELD_TEMPLATE[12] = "string%d_mc";

namespace
{
    const char KAC_APT_TRANSITION[]   = "apt_transition";
    const char KAC_TRANSITION_IN[]    = "transin";
    const char KAC_TRANSITION_OUT[]   = "transout";
    const char* const KPC_EMPTY_TEXT  = "";

    // The text-field name buffer Construct formats into (the console hands SPrintf one byte
    // less and clears the last byte up front).
    const u32 KU_STRING_NAME_LENGTH = 32;

    // The online modes that use the stunt-run key-frame (EGameModeType 12/14/17).
    bool UsesStuntRunKeyFrame(s32 liGameModeType)
    {
        namespace GM = BrnGameState::GameStateModuleIO;
        return liGameModeType == GM::E_MODE_ONLINE_FUGITIVE   // 12
            || liGameModeType == GM::E_MODE_ONLINE_FREE_BURN  // 14
            || liGameModeType == GM::E_MODE_ONLINE_MODE_END;  // 17
    }
}

int OnlinePreEventMessages::SelectScreenKeyFrameForGameMode()
{
    // Guest: assert the state interface is set (BrnOnlinePreEventMessages.h:116).
    CGS_ASSERT(mpStateInterface != 0, "mpStateInterface");

    // Guest: mpStateInterface->GetAccessPointers() -- asserted non-null both inside the
    // accessor (CgsGuiStateInterface.h:344 "mpAccessPointers != NULL") and at the call
    // site (BrnOnlinePreEventMessages.h:117).
    CgsGui::GuiAccessPointers* lpAccessPointers = mpStateInterface->GetAccessPointers();
    CGS_ASSERT(lpAccessPointers != 0, "mpAccessPointers != NULL");
    CGS_ASSERT(lpAccessPointers != 0, "mpStateInterface->GetAccessPointers()");

    // Guest: ...->GetGuiCache() -- asserted non-null inside the accessor
    // (CgsGuiShared.h:201 "mpGuiCache") and at the call site
    // (BrnOnlinePreEventMessages.h:118).
    BrnGui::GuiCache* lpGuiCache = lpAccessPointers->GetGuiCache();
    CGS_ASSERT(lpGuiCache != 0, "mpGuiCache");
    CGS_ASSERT(lpGuiCache != 0, "mpStateInterface->GetAccessPointers()->GetGuiCache()");

    // Guest: r11 = *(guiCache + 40536) -- the active game-mode type.
    const s32 liGameModeType = lpGuiCache->GetCurrentGameModeType();

    // Guest: v7 == 12 || v7 == 14 || v7 == 17 ? "anim1_StuntRun" : "anim1".
    const char* lpacViewState = UsesStuntRunKeyFrame(liGameModeType) ? "anim1_StuntRun" : "anim1";

    // Guest: CgsGui::GuiComponent::AddOutputAptViewState(this, "apt_state", v8, 0).
    AddOutputAptViewState("apt_state", lpacViewState, false);

    // AddOutputAptViewState is void in the committed base; the X360 r3 result the guest
    // returns is unused by the Show/Hide callers, so return 0.
    return 0;
}

// Build the component, then its three text fields "string0_mc".."string2_mc" under it.
void OnlinePreEventMessages::Construct(const char* lpacName,
                                       CgsGui::StateInterface* lpStateInterface,
                                       const char* lpacParentName)
{
    CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);

    char lacStringName[KU_STRING_NAME_LENGTH];
    lacStringName[KU_STRING_NAME_LENGTH - 1] = 0;
    for (s32 liIndex = 0; liIndex < KI_NUM_MESSAGE_STRINGS; ++liIndex)
    {
        CgsCore::SPrintf(lacStringName, KU_STRING_NAME_LENGTH - 1, KAC_STRING_TEXTFIELD_TEMPLATE,
                         liIndex);
        maString[liIndex].Construct(lacStringName, mpStateInterface, macName);
    }

    mbIsShowing = false;
}

// Fill each string from the entry's message ids (an id with a parameter takes it as plain
// text), blank the unused ones, and transition in.
void OnlinePreEventMessages::Show(const PreEventInfo* lpInfo)
{
    if (mbIsShowing)
    {
        return;
    }

    mbIsShowing = true;
    SelectScreenKeyFrameForGameMode();
    AddOutputAptViewState(KAC_APT_TRANSITION, KAC_TRANSITION_IN, false);

    s32 liIndex = 0;
    for (; liIndex < lpInfo->miNumMsgIDs; ++liIndex)
    {
        if (lpInfo->maiNumParams[liIndex] > 0)
        {
            CgsLanguage::LanguageManager::ParameterFormatType leParamFormat =
                CgsLanguage::LanguageManager::E_FORMAT_TEXT;
            const char* lpacParam = lpInfo->maacMessageParameters[liIndex];

            CGS_ASSERT(lpInfo->maiNumParams[liIndex] == 1, "lpInfo->maiNumParams[liIndex] == 1");

            maString[liIndex].SetLocalisedText(lpInfo->maacMessageIDs[liIndex],
                                               CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
                                               1, &lpacParam, &leParamFormat);
        }
        else
        {
            maString[liIndex].SetLocalisedText(lpInfo->maacMessageIDs[liIndex],
                                               CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
        }
    }

    for (; liIndex < KI_NUM_MESSAGE_STRINGS; ++liIndex)
    {
        maString[liIndex].SetText(KPC_EMPTY_TEXT);
    }
}

// Transition out.
void OnlinePreEventMessages::Hide()
{
    if (!mbIsShowing)
    {
        return;
    }

    mbIsShowing = false;
    SelectScreenKeyFrameForGameMode();
    AddOutputAptViewState(KAC_APT_TRANSITION, KAC_TRANSITION_OUT, false);
}

// A component under this one finished loading: re-push the matching string's text.
bool OnlinePreEventMessages::HandleLoadNotification(const char* lpacComponentName)
{
    if (std::strstr(lpacComponentName, macName) == 0)
    {
        return false;
    }

    for (s32 liIndex = 0; liIndex < KI_NUM_MESSAGE_STRINGS; ++liIndex)
    {
        if (std::strcmp(maString[liIndex].GetName(), lpacComponentName) == 0)
        {
            maString[liIndex].SetText(maString[liIndex].GetText());
            break;
        }
    }

    return true;
}
}
