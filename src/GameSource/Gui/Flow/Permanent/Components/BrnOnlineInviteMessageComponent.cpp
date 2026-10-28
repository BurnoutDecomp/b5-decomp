#include "GameSource/Gui/BrnGuiCache.h"
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
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
    mfTimeToRemoveCurrentMessage = 0.0f;
    mfTimeToAllowMessages = 0.0f;
    meShowingState = E_SHOWINGSTATE_INVISIBLE;
    meShowingType = E_ONLINENOTIFICATIONCHYRONTYPE_INVITE_RECEIVED;
    meMessagesAvailable = E_MESSAGING_AVAILABLE_STATE_UNAVAILABLE;
    mbGameStarted = false;
    meQueuedType = E_ONLINENOTIFICATIONCHYRONTYPE_NONE;
    mpGuiCache = nullptr;
    mbOutputFinishEvent = false;
    maTextFields[0].SetInvalid();
    maTextFields[1].SetInvalid();
    // ARTIST82415D68 does not initialize macQueuedName or miQueuedCount.
    // The matching ShowMessage writer owns each before its queued type can read it.
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

// The exact original argument and localization data are defined from the captured ARTIST image below.
extern const char gacOnlineInviteArgument820046A7[];

const f32 OnlineInviteMessageComponent::KF_MESSAGE_DURATION = 5.0f;
const f32 OnlineInviteMessageComponent::KF_TIME_DELAY_BEFORE_ALLOWING_MESSAGES = 1.0f;

void OnlineInviteMessageComponent::Update()
{
    if (mbOutputFinishEvent == true)
    {
        CgsGui::GuiEvent<106> lFinished;
        CgsGui::GuiEventWrapper<CgsGui::GuiEvent<106>, 40> lRecord(lFinished);
        static_assert(sizeof(lRecord) == 16, "original invite-finished channel40 record");
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRecord), 40, sizeof(lRecord));
        mbOutputFinishEvent = false;
    }
    if (!mpGuiCache)
    {
        mpGuiCache = mpStateInterface->GetAccessPointers()->GetGuiCache();
        return;
    }
    if (meMessagesAvailable == E_MESSAGING_AVAILABLE_STATE_PRE_WAIT)
    {
        if (!IsMessageAllowed())
        {
            meMessagesAvailable = E_MESSAGING_AVAILABLE_STATE_UNAVAILABLE;
            return;
        }
        if (mpGuiCache->GetTime() < mfTimeToAllowMessages)
            return;
        AllowMessages();
    }
    else if (meMessagesAvailable == E_MESSAGING_AVAILABLE_STATE_AVAILABLE)
    {
        if (!IsMessageAllowed())
        {
            DisallowMessages();
            return;
        }
    }
    else
    {
        if (IsMessageAllowed() == true)
        {
            meMessagesAvailable = E_MESSAGING_AVAILABLE_STATE_PRE_WAIT;
            mfTimeToAllowMessages = mpGuiCache->GetTime() + KF_TIME_DELAY_BEFORE_ALLOWING_MESSAGES;
        }
        return;
    }
    if (meMessagesAvailable != E_MESSAGING_AVAILABLE_STATE_AVAILABLE)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("meMessagesAvailable == E_MESSAGING_AVAILABLE_STATE_AVAILABLE",
            "..\\..\\..\\GameSource\\Gui/Flow/Permanent/Components/BrnOnlineInviteMessageComponent.cpp", 225);
        CgsDev::Assert::EndAssert();
    }
    if (meShowingState == E_SHOWINGSTATE_SHOWING &&
        mpGuiCache->GetTime() > mfTimeToRemoveCurrentMessage)
        RemoveMessage();
}

void OnlineInviteMessageComponent::ShowMessage(const char* lpcName,
    EOnlineNotificationChyronType leType)
{
    if (!mpGuiCache)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpGuiCache",
            "..\\..\\..\\GameSource\\Gui/Flow/Permanent/Components/BrnOnlineInviteMessageComponent.cpp", 252);
        CgsDev::Assert::EndAssert();
    }
    if (meMessagesAvailable == E_MESSAGING_AVAILABLE_STATE_AVAILABLE)
    {
        if (meShowingState == E_SHOWINGSTATE_INVISIBLE)
        {
            mfTimeToRemoveCurrentMessage = mpGuiCache->GetTime() + KF_MESSAGE_DURATION;
            mAptRef.GotoAndPlayLabel(KAE_TYPE_SIZE_CONVERSION[leType] == E_ONLINE_NOTIFICATION_CHYRON_SIZE_SMALL
                ? "smallTransin" : "largeTransin");
            maTextFields[0].SetLocalisedText(KAPC_TEXT_FIELDS_STINGS_0[leType], 9);
            if (leType == E_ONLINENOTIFICATIONCHYRONTYPE_HOST_START_EVENT ||
                leType == E_ONLINENOTIFICATIONCHYRONTYPE_NEW_NEWS)
                maTextFields[1].SetLocalisedText(KAPC_TEXT_FIELDS_STINGS_1[leType], 9);
            else
            {
                const s32 laFormats[1] = { 0 };
                maTextFields[1].SetLocalisedText(KAPC_TEXT_FIELDS_STINGS_1[leType],
                    9, 1, &lpcName, laFormats);
            }
            meShowingType = leType;
            meShowingState = E_SHOWINGSTATE_SHOWING;
        }
    }
    else if (leType > meQueuedType)
    {
        if (leType != E_ONLINENOTIFICATIONCHYRONTYPE_INVITE_RECEIVED ||
            mpGuiCache->IsFriendsListOpen() != true)
        {
            meQueuedType = leType;
            // ARTIST82415F88..FA0 uses the actual name as the format, then
            // explicitly terminates byte31. Do not replace it with a new format.
            CgsCore::SnPrintf(macQueuedName, KI_MAX_NAME_LENGTH, lpcName);
            macQueuedName[KI_MAX_NAME_LENGTH - 1] = '\0';
        }
    }
}

void OnlineInviteMessageComponent::ShowMessage(s32 liCount,
    EOnlineNotificationChyronType leType)
{
    if (!mpGuiCache)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpGuiCache",
            "..\\..\\..\\GameSource\\Gui/Flow/Permanent/Components/BrnOnlineInviteMessageComponent.cpp", 341);
        CgsDev::Assert::EndAssert();
    }
    if (meMessagesAvailable == E_MESSAGING_AVAILABLE_STATE_AVAILABLE)
    {
        mfTimeToRemoveCurrentMessage = mpGuiCache->GetTime() + KF_MESSAGE_DURATION;
        if (meShowingType != E_ONLINENOTIFICATIONCHYRONTYPE_NUM_BUDDIES_ONLINE)
        {
            mAptRef.GotoAndPlayLabel(KAE_TYPE_SIZE_CONVERSION[leType] == E_ONLINE_NOTIFICATION_CHYRON_SIZE_SMALL
                ? "smallTransin" : "largeTransin");
            maTextFields[0].SetLocalisedText(KAPC_TEXT_FIELDS_STINGS_0[leType], 9);
        }
        maTextFields[1].SetLocalisedText(KAPC_TEXT_FIELDS_STINGS_1[leType], 9, liCount, 11);
        meShowingType = leType;
        meShowingState = E_SHOWINGSTATE_SHOWING;
    }
    else if (leType > meQueuedType &&
        (leType != E_ONLINENOTIFICATIONCHYRONTYPE_INVITE_RECEIVED ||
         mpGuiCache->IsFriendsListOpen() != true))
    {
        meQueuedType = leType;
        miQueuedCount = liCount;
    }
}

bool OnlineInviteMessageComponent::IsMessageAllowed() const
{
    if (!mpGuiCache)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpGuiCache",
            "..\\..\\..\\GameSource\\Gui/Flow/Permanent/Components/BrnOnlineInviteMessageComponent.cpp", 481);
        CgsDev::Assert::EndAssert();
    }
    return !mpGuiCache->IsFriendsListOpen() && mpGuiCache->IsEasyDriveGateActive() == true &&
        !mpGuiCache->IsEATraxChyronActive() && mpGuiCache->IsGameplayHudReadyC() == true &&
        !mpGuiCache->IsReplayExporting();
}

void OnlineInviteMessageComponent::AllowMessages()
{
    meMessagesAvailable = E_MESSAGING_AVAILABLE_STATE_AVAILABLE;
    switch (meQueuedType)
    {
    case E_ONLINENOTIFICATIONCHYRONTYPE_HOST_START_EVENT:
    case E_ONLINENOTIFICATIONCHYRONTYPE_NEW_NEWS:
        ShowMessage(gacOnlineInviteArgument820046A7, meQueuedType);
        break;
    case E_ONLINENOTIFICATIONCHYRONTYPE_BUDDY_ONLINE:
    case E_ONLINENOTIFICATIONCHYRONTYPE_INVITE_RECEIVED:
        ShowMessage(macQueuedName, meQueuedType);
        break;
    case E_ONLINENOTIFICATIONCHYRONTYPE_NUM_BUDDIES_ONLINE:
        ShowMessage(miQueuedCount, meQueuedType);
        break;
    default:
        break;
    }
    meQueuedType = E_ONLINENOTIFICATIONCHYRONTYPE_NONE;
}

void OnlineInviteMessageComponent::DisallowMessages()
{
    if (meMessagesAvailable != E_MESSAGING_AVAILABLE_STATE_AVAILABLE)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("meMessagesAvailable == E_MESSAGING_AVAILABLE_STATE_AVAILABLE",
            "..\\..\\..\\GameSource\\Gui/Flow/Permanent/Components/BrnOnlineInviteMessageComponent.cpp", 563);
        CgsDev::Assert::EndAssert();
    }
    meMessagesAvailable = E_MESSAGING_AVAILABLE_STATE_UNAVAILABLE;
    meQueuedType = E_ONLINENOTIFICATIONCHYRONTYPE_NONE;
    if (meShowingState == E_SHOWINGSTATE_SHOWING)
        RemoveMessage();
}

void OnlineInviteMessageComponent::RemoveMessage()
{
    if (meShowingState != E_SHOWINGSTATE_SHOWING)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("meShowingState == E_SHOWINGSTATE_SHOWING",
            "..\\..\\..\\GameSource\\Gui/Flow/Permanent/Components/BrnOnlineInviteMessageComponent.cpp", 586);
        CgsDev::Assert::EndAssert();
    }
    mAptRef.GotoAndPlayLabel(KAE_TYPE_SIZE_CONVERSION[meShowingType] == E_ONLINE_NOTIFICATION_CHYRON_SIZE_SMALL
        ? "smallTransout" : "largeTransout");
    meShowingState = E_SHOWINGSTATE_TRANSITIONING_OUT;
}

void OnlineInviteMessageComponent::DoTransitionComplete()
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

    // DecFIGS void shape; no output is produced by the original r3 residue.
}

void OnlineInviteMessageComponent::TransitionCompleteCallback(void* lpUserData,
                                                              u16 luFrame)
{
    (void)luFrame;
    CGS_ASSERT(lpUserData != 0, "lpUserData");
    static_cast<OnlineInviteMessageComponent*>(lpUserData)->DoTransitionComplete();
}

// ARTIST820046A7 is the file-backed NUL byte. Both table index-zero
// entries and AllowMessages retain the same actual native character object.
extern const char gacOnlineInviteArgument820046A7[];
const char gacOnlineInviteArgument820046A7[] = { '\0' };

namespace
{
    // One native object per distinct original string target. Repeated table
    // pointers share these objects instead of relying on compiler pooling.
    const char KAC_INVITE_GAMERTAG[] = "GAMERTAG";                       // 820298FC
    const char KAC_INVITE_NEWS_DESC[] = "NEW_NEWS_DESC";                  // 82049D3C
    const char KAC_INVITE_FRIENDS_COUNT[] = "ONLINE_FRIENDS_COUNT";        // 82049D4C
    const char KAC_INVITE_START_EVENT_TEXT[] = "EZDRIVE_TO_START_EVENT_1"; // 82049D64
    const char KAC_INVITE_NEWS_TITLE[] = "NEW_NEWS_TITLE";                // 82049D80
    const char KAC_INVITE_FROM[] = "ONLINE_INVITE_FROM";                  // 82049D90
    const char KAC_INVITE_PLAYING_B5[] = "ONLINE_PLAYING_B5";              // 82049DA4
    const char KAC_INVITE_START_EVENT_TITLE[] = "EZDRIVE_TO_START_EVENT_0";// 82049DB8
}

OnlineInviteMessageComponent::EOnlineNotificationChyronSize
OnlineInviteMessageComponent::KAE_TYPE_SIZE_CONVERSION[E_ONLINENOTIFICATIONCHYRONTYPE_COUNT] =
{
    E_ONLINE_NOTIFICATION_CHYRON_SIZE_SMALL,
    E_ONLINE_NOTIFICATION_CHYRON_SIZE_BIG,
    E_ONLINE_NOTIFICATION_CHYRON_SIZE_SMALL,
    E_ONLINE_NOTIFICATION_CHYRON_SIZE_SMALL,
    E_ONLINE_NOTIFICATION_CHYRON_SIZE_SMALL,
    E_ONLINE_NOTIFICATION_CHYRON_SIZE_BIG,
};

const char* OnlineInviteMessageComponent::KAPC_TEXT_FIELDS_STINGS_0[E_ONLINENOTIFICATIONCHYRONTYPE_COUNT] =
{
    gacOnlineInviteArgument820046A7,
    KAC_INVITE_START_EVENT_TITLE,
    KAC_INVITE_PLAYING_B5,
    KAC_INVITE_PLAYING_B5,
    KAC_INVITE_FROM,
    KAC_INVITE_NEWS_TITLE,
};

const char* OnlineInviteMessageComponent::KAPC_TEXT_FIELDS_STINGS_1[E_ONLINENOTIFICATIONCHYRONTYPE_COUNT] =
{
    gacOnlineInviteArgument820046A7,
    KAC_INVITE_START_EVENT_TEXT,
    KAC_INVITE_GAMERTAG,
    KAC_INVITE_FRIENDS_COUNT,
    KAC_INVITE_GAMERTAG,
    KAC_INVITE_NEWS_DESC,
};
}
