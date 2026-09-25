// wave-I partfile

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnline.h"
#include <cstddef>                                                       // offsetof (wire records)
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                      // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // StateInterface out-queue / PlayAptMovie
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // the state in-queue / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                  // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                          // overlay requests, GuiEventActivateCrashNav, GuiFlow

namespace BrnGui
{

// ===================================================================================
// BrnGui::CrashNavEnterOnlineBase -- the class's out-of-line static data.
//
// The crash-nav "enter online" sign-in screen's tables, all read straight out of
// BURNOUT_X360_ARTIST.XEX .rdata (addresses in the trailing comments). Nothing here is
// derived or inferred: the event list, the resource tuple, the dead zone and the two
// string-pointer families were dumped with headless IDA
// (scratchpad/waveI/cneo_rodata.txt + cneo_kac.txt).
// ===================================================================================
    // The events the screen subscribes to in OnEnter. In the X360's order:
    // controller-press, apt ONLOAD, network-connected, show-login-question,
    // network-disconnected, frame tick, controller-axis, gui-cache, collision-world
    // request, overlay-complete, 134, junkyard-entered.
    const s32 CrashNavEnterOnlineBase::maiEventToObserve[12] =    // @0x820660E0
    {
        6, 21, 43, 47, 44, 26, 8, 64, 493, 189, 134, 81
    };
    const s32 CrashNavEnterOnlineBase::miNumEventsObserved = 12;  // @0x82066110

    // The one apt package the screen loads before it can show anything.
    const CgsGui::sResourceTuple CrashNavEnterOnlineBase::maResourceTuplesToLoad[] =   // @0x82066114
    {
        { 165, CgsGui::E_GUI_RESOURCETYPE_APT }
    };
    const s32 CrashNavEnterOnlineBase::miNumResourcesToLoad = 1;  // @0x8206611C

    // Analogue-stick magnitude the TOS scroll ignores.
    const f32 CrashNavEnterOnlineBase::KF_AXIS_DEAD_ZONE = 0.25f; // @0x82066260

    // ---- component names (the Construct() arguments) --------------------------------
    const char CrashNavEnterOnlineBase::KAC_TEXTFIELD_COMPONENT[12]              = "MessageText";      // @0x82066124
    const char CrashNavEnterOnlineBase::KAC_MESSAGE_BUTTONS_COMPONENT[7]         = "Button";           // @0x82066130
    const char CrashNavEnterOnlineBase::KAC_TOS_QUESTION_COMPONENT[12]           = "TOSQuestion";      // @0x82066138
    const char CrashNavEnterOnlineBase::KAC_TOS_TEXT_COMPONENT[8]                = "TOSText";          // @0x82066144
    const char CrashNavEnterOnlineBase::KAC_TOS_MENU_COMPONENT[10]               = "TOSButton";        // @0x8206614C
    const char CrashNavEnterOnlineBase::KAC_SHARE_INFO_TOGGLES_COMPONENT[10]     = "ShareInfo";        // @0x82066158
    const char CrashNavEnterOnlineBase::KAC_SHARE_INFO_OK_COMPONENT[13]          = "ShareInfo_OK";     // @0x82066164

    // ---- animation-carrier component names ------------------------------------------
    const char CrashNavEnterOnlineBase::KAC_SHARE_INFO_ANIMATION_COMPONENT[20]      = "ShareInfoTransition";        // @0x82066174
    const char CrashNavEnterOnlineBase::KAC_MESSAGE_ANIMATION_COMPONENT[22]         = "MessageTextTransition";      // @0x82066188
    const char CrashNavEnterOnlineBase::KAC_MESSAGE_BUTTONS_ANIMATION_COMPONENT[25] = "MessageButtonsTransition";   // @0x820661A0
    const char CrashNavEnterOnlineBase::KAC_TOS_DISPLAY_ANIMATION_COMPONENT[14]     = "TOSTransition";              // @0x820661BC
    const char CrashNavEnterOnlineBase::KAC_BUTTON_PROMPT_ANIMATION_COMPONENT[23]   = "ButtonPromptTransition";     // @0x820661CC
    const char CrashNavEnterOnlineBase::KAC_SIGN_IN_BACKGROUND_ANIMATION_COMPONENT[27] = "SignInBackgroundTransition"; // @0x820661E4

    // ---- overlay ids / database-text keys --------------------------------------------
    const char CrashNavEnterOnlineBase::KAC_CONNECTING_STRING_ID[13]              = "OnConnecting";    // @0x82066200
    const char CrashNavEnterOnlineBase::KAC_DISCONNECTED_STRING_ID[10]            = "OnConFail";       // @0x82066210
    const char CrashNavEnterOnlineBase::KAC_DISCONNECTED_WITH_REASON_STRING_ID[13] = "OnConFailRsn";   // @0x8206621C
    const char CrashNavEnterOnlineBase::KAC_TOS_TEXT_STRING_ID[10]                = "~TOS_TEXT";       // @0x8206622C
    const char CrashNavEnterOnlineBase::KAC_CREATE_ACCOUNT_TITLE_STRING_ID[35]    = "$ONLINE_LOGIN_CREATE_ACCOUNT_TITLE";  // @0x82066238

    // ---- the login-question body text, indexed by CgsGui::ELoginQuestion --------------
    // The leading '~' on two of them is verbatim: it is the database-text marker the
    // text field strips, not a typo.
    const char* const CrashNavEnterOnlineBase::KAPC_LOGIN_QUESTION_STRING_ID[7] =   // @0x82F26D94
    {
        "$ONLINE_LOGIN_QUESTION_TOS",             // E_LOGIN_QUESTION_TOS
        "~ONLINE_LOGIN_QUESTION_CREATE_ACCOUNT",  // E_LOGIN_QUESTION_CREATE_ACCOUNT
        "$ONLINE_LOGIN_QUESTION_SHARE_1",         // E_LOGIN_QUESTION_SHARE
        "~ONLINE_LOGIN_QUESTION_US_ACCOUNT",      // E_LOGIN_QUESTION_OPEN_US_ACCOUNT
        "$ONLINE_LOGIN_QUESTION_NO_AGREEMENT",    // E_LOGIN_QUESTION_NO_AGREEMENT
        "$ONLINE_LOGIN_QUESTION_SIGN_IN",         // E_LOGIN_QUESTION_SHOW_SIGN_IN
        "$ONLINE_POPUP_CHAT_DISABLED",            // E_LOGIN_QUESTION_CHAT_RESTRICTION
    };

    // ---- the option-pair tables the two-row button menu is dressed from ---------------
    // Each is consumed by a two-entry pointer walk whose bound is the NEXT table's
    // address, so the tables are adjacent in .rdata and their lengths are exact.
    const char* const CrashNavEnterOnlineBase::KAPC_TOS_BUTTON_STRING_ID[2] =           // @0x82F26E40
    {
        "$GENERAL_OPTION_ACCEPT", "$GENERAL_OPTION_DECLINE"
    };
    const char* const CrashNavEnterOnlineBase::KAPC_YES_NO_BUTTON_STRING_ID[2] =        // @0x82F26E48
    {
        "$GENERAL_OPTION_YES", "$GENERAL_OPTION_NO"
    };
    const char* const CrashNavEnterOnlineBase::KAPC_RETRY_CANCEL_BUTTON_STRING_ID[2] =  // @0x82F26E50
    {
        "$GENERAL_OPTION_RETRY", "$GENERAL_OPTION_CANCEL"
    };
    const char* const CrashNavEnterOnlineBase::KAPC_SUBMIT_CANCEL_BUTTON_STRING_ID[2] = // @0x82F26E58
    {
        "$GENERAL_OPTION_SUBMIT", "$GENERAL_OPTION_CANCEL"
    };
    const char* const CrashNavEnterOnlineBase::KAPC_OK_BUTTON_STRING_ID[1] =            // @0x82F26E60
    {
        "$GENERAL_OPTION_OK"
    };

    // ---- the apt view states the six animation carriers are driven with ---------------
    const char* const CrashNavEnterOnlineBase::KAPC_ANIMATION_STATES[4] =               // @0x82F26E64
    {
        "Visible", "Invisible", "Visible_tos", "Visible_no_back"
    };
}

// ===================================================================================
// BrnGui::CrashNavEnterOnlineBase -- the lifecycle and the three page helpers.
//
//   CrashNavEnterOnlineBase()   the constructor (members only; nothing is initialised here,
//                               OnEnter does all of it)
//   OnLeave                     drop the connecting overlay, clear apt level 3, stop
//                               observing, forget the expected apt components
//   Update                      the per-frame event pump, the TOS scroll and the delayed
//                               "connecting" message
//   HideAllComponents           every panel Invisible, the TOS body blanked
//   ShowChatRestrictedPopup     the chat-restriction notice (login question 6)
//   ShowConnectingMessage       every panel Invisible plus the "OnConnecting" overlay
//
// Read store for store off the console asm. The out-queue records are the console's own
// wire records, posted on channel 40 through GetOutputEventQueue()->AddEvent with host
// sizeof sizes (the committed OutputGuiEvent template does not use channel 40; see
// CgsGuiStateInterface.h).
// ===================================================================================
namespace BrnGui
{
    // The two timing constants Update reads. Both values were read from the image.
    const f32 CrashNavEnterOnlineBase::KF_MESSAGE_DISPLAY_WAIT_TIME = 5.0f;
    const f32 CrashNavEnterOnlineBase::KF_TIME_TO_SCROLL_ONE_LINE  = 0.2f;

    namespace
    {
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;   // mpInGuiEventQueue's real type

        const s32 KI_CHANNEL_GUI_OUT = 40;

        // The events Update dispatches on (the maiEventToObserve set).
        const s32 KI_EVENT_CONTROLLER_INPUT      = 6;
        const s32 KI_EVENT_CONTROLLER_AXIS       = 8;
        const s32 KI_EVENT_APT_ONLOAD            = 21;    // observed, no arm
        const s32 KI_EVENT_FRAME_TICK            = 26;
        const s32 KI_EVENT_NETWORK_CONNECTED     = 43;
        const s32 KI_EVENT_NETWORK_DISCONNECTED  = 44;
        const s32 KI_EVENT_SHOW_LOGIN_QUESTION   = 47;
        const s32 KI_EVENT_GUI_CACHE             = 64;
        const s32 KI_EVENT_JUNKYARD_ENTERED      = 81;
        const s32 KI_EVENT_REQUEST_134           = 134;   // answered with GUI record 135
        const s32 KI_EVENT_OVERLAY_COMPLETE      = 189;
        const s32 KI_EVENT_COLLISION_WORLD       = 493;

        // Event 81's payload word that means "the junkyard has been entered".
        const s32 KI_JUNKYARD_ENTERED = 1;

        // OnLeave clears apt level 3 with the empty movie name (the inlined PlayAptMovie).
        const char* const KPC_EMPTY_STRING   = "";
        const s32         KI_APT_MOVIE_LEVEL = 3;

        const char KAC_ADVANCE_EVENT[] = "ADVANCE";

        // ShowConnectingMessage parks mfTimeInState below zero: the frame tick only
        // accumulates a non-negative time, so the message is shown once.
        const f32 KF_TIME_IN_STATE_MESSAGE_SHOWN = -1.0f;

        // The second payload word of the deactivate-CrashNav record the no-title sign-in
        // posts (the same 2 HandleShowLoginQuestion writes).
        const u32 KU_ACTIVATE_CRASHNAV_PARAM_SIGN_IN = 2;

        // The apt view every transition here is posted on, and the view-state indices
        // into KAPC_ANIMATION_STATES.
        const char KAC_APT_TRANSITION_NAME[]            = "apt_Transition";
        const s32  KI_ANIMATION_STATE_VISIBLE           = 0;
        const s32  KI_ANIMATION_STATE_INVISIBLE         = 1;
        const s32  KI_ANIMATION_STATE_VISIBLE_NO_BACK   = 3;

        // ---- in-queue payload views (the queue hands out the header-stripped payload) ----
        struct FrameTickPayload : public CgsModule::Event
        {
            f32 mfDeltaTime;   // +0x00
        };

        struct JunkyardEnteredPayload : public CgsModule::Event
        {
            s32 miState;       // +0x00
        };

        // ---- out-queue wire records ------------------------------------------------------
        // { 8, 188, 16, <pad>, the compressed overlay id }, channel 40, 24 bytes.
        struct GuiOverlayWaitFinishWire : public CgsGui::GuiEvent<188>
        {
            GuiOverlayWaitFinishRequest mRequest;   // +0x10

            explicit GuiOverlayWaitFinishWire(const char* lpcOverlayName)
                : CgsGui::GuiEvent<188>(
                      static_cast<u32>(sizeof(GuiOverlayWaitFinishRequest)),
                      static_cast<u32>(offsetof(GuiOverlayWaitFinishWire, mRequest)))
            {
                mRequest.Construct(lpcOverlayName);
            }
        };

        // { 288, 184, 16, <pad>, the 288-byte request }, channel 40, 304 bytes.
        struct GuiOverlayRequestWire : public CgsGui::GuiEvent<184>
        {
            u32               muPad0C;    // +0x0C
            GuiOverlayRequest mRequest;   // +0x10

            GuiOverlayRequestWire()
                : CgsGui::GuiEvent<184>(static_cast<u32>(sizeof(GuiOverlayRequest)), 16)
                , muPad0C(0)
            {
            }
        };

        // { 1, 135, 12, <one payload byte> }, channel 40, 16 bytes. The console builds only
        // the three header words; the payload byte it sends is never written (zero here).
        struct GuiEventReply135Wire : public CgsGui::GuiEvent<135>
        {
            u8 muUnwrittenPayload;   // +0x0C

            GuiEventReply135Wire()
                : CgsGui::GuiEvent<135>(
                      static_cast<u32>(sizeof(u8)),
                      static_cast<u32>(offsetof(GuiEventReply135Wire, muUnwrittenPayload)))
                , muUnwrittenPayload(0)
            {
            }
        };

        // { 1, 148, 12, <show byte> }, channel 40, 16 bytes: show / hide the HUD.
        struct GuiEventShowHideHudWire : public CgsGui::GuiEvent<148>
        {
            bool mbShowHud;   // +0x0C

            explicit GuiEventShowHideHudWire(bool lbShowHud)
                : CgsGui::GuiEvent<148>(
                      static_cast<u32>(sizeof(bool)),
                      static_cast<u32>(offsetof(GuiEventShowHideHudWire, mbShowHud)))
                , mbShowHud(lbShowHud)
            {
            }
        };

        static_assert(sizeof(GuiOverlayWaitFinishWire) == 24, "wait-finish record is 24 bytes");
        static_assert(sizeof(GuiOverlayRequestWire) == 304, "overlay request record is 304 bytes");
        static_assert(sizeof(GuiEventReply135Wire) == 16, "id-135 record is 16 bytes");
        static_assert(sizeof(GuiEventShowHideHudWire) == 16, "show/hide-hud record is 16 bytes");
        static_assert(sizeof(GuiEventActivateCrashNav) == 20, "activate-crashnav record is 20 bytes");
    }

    // The constructor. The console body stores the vtables of the nine by-value components
    // and runs the two container constructors (the button menu and the share-info toggle
    // group); no scalar member is touched until OnEnter.
    CrashNavEnterOnlineBase::CrashNavEnterOnlineBase()
    {
    }

    // Leaving the sign-in screen: finish the "OnConnecting" overlay wait, clear apt level 3,
    // stop observing the twelve events and drop the apt components the screen was waiting on.
    void CrashNavEnterOnlineBase::OnLeave()
    {
        const GuiOverlayWaitFinishWire lWaitConnecting(KAC_CONNECTING_STRING_ID);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lWaitConnecting), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lWaitConnecting)));

        // The console inlines StateInterface::PlayAptMovie here: { 8, 18, 12, name, level }
        // on channel 41.
        mpStateInterface->PlayAptMovie(KPC_EMPTY_STRING, KI_APT_MOVIE_LEVEL);

        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        if (mpGuiCache != 0)
        {
            mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
        }
    }

    // The per-frame pump. Every observed event is routed to its handler; after the queue is
    // drained the screen finishes loading, updates its two selectable containers and, once
    // the connection attempt has run for KF_MESSAGE_DISPLAY_WAIT_TIME seconds, puts up the
    // "connecting" message.
    void CrashNavEnterOnlineBase::Update()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            switch (liEventId)
            {
            case KI_EVENT_CONTROLLER_INPUT:
                HandleControllerInput(lpEvent);
                break;

            case KI_EVENT_CONTROLLER_AXIS:
                HandleControllerAxis(reinterpret_cast<const CgsGui::GuiEventControllerAxis*>(lpEvent));
                break;

            case KI_EVENT_APT_ONLOAD:
                break;

            case KI_EVENT_FRAME_TICK:
            {
                const f32 lfDeltaTime = reinterpret_cast<const FrameTickPayload*>(lpEvent)->mfDeltaTime;

                // A negative time means the connecting message is already up: stop counting.
                if (!(mfTimeInState < 0.0f))
                {
                    mfTimeInState += lfDeltaTime;
                }

                // The TOS body scrolls one line for every KF_TIME_TO_SCROLL_ONE_LINE the
                // stick has been held (HandleControllerAxis latches the stick value).
                if (mfLastAxisValue == 0.0f)
                {
                    mfScrollAmount = 0.0f;
                }
                else
                {
                    mfScrollAmount = lfDeltaTime * mfLastAxisValue + mfScrollAmount;
                    if (mfScrollAmount > -KF_TIME_TO_SCROLL_ONE_LINE)
                    {
                        if (!(mfScrollAmount < KF_TIME_TO_SCROLL_ONE_LINE))
                        {
                            mTOSText.ScrollUp();
                            mTOSText.OutputAptData();
                            mfScrollAmount = mfScrollAmount - KF_TIME_TO_SCROLL_ONE_LINE;
                        }
                    }
                    else
                    {
                        mTOSText.ScrollDown();
                        mTOSText.OutputAptData();
                        mfScrollAmount = mfScrollAmount + KF_TIME_TO_SCROLL_ONE_LINE;
                    }
                }
                break;
            }

            case KI_EVENT_NETWORK_CONNECTED:
                // Only once the screen is past its two loading sub-states.
                if (meSubState > E_SUBSTATE_LOADING_COMPONENTS)
                {
                    const GuiOverlayWaitFinishWire lWaitConnecting(KAC_CONNECTING_STRING_ID);
                    mpStateInterface->GetOutputEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lWaitConnecting), KI_CHANNEL_GUI_OUT,
                        static_cast<s32>(sizeof(lWaitConnecting)));

                    if (mbJunkyardEntered)
                    {
                        HandleEnteringJunkyard();
                    }
                    else
                    {
                        SendStateEvent(KAC_ADVANCE_EVENT);
                    }
                }
                break;

            case KI_EVENT_NETWORK_DISCONNECTED:
                HandleDisconnectedEvent(lpEvent);
                break;

            case KI_EVENT_SHOW_LOGIN_QUESTION:
                HandleShowLoginQuestion(lpEvent);
                break;

            case KI_EVENT_GUI_CACHE:
                HandleGuiCacheEvent(lpEvent);
                break;

            case KI_EVENT_JUNKYARD_ENTERED:
                if (reinterpret_cast<const JunkyardEnteredPayload*>(lpEvent)->miState == KI_JUNKYARD_ENTERED)
                {
                    mbJunkyardEntered = true;
                }
                break;

            case KI_EVENT_REQUEST_134:
            {
                const GuiEventReply135Wire lReply;
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lReply), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lReply)));
                break;
            }

            case KI_EVENT_OVERLAY_COMPLETE:
                HandleOverlayCompleteEvent(lpEvent);
                break;

            case KI_EVENT_COLLISION_WORLD:
                HandleCollisionWorldEvent(lpEvent);   // the class's own virtual (empty on this screen)
                break;

            default:
                // The console streams the event id and its file/line (line 403) after this
                // text; lowered to the static text.
                CGS_ASSERT(false, "Unexpected event received : ");
                break;
            }
        }

        lpInQueue->Clear();

        CheckForCompletedLoads();

        // Component vtable slot 5 on the two selectable containers.
        mMessageButtons.Update();
        mShareInfoToggles.Update();

        if (!(mfTimeInState < KF_MESSAGE_DISPLAY_WAIT_TIME) && meSubState == E_SUBSTATE_CONNECTING)
        {
            ShowConnectingMessage();

            // The no-title flavour also takes the front end over: CrashNav down, HUD hidden.
            if (meSignInType == E_SIGN_IN_TYPE_NO_TITLE)
            {
                GuiEventActivateCrashNav lDeactivateCrashNav(false);
                lDeactivateCrashNav.muParam = KU_ACTIVATE_CRASHNAV_PARAM_SIGN_IN;
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lDeactivateCrashNav), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lDeactivateCrashNav)));

                const GuiEventShowHideHudWire lHideHud(false);
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lHideHud), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lHideHud)));
            }
        }
    }

    // Every panel goes Invisible and the TOS body is blanked, in the console's call order.
    void CrashNavEnterOnlineBase::HideAllComponents()
    {
        const char* lpcInvisible = KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_INVISIBLE];

        mTOSDisplayAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME, lpcInvisible, false);
        mTOSText.SetText(KPC_EMPTY_STRING);
        mMessageButtonsAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME, lpcInvisible, false);
        mShareInfoTogglesAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME, lpcInvisible, false);
        mMessageAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME, lpcInvisible, false);
        mButtonPromptAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME, lpcInvisible, false);
        mSignInBackgroundAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME, lpcInvisible, false);
    }

    // The chat-restriction notice: the message panel with a single OK button, and a button
    // prompt without a back button (the notice cannot be backed out of).
    void CrashNavEnterOnlineBase::ShowChatRestrictedPopup()
    {
        mMessageText.SetText(KAPC_LOGIN_QUESTION_STRING_ID[CgsGui::E_LOGIN_QUESTION_CHAT_RESTRICTION]);

        mMessageButtons.SetupMenu(1, false);
        mMessageButtons.SetText(0, KAPC_OK_BUTTON_STRING_ID[0]);

        const char* lpcInvisible = KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_INVISIBLE];
        const char* lpcVisible   = KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_VISIBLE];

        mTOSDisplayAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME, lpcInvisible, false);
        mTOSText.SetText(KPC_EMPTY_STRING);
        mShareInfoTogglesAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME, lpcInvisible, false);
        mSignInBackgroundAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME, lpcInvisible, false);
        mMessageAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME, lpcVisible, false);
        mMessageButtonsAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME, lpcVisible, false);
        mButtonPromptAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                     KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_VISIBLE_NO_BACK],
                                                     false);
    }

    // The connection attempt is taking a while: hide every panel behind the "OnConnecting"
    // overlay and park the timer so the message is raised only once.
    void CrashNavEnterOnlineBase::ShowConnectingMessage()
    {
        const char* lpcInvisible = KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_INVISIBLE];

        mTOSDisplayAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME, lpcInvisible, false);
        mTOSText.SetText(KPC_EMPTY_STRING);
        mMessageButtonsAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME, lpcInvisible, false);
        mShareInfoTogglesAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME, lpcInvisible, false);
        mButtonPromptAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME, lpcInvisible, false);
        mSignInBackgroundAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME, lpcInvisible, false);
        mMessageAnimation.AddOutputAptViewState(KAC_APT_TRANSITION_NAME, lpcInvisible, false);

        GuiOverlayRequestWire lConnecting;
        lConnecting.mRequest.Construct(KAC_CONNECTING_STRING_ID);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lConnecting), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lConnecting)));

        mfTimeInState = KF_TIME_IN_STATE_MESSAGE_SHOWN;
    }
}
