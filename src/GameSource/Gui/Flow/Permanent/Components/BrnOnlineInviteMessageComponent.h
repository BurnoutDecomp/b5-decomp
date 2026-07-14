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
// MINIMAL-SLICE class: only DoTransitionComplete is in scope, so this models ONLY the
// three members it touches (the showing-state at +0x30, a secondary state at +0x34, and
// a bool flag at +0x68). The full component (its GuiComponent base, the message text /
// timers, the rest of the state machine) is uncommitted and OMITTED. FLAG: minimal-slice
// class. Offsets are not load-bearing on the 64-bit host; members are declared by name.

namespace BrnGui
{
    class GuiCache;

    class OnlineInviteMessageComponent : public BrnFlaptComponent
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
        // pending dismiss, value 4) latch the dismissed flag. Returns the X360 r3 result
        // (`this` on the non-assert path); kept as void* to mirror the guest _DWORD* return.
        void* DoTransitionComplete();

        // --- Construct / Prepare exercised by the AlwaysAvailableComponentsManager ----
        // Declared here (additive) so the manager TU compiles against the real home
        // rather than forking the type; bodies live in this component's own TU.

        // @0x824F3628 Construct site: r4="OnlineInvite_mc", r5=&StateInterface, r6=0.
        void Construct(const char* lpcMovieClipName,
                       CgsGui::StateInterface* lpStateInterface,
                       const char* lpcParentName);

        // PrepareFlapt site: r4="OnlineInvite_mc", r5=FileRef.
        void Prepare(const char* lpcMovieClipName, const BrnFlapt::FileRef& lFile);

        static void TransitionCompleteCallback(void* lpUserData, u16 luFrame);

    private:
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
