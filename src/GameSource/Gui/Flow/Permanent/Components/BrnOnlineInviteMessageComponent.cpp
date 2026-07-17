#include "BrnOnlineInviteMessageComponent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h" // CGS_ASSERT
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponentUtils.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnGui::OnlineInviteMessageComponent::DoTransitionComplete @ 0x824162E0
//
// The transition-out completion handler: assert the component was transitioning out,
// snapshot the secondary state, reset the showing-state to HIDDEN, and -- if the
// secondary state was 4 -- latch the dismissed flag (a byte store, hence a bool field).

namespace BrnGui
{
namespace
{
    // The secondary-state value (+0x34) that means the message should latch as dismissed.
    const char* const KAC_NOTIFY_TYPE_TEXTFIELD_NAME = "NotifyType_txt";
    const char* const KAC_NOTIFY_TYPE_COMPONENT_NAME = "NotifyType_mc";
    const char* const KAC_PLAYER_NAME_TEXTFIELD_NAME = "PlayerName_txt";
    const char* const KAC_PLAYER_NAME_COMPONENT_NAME = "PlayerName_mc";
}

void OnlineInviteMessageComponent::Construct(
    const char* lpcMovieClipName, CgsGui::StateInterface* lpStateInterface,
    const char* lpcParentName)
{
    (void)lpcMovieClipName;
    (void)lpcParentName;
    BrnFlaptComponent::Construct(lpStateInterface);
    mpGuiCache = 0;
    maTextFields[0].SetInvalid();
    maTextFields[1].SetInvalid();
    mfTimeToRemoveCurrentMessage = 0.0f;
    mfTimeToAllowMessages = 0.0f;
    meShowingState = E_SHOWINGSTATE_INVISIBLE;
    meShowingType = E_ONLINENOTIFICATIONCHYRONTYPE_INVITE_RECEIVED;
    meMessagesAvailable = E_MESSAGING_AVAILABLE_STATE_UNAVAILABLE;
    mbGameStarted = false;
    meQueuedType = E_ONLINENOTIFICATIONCHYRONTYPE_NONE;
    for (u32 luChar = 0; luChar < sizeof(macQueuedName); ++luChar)
        macQueuedName[luChar] = 0;
    miQueuedCount = 0;
    mbOutputFinishEvent = false;
}

void OnlineInviteMessageComponent::Prepare(
    const char* lpcMovieClipName, const BrnFlapt::FileRef& lFile)
{
    BrnFlaptComponent::Prepare(lpcMovieClipName, lFile, 0);
    mAptRef.SetFrameTriggerCallback(
        reinterpret_cast<void*>(&TransitionCompleteCallback), this);

    BrnFlapt::TextFieldRef lTextField;
    maTextFields[0] = *AttachToTextFieldComponent(
        &lTextField, KAC_NOTIFY_TYPE_TEXTFIELD_NAME,
        KAC_NOTIFY_TYPE_COMPONENT_NAME, lpcMovieClipName, lFile);
    maTextFields[1] = *AttachToTextFieldComponent(
        &lTextField, KAC_PLAYER_NAME_TEXTFIELD_NAME,
        KAC_PLAYER_NAME_COMPONENT_NAME, lpcMovieClipName, lFile);
}

void* OnlineInviteMessageComponent::DoTransitionComplete()
{
    // Guest: lwz r11, 0x30(this); cmpwi r11, 2 -- must be transitioning out.
    CGS_ASSERT(meShowingState == E_SHOWINGSTATE_TRANSITIONING_OUT,
               "meShowingState == E_SHOWINGSTATE_TRANSITIONING_OUT");

    // Guest: lwz r10, 0x34(this) -- snapshot the secondary state BEFORE clearing +0x30.
    const EOnlineNotificationChyronType leShowingType = meShowingType;

    // Guest: stw 0, 0x30(this) -- drop back to HIDDEN.
    meShowingState = E_SHOWINGSTATE_INVISIBLE;

    // Guest: cmpwi r10, 4; stb 1, 0x68(this) -- latch dismissed on the match.
    if (leShowingType == E_ONLINENOTIFICATIONCHYRONTYPE_INVITE_RECEIVED)
    {
        mbOutputFinishEvent = true;
    }

    // The X360 leaves `this` (r31/r3) in the result on the non-assert path.
    return this;
}

void OnlineInviteMessageComponent::TransitionCompleteCallback(void* lpUserData,
                                                              u16 luFrame)
{
    (void)luFrame;
    CGS_ASSERT(lpUserData != 0, "lpUserData");
    static_cast<OnlineInviteMessageComponent*>(lpUserData)->DoTransitionComplete();
}
}
