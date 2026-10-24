#ifndef BRN_ONLINE_INVITE_MESSAGE_COMPONENT_H
#define BRN_ONLINE_INVITE_MESSAGE_COMPONENT_H

#include "types.hpp"
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h"
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"

namespace CgsGui { class StateInterface; }
namespace BrnFlapt { struct FileRef; }

// BrnGui::OnlineInviteMessageComponent - the permanent GUI component that shows the
// "you've been invited" online message and runs its show / transition-out animation.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnGui::OnlineInviteMessageComponent::DoTransitionComplete @ 0x824162E0
//
// Complete original state/timer/queue owner. The three original data arrays and
// unnamed argument storage remain genuine unresolved definitions; this recovery
// is unactivated and not link-ready. Members use native host placement by name.

namespace BrnGui
{
    class GuiCache;

    // DecFIGS h48: transparent original intermediate base, no extra data.
    struct BaseOnlineInviteMessageComponent : public BrnFlaptComponent {};

    class OnlineInviteMessageComponent : public BaseOnlineInviteMessageComponent
    {
    public:
        // The component's showing-state machine. The transition-complete callback asserts
        // the state is TRANSITIONING_OUT and then resets it to HIDDEN. Only the two values
        // the asm names (0 reset target, 2 the asserted value) are attested; the
        // intermediate values are the conventional show/transition-in states.
        enum EShowingState
        {
            E_SHOWINGSTATE_INVISIBLE        = 0,
            E_SHOWINGSTATE_SHOWING          = 1,
            E_SHOWINGSTATE_TRANSITIONING_OUT = 2, // value the assert requires
            E_SHOWINGSTATE_COUNT            = 3,
        };

        enum EOnlineNotificationChyronType
        {
            E_ONLINENOTIFICATIONCHYRONTYPE_NONE = 0,
            E_ONLINENOTIFICATIONCHYRONTYPE_HOST_START_EVENT = 1,
            E_ONLINENOTIFICATIONCHYRONTYPE_BUDDY_ONLINE = 2,
            E_ONLINENOTIFICATIONCHYRONTYPE_NUM_BUDDIES_ONLINE = 3,
            E_ONLINENOTIFICATIONCHYRONTYPE_INVITE_RECEIVED = 4,
            E_ONLINENOTIFICATIONCHYRONTYPE_NEW_NEWS = 5,
            E_ONLINENOTIFICATIONCHYRONTYPE_COUNT = 6,
        };

        enum EMessagingAvailableState
        {
            E_MESSAGING_AVAILABLE_STATE_UNAVAILABLE = 0,
            E_MESSAGING_AVAILABLE_STATE_PRE_WAIT = 1,
            E_MESSAGING_AVAILABLE_STATE_AVAILABLE = 2,
            E_MESSAGING_AVAILABLE_STATE_COUNT = 3,
        };

        // 0x824162E0 -- called when the transition-out animation finishes: assert we were
        // transitioning out, drop to HIDDEN, and (if the secondary state indicates a
        // pending dismiss, value4) latch the finish event. The original logical
        // return type is void; r3 is ignored residue, not an output object.
        void DoTransitionComplete();

        // --- Construct / Prepare exercised by the AlwaysAvailableComponentsManager ----
        // Declared here (additive) so the manager TU compiles against the real home
        // rather than forking the type; bodies live in this component's own TU.

        // @0x824F3628 Construct site: r4="OnlineInvite_mc", r5=&StateInterface, r6=0.
        void Construct(const char* lpcMovieClipName,
                       CgsGui::StateInterface* lpStateInterface,
                       const char* lpcParentName);

        // PrepareFlapt site: r4="OnlineInvite_mc", r5=FileRef.
        void Prepare(const char* lpcMovieClipName, const BrnFlapt::FileRef& lFile);

        // ARTIST82439FF0; DecFIGS cpp158. Complete original provider.
        void Update();
        void ShowMessage(const char* lpcName, EOnlineNotificationChyronType leType);
        void ShowMessage(s32 liCount, EOnlineNotificationChyronType leType);

        static void TransitionCompleteCallback(void* lpUserData, u16 luFrame);

    private:
        bool IsMessageAllowed() const;
        void AllowMessages();
        void DisallowMessages();
        void RemoveMessage();

        enum EOnlineNotificationChyronSize
        {
            E_ONLINE_NOTIFICATION_CHYRON_SIZE_SMALL = 0,
            E_ONLINE_NOTIFICATION_CHYRON_SIZE_BIG = 1,
        };
        static const s32 KI_MAX_NAME_LENGTH = 32;
        static const f32 KF_MESSAGE_DURATION;
        static const f32 KF_TIME_DELAY_BEFORE_ALLOWING_MESSAGES;
        // Genuine ARTIST data owners; declarations only until actual values and
        // pointed strings are recovered. Never initialize from notification names.
        static EOnlineNotificationChyronSize KAE_TYPE_SIZE_CONVERSION[E_ONLINENOTIFICATIONCHYRONTYPE_COUNT]; // 8204C594
        static const char* KAPC_TEXT_FIELDS_STINGS_0[E_ONLINENOTIFICATIONCHYRONTYPE_COUNT]; // 82F24F8C
        static const char* KAPC_TEXT_FIELDS_STINGS_1[E_ONLINENOTIFICATIONCHYRONTYPE_COUNT]; // 82F24FA4

        GuiCache* mpGuiCache;
        BrnFlapt::TextFieldRef maTextFields[2];
        f32 mfTimeToRemoveCurrentMessage;
        f32 mfTimeToAllowMessages;
        EShowingState meShowingState;
        EOnlineNotificationChyronType meShowingType;
        EMessagingAvailableState meMessagesAvailable;
        bool mbGameStarted;
        EOnlineNotificationChyronType meQueuedType;
        char macQueuedName[32];
        s32 miQueuedCount;
        bool mbOutputFinishEvent;
    };
}

#endif
