// wave-I partfile
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnCrashNavEnterOnline.cpp"
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
    // ARTIST loads these pointers from the table. DecFIGS declares the entries as
    // const char*. The text field uses the leading '~' as a database-text marker.
    const char* CrashNavEnterOnlineBase::KAPC_LOGIN_QUESTION_STRING_ID[7] =   // @0x82F26D94
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
    const char* CrashNavEnterOnlineBase::KAPC_OK_BUTTON_STRING_ID[1] =                  // @0x82F26E60
    {
        "$GENERAL_OPTION_OK"
    };

    // ---- the apt view states the six animation carriers are driven with ---------------
    const char* CrashNavEnterOnlineBase::KAPC_ANIMATION_STATES[4] =                        // @0x82F26E64
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


        // ShowConnectingMessage parks mfTimeInState below zero: the frame tick only
        // accumulates a non-negative time, so the message is shown once.
        const f32 KF_TIME_IN_STATE_MESSAGE_SHOWN = -1.0f;

        // The second payload word of the deactivate-CrashNav record the no-title sign-in
        // posts (the same 2 HandleShowLoginQuestion writes).
        const u32 KU_ACTIVATE_CRASHNAV_PARAM_SIGN_IN = 2;

        // The apt view every transition here is posted on, and the view-state indices
        // into KAPC_ANIMATION_STATES.
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
        struct GuiOverlayWaitFinishWire
        {
            GuiOverlayWaitFinishRequest mRequest;   // the OutputGuiEvent payload

            explicit GuiOverlayWaitFinishWire(const char* lpcOverlayName)
            {
                mRequest.Construct(lpcOverlayName);
            }
        };

        // { 288, 184, 16, <pad>, the 288-byte request }, channel 40, 304 bytes.
        struct GuiOverlayRequestWire
        {
            GuiOverlayRequest mRequest;   // the OutputGuiEvent payload

            GuiOverlayRequestWire()
            {
            }
        };

        // { 1, 135, 12, <one payload byte> }, channel 40, 16 bytes. The console builds only
        // the three header words; the payload byte it sends is never written (zero here).
        struct GuiEventReply135Wire : public CgsGui::GuiEvent<135>
        {
            u8 muUnwrittenPayload;   // +0x0C

            GuiEventReply135Wire()
                : muUnwrittenPayload(0)
            {
            }
        };

        // { 1, 148, 12, <show byte> }, channel 40, 16 bytes: show / hide the HUD.
        struct GuiEventShowHideHudWire : public CgsGui::GuiEvent<148>
        {
            bool mbShowHud;   // +0x0C

            explicit GuiEventShowHideHudWire(bool lbShowHud)
                : mbShowHud(lbShowHud)
            {
            }
            GuiEventShowHideHudWire() {}
        };

        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiOverlayWaitFinishRequest>) == 24), "wait-finish record is 24 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiOverlayRequest>) == 304), "overlay request record is 304 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiEventReply135Wire>) == 16), "id-135 record is 16 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiEventShowHideHudWire>) == 16), "show/hide-hud record is 16 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiEventActivateCrashNav>) == 20), "activate-crashnav record is 20 bytes");
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
        GuiOverlayWaitFinishWire lWaitConnecting(KAC_CONNECTING_STRING_ID);
        mpStateInterface->OutputGuiEvent(lWaitConnecting.mRequest);

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


        const CgsModule::Event* lpEvent;
        s32 liSize;
        for (s32 liEventId = mpInGuiEventQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = mpInGuiEventQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
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
                    GuiOverlayWaitFinishWire lWaitConnecting(KAC_CONNECTING_STRING_ID);
                    mpStateInterface->OutputGuiEvent(lWaitConnecting.mRequest);

                    if (mbJunkyardEntered)
                    {
                        HandleEnteringJunkyard();
                    }
                    else
                    {
                        SendStateEvent("ADVANCE");
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
                GuiEventReply135Wire lReply;
                mpStateInterface->OutputGuiEvent(lReply);
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

        mpInGuiEventQueue->Clear();

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
                GuiEventActivateCrashNav lDeactivateCrashNav;
                lDeactivateCrashNav.muActivate = 0u;
                lDeactivateCrashNav.muParam = 0;
                lDeactivateCrashNav.muParam = KU_ACTIVATE_CRASHNAV_PARAM_SIGN_IN;
                mpStateInterface->OutputGuiEvent(lDeactivateCrashNav);

                GuiEventShowHideHudWire lHideHud(false);
                mpStateInterface->OutputGuiEvent(lHideHud);
            }
        }
    }

    // Every panel goes Invisible and the TOS body is blanked, in the console's call order.
    void CrashNavEnterOnlineBase::HideAllComponents()
    {
        const char* lpcInvisible = KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_INVISIBLE];

        mTOSDisplayAnimation.AddOutputAptViewState("apt_Transition", lpcInvisible, false);
        mTOSText.SetText(KPC_EMPTY_STRING);
        mMessageButtonsAnimation.AddOutputAptViewState("apt_Transition", lpcInvisible, false);
        mShareInfoTogglesAnimation.AddOutputAptViewState("apt_Transition", lpcInvisible, false);
        mMessageAnimation.AddOutputAptViewState("apt_Transition", lpcInvisible, false);
        mButtonPromptAnimation.AddOutputAptViewState("apt_Transition", lpcInvisible, false);
        mSignInBackgroundAnimation.AddOutputAptViewState("apt_Transition", lpcInvisible, false);
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

        mTOSDisplayAnimation.AddOutputAptViewState("apt_Transition", lpcInvisible, false);
        mTOSText.SetText(KPC_EMPTY_STRING);
        mShareInfoTogglesAnimation.AddOutputAptViewState("apt_Transition", lpcInvisible, false);
        mSignInBackgroundAnimation.AddOutputAptViewState("apt_Transition", lpcInvisible, false);
        mMessageAnimation.AddOutputAptViewState("apt_Transition", lpcVisible, false);
        mMessageButtonsAnimation.AddOutputAptViewState("apt_Transition", lpcVisible, false);
        mButtonPromptAnimation.AddOutputAptViewState("apt_Transition",
                                                     KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_VISIBLE_NO_BACK],
                                                     false);
    }

    // The connection attempt is taking a while: hide every panel behind the "OnConnecting"
    // overlay and park the timer so the message is raised only once.
    void CrashNavEnterOnlineBase::ShowConnectingMessage()
    {
        const char* lpcInvisible = KAPC_ANIMATION_STATES[KI_ANIMATION_STATE_INVISIBLE];

        mTOSDisplayAnimation.AddOutputAptViewState("apt_Transition", lpcInvisible, false);
        mTOSText.SetText(KPC_EMPTY_STRING);
        mMessageButtonsAnimation.AddOutputAptViewState("apt_Transition", lpcInvisible, false);
        mShareInfoTogglesAnimation.AddOutputAptViewState("apt_Transition", lpcInvisible, false);
        mButtonPromptAnimation.AddOutputAptViewState("apt_Transition", lpcInvisible, false);
        mSignInBackgroundAnimation.AddOutputAptViewState("apt_Transition", lpcInvisible, false);
        mMessageAnimation.AddOutputAptViewState("apt_Transition", lpcInvisible, false);

        GuiOverlayRequestWire lConnecting;
        lConnecting.mRequest.Construct(KAC_CONNECTING_STRING_ID);
        mpStateInterface->OutputGuiEvent(lConnecting.mRequest);

        mfTimeInState = KF_TIME_IN_STATE_MESSAGE_SHOWN;
    }
}

// FOLDED FROM BrnCrashNavEnterOnline_wI_02.cpp
// wave-I partfile

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnline.h"

namespace BrnGui
{

    // @0x824BC7C8 (cpp:1185) -- the "create an account" page. Same TOS-panel dressing as
    // ShowTOS, but with the create-account title, the create-account question body and
    // the SUBMIT/CANCEL option pair; its button prompt is the ordinary "Visible" state.
    // The question string keeps its leading '~' verbatim -- that prefix is the
    // database-text marker, not stray punctuation.
    void CrashNavEnterOnlineBase::ShowCreateAccount()
    {
        mTOSQuestion.SetText(KAC_CREATE_ACCOUNT_TITLE_STRING_ID);
        TextField& lrTOSText = mTOSText;
        lrTOSText.SetText(KAPC_LOGIN_QUESTION_STRING_ID[CgsGui::E_LOGIN_QUESTION_CREATE_ACCOUNT]);
        lrTOSText.ResetScroll();
        lrTOSText.OutputAptData();

        mMessageButtons.SetupMenu(2, true);
        for (s32 li = 0; li < 2; ++li)
        {
            mMessageButtons.SetText(li, KAPC_SUBMIT_CANCEL_BUTTON_STRING_ID[li]);   // SUBMIT / CANCEL
        }

        // The six view-state pushes, in the X360's call order (@0x824BC870 onwards).
        mMessageAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);      // "Invisible"
        mShareInfoTogglesAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mSignInBackgroundAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mTOSDisplayAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false);   // "Visible"
        mMessageButtonsAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false);
        mButtonPromptAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false);
    }

    // @0x824BCC08 (cpp:1312) -- the sign-in failed page, entered from
    // HandleDisconnectedEvent when the disconnect carried an error code but no reason
    // text. liError is the raw server-interface error the network layer reported; only
    // three of them get their own wording, everything else falls back to the generic
    // "connecting failed" line. The values are kept raw here because the producer-side
    // enum is not recovered -- the compares are the X360's own cmpwi 8 / 0xE / 0x19
    // (@0x824BCC18-0x824BCC30).
    void CrashNavEnterOnlineBase::ShowLoginError(s32 liError)
    {
        // One SetText per case (ARTIST 0x824BCC08 compares 8 / 14 / 25 and cross-jumps the call).
        switch (liError)
        {
        case 8:
            mMessageText.SetText("$ONLINE_POPUP_LOBBY_DISCONNECT");
            break;
        case 14:
            mMessageText.SetText("$ONLINE_POPUP_ACCOUNT_LOCKED");
            break;
        case 25:
            mMessageText.SetText("$ONLINE_NOT_SIGNED_IN");
            break;
        default:
            mMessageText.SetText("$ONLINE_CONNECTING_FAILED");
            break;
        }

        // One button only: acknowledge and go back.
        mMessageButtons.SetupMenu(1, true);
        mMessageButtons.SetText(0, KAPC_OK_BUTTON_STRING_ID[0]);

        // The seven pushes, in the X360's call order (@0x824BCCA8 onwards) -- the
        // message panel comes up and everything else goes away. The TOS body is blanked
        // in the middle of the run exactly as the X360 does it (the empty literal is
        // unk_820046A7, a lone NUL in the pool).
        mTOSDisplayAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);   // "Invisible"
        mTOSText.SetText("");
        mShareInfoTogglesAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mSignInBackgroundAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mMessageAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false);      // "Visible"
        mMessageButtonsAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false);
        mButtonPromptAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false);

        // Last store in the X360 (stw 5, 0x37E4) -- after all seven pushes.
        meSubState = E_SUBSTATE_LOGIN_ERROR;
    }

// ===================================================================================
// BrnGui::CrashNavEnterOnlineBase -- wave-I partfile 02: two more of the seven
// login-question "show" pages, plus the login-error page.
//   ShowTOS           @0x824BC598  cpp:1095
//   ShowCreateAccount @0x824BC7C8  cpp:1185
//   ShowLoginError    @0x824BCC08  cpp:1312
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + asm). The
// screen carries two panels: the MESSAGE panel (mMessageText + mMessageButtons) and the
// TOS panel (the mTOSQuestion title over the scrolling mTOSText body). Each page dresses
// the panel it needs from the question-string table, re-arms the button menu from its
// own option-pair table, and then pushes one apt view state at each of the six animation
// carriers so exactly one panel ends up Visible. Every one of those six pushes was
// transcribed from its own asm -- ShowTOS and ShowCreateAccount are the TOS-panel
// flavour, ShowLoginError is the message-panel flavour, so their Visible/Invisible
// assignments are mirror images and were never copied across bodies.
//
// The button loop in the two TOS pages is the X360's two-entry pointer walk (r30
// marching over the table until it reaches the NEXT table's address, @0x824BC5FC-
// 0x824BC620 and @0x824BC830-0x824BC854): the bound is the table's own length, so it is
// written here as the plain 2-iteration index loop.
// ===================================================================================
    // @0x824BC598 (cpp:1095) -- the terms-of-service page. TOS-panel flavour: the TOS
    // question goes in the title field and the (long) agreement body into mTOSText,
    // which is rewound to the top and re-published before the panel is shown. The one
    // thing that makes this page special is its button prompt: it is the only site in
    // the class that pushes KAPC_ANIMATION_STATES[2] "Visible_tos" (off_82F26E6C), the
    // scroll-aware prompt layout, instead of the plain "Visible".
    void CrashNavEnterOnlineBase::ShowTOS()
    {
        mTOSQuestion.SetText(KAPC_LOGIN_QUESTION_STRING_ID[CgsGui::E_LOGIN_QUESTION_TOS]);
        TextField& lrTOSText = mTOSText;
        lrTOSText.SetText(KAC_TOS_TEXT_STRING_ID);
        lrTOSText.ResetScroll();
        lrTOSText.OutputAptData();

        mMessageButtons.SetupMenu(2, true);
        for (s32 li = 0; li < 2; ++li)
        {
            mMessageButtons.SetText(li, KAPC_TOS_BUTTON_STRING_ID[li]);   // ACCEPT / DECLINE
        }

        // The six view-state pushes, in the X360's call order (@0x824BC63C onwards).
        mMessageAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);      // "Invisible"
        mShareInfoTogglesAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mSignInBackgroundAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mTOSDisplayAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false);   // "Visible"
        mMessageButtonsAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false);
        mButtonPromptAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[2], false); // "Visible_tos"
    }
}

// FOLDED FROM BrnCrashNavEnterOnline_wI_03.cpp
// ===================================================================================
// BrnGui::CrashNavEnterOnlineBase -- wave-I partfile 03: the show-function table and
// the share-info question (its page and its controller handling).
//   InitialiseFunctionArrays  @0x824C1748  cpp:201 (assert cpp:220)
//   ShowShareInfo             @0x824BEF68  cpp:1158
//   HandleLoginInputShareInfo @0x824D8490  cpp:547
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + asm).
//
// InitialiseFunctionArrays builds the per-question dispatch table OnEnter installs: one
// member-function pointer per CgsGui::ELoginQuestion, which HandleShowLoginQuestion then
// calls to dress the screen for whichever question the sign-in flow asked for. The
// console stores raw function addresses into a 7-word array; on the host the same array
// is a pointer-to-member array, so the table is written and checked BY NAME and no
// console word size appears anywhere in the body.
//
// The other two are the share-info question itself -- the only login question that is a
// pair of YES/NO toggle ROWS rather than a message plus a button bar. ShowShareInfo
// re-arms the two rows and brings the toggle panel up; HandleLoginInputShareInfo walks
// the rows/options with the controller and answers the question with one accept flag per
// row. The row-index reads and the group vtable slots behind them were MEASURED, not
// inferred (see the notes on each).
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnline.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"               // CgsGui::ELoginQuestion
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuToggle.h"          // BrnGui::MenuToggle (the group's rows)

namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channel (the out-queue selector word) ---------------------------
        // KI_CHANNEL_GUI_OUT is declared above in this translation unit.

        // ---- controller action ids (the payload's second word) ------------------------
        // The X360 compiles the switch as a jump table over (action - 0x29) with a
        // ten-entry span, so cases 45..48 fall through the default with the rest.
        // FLAG: the producer-side enum for these is not in the recovered DWARF slice; they
        // are named for the role this handler gives them (49 is the GUI_SELECT action the
        // sibling screens also spell 49, 50 the matching back action).
        const s32 KI_ACTION_TOGGLE_PREV = 41;   // 0x29 -- previous toggle ROW
        const s32 KI_ACTION_TOGGLE_NEXT = 42;   // 0x2A -- next toggle ROW
        const s32 KI_ACTION_OPTION_PREV = 43;   // 0x2B -- previous OPTION within the row
        const s32 KI_ACTION_OPTION_NEXT = 44;   // 0x2C -- next OPTION within the row
        const s32 KI_ACTION_ACCEPT      = 49;   // 0x31
        const s32 KI_ACTION_BACK        = 50;   // 0x32

        // ---- in-queue payload view ----------------------------------------------------
        // ARTIST controller event payload: pad id at +0x00, the button/action id at +0x04.
        // Only the action word is read here (`lwz r11, 4(r4)` @0x824D84A4). The queue hands
        // the handler the HEADER-STRIPPED payload, which is why the parameter stays a bare
        // CgsModule::Event* and the view has no GuiEvent header in front of it.
        struct ControllerInputPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04
        };

        // ---- out-queue wire record ----------------------------------------------------
        // The X360 stack-builds { 1, 49, 12 } at [sp+0x50] and AddEvent()s it on channel 40
        // with record size 16 (@0x824D8548-0x824D8574). Event 49 is
        // CgsGui::GuiEventCancelLogin -- the "cancel the sign-in attempt" request the back
        // action posts (the name is the DWARF's, CgsGuiEventTypeDefs.h:427, reached through
        // GuiEventWrapper<CgsGui::GuiEvent<49>,40>). It carries no payload of its own, which
        // is why the console's payload-size word is 1: the size of the empty struct.
        //
        // The two header words are HOST expressions -- the payload size is sizeof(the
        // payload byte) and the payload offset is the host sizeof of the 3-word GuiEvent
        // header (12 on the console, and 12 here as well because CgsModule::Event is empty,
        // but written as sizeof so it tracks the host).
        struct GuiEventCancelLoginWire : public CgsGui::GuiEventCancelLogin
        {
            u8 muPayload;   // +0x0C

            explicit GuiEventCancelLoginWire(u8 luPayload)
                : muPayload(luPayload)
            {
            }
            GuiEventCancelLoginWire() {}
        };
    }

    // ---- InitialiseFunctionArrays @ 0x824C1748 (cpp:201) ---------------------------
    // Fill the per-login-question show-function table. The X360 clears all seven slots
    // with a counted loop (ctr = 7 @0x824C1768), writes the seven entries, then re-walks
    // the same seven slots asserting none of them stayed null -- the belt-and-braces shape
    // that catches a question added to the enum without a page to show for it.
    void CrashNavEnterOnlineBase::InitialiseFunctionArrays()
    {
        for (s32 li = 0; li < CgsGui::E_LOGIN_QUESTION_COUNT; ++li)
        {
            maShowControlsFunc[li] = 0;
        }

        maShowControlsFunc[CgsGui::E_LOGIN_QUESTION_TOS]              = &CrashNavEnterOnlineBase::ShowTOS;
        maShowControlsFunc[CgsGui::E_LOGIN_QUESTION_CREATE_ACCOUNT]   = &CrashNavEnterOnlineBase::ShowCreateAccount;
        maShowControlsFunc[CgsGui::E_LOGIN_QUESTION_SHARE]            = &CrashNavEnterOnlineBase::ShowShareInfo;
        maShowControlsFunc[CgsGui::E_LOGIN_QUESTION_OPEN_US_ACCOUNT]  = &CrashNavEnterOnlineBase::ShowOpenAccountInUS;
        maShowControlsFunc[CgsGui::E_LOGIN_QUESTION_NO_AGREEMENT]     = &CrashNavEnterOnlineBase::ShowNoAgreement;
        maShowControlsFunc[CgsGui::E_LOGIN_QUESTION_SHOW_SIGN_IN]     = &CrashNavEnterOnlineBase::ShowSignIn;
        // The chat-restriction page is owned by a different ledger TU; only its address is
        // needed here, which is why taking it is enough.
        maShowControlsFunc[CgsGui::E_LOGIN_QUESTION_CHAT_RESTRICTION] = &CrashNavEnterOnlineBase::ShowChatRestrictedPopup;

        for (s32 li = 0; li < CgsGui::E_LOGIN_QUESTION_COUNT; ++li)
        {
            // cpp:220. Non-fatal in the X360 build -- the loop keeps walking the table.
            CGS_ASSERT(maShowControlsFunc[li] != 0,
                CGS_ASSERT_MESSAGE_AT(
                    "No show control function supplied for this login question",
                    CGS_ARTIST_FILE, 220 /* ARTIST 0x824C17F0 */));
        }
    }

    // ---- ShowShareInfo @ 0x824BEF68 (cpp:1158) -------------------------------------
    // The "may we share your info?" page. Unlike the other six questions this one is a
    // two-row toggle group: row 0 asks the first sharing question and row 1 the second,
    // each with the same YES/NO option pair. Both rows are re-armed from scratch, then
    // parked on option 1 (== NO, the second entry of the YES/NO pair) so the page opens on
    // the conservative answer; a row that actually moved is dirtied so it repaints.
    void CrashNavEnterOnlineBase::ShowShareInfo()
    {
        mShareInfoToggles.SetupGroup(2, 0);

        // r5 = 2 options, r6 = 1 active, r7 = the row caption, r8 = the option pair,
        // r9 = 0 (no id array -- the rows are answered by option INDEX, not by id).
        // The option table is const in this class but SetupToggle's owning header takes a
        // plain `const char**`, so the pointer is cast through; the console passes the
        // rodata table address straight into r8.
        mShareInfoToggles.SetupToggle(0, 2, true,
                                      KAPC_LOGIN_QUESTION_STRING_ID[CgsGui::E_LOGIN_QUESTION_SHARE],
                                      const_cast<const char**>(KAPC_YES_NO_BUTTON_STRING_ID), 0);
        // Row 1's caption is an inline literal, NOT an entry of the question table: only
        // the first share question has a table slot (the table is indexed by question, and
        // both rows belong to the one SHARE question).
        mShareInfoToggles.SetupToggle(1, 2, true, "$ONLINE_LOGIN_QUESTION_SHARE_2",
                                      const_cast<const char**>(KAPC_YES_NO_BUTTON_STRING_ID), 0);

        // Both rows come back through the BASE group's GetSelectable (the X360 calls
        // BrnGui::SelectableGroup::GetSelectable @0x824BEFE8 / @0x824BF028), so they arrive
        // as Selectable* and are the group's own MenuToggle rows. Highlighting option 1 is
        // the group vtable slot 12 call on the row's embedded option selection
        // (`(*(*(row + 0xA8) + 0x30))(row + 0xA8, 1)` -- +0xA8 IS MenuToggle::mItemText and
        // slot 12 IS SelectableGroup::HighlightIndex, both measured off off_820746F0).
        for (s32 li = 0; li < 2; ++li)
        {
            MenuToggle* lpRow = static_cast<MenuToggle*>(mShareInfoToggles.SelectableGroup::GetSelectable(li));
            if (lpRow->mItemText.HighlightIndex(1))
            {
                lpRow->SetDirty();
            }
        }

        // The seven view-state pushes, in the X360's call order (@0x824BF060 onwards): the
        // share-info toggle panel and the button prompt come up, both message panels and
        // the TOS panel go away, and the TOS body is blanked on the way past (the empty
        // literal is unk_820046A7, a lone NUL in the pool).
        mTOSDisplayAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);   // "Invisible"
        mTOSText.SetText("");
        mMessageAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mMessageButtonsAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mSignInBackgroundAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mShareInfoTogglesAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false); // "Visible"
        mButtonPromptAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[0], false);
    }

    // ---- HandleLoginInputShareInfo @ 0x824D8490 (cpp:547) --------------------------
    // Controller handling while the share-info question is up. The four navigation actions
    // split two ways: 41/42 move between the two toggle ROWS, 43/44 move between the two
    // OPTIONS of whichever row is highlighted. The dispatch targets were MEASURED off the
    // group's component vtable off_820746F0 (slot 10 HighlightNext, 11 HighlightPrevious,
    // 13 HighlightNextItem, 14 HighlightPreviousItem), so the displacements the pseudocode
    // shows (0x28/0x2C/0x34/0x38) are the named methods below.
    void CrashNavEnterOnlineBase::HandleLoginInputShareInfo(const CgsModule::Event* lpEvent)
    {
        const ControllerInputPayload* lpPayload =
            reinterpret_cast<const ControllerInputPayload*>(lpEvent);

        switch (lpPayload->miButtonId)
        {
            case KI_ACTION_TOGGLE_PREV:
                mShareInfoToggles.HighlightPrevious(false);
                break;

            case KI_ACTION_TOGGLE_NEXT:
                mShareInfoToggles.HighlightNext(false);
                break;

            case KI_ACTION_OPTION_PREV:
                mShareInfoToggles.HighlightPreviousItem();
                break;

            case KI_ACTION_OPTION_NEXT:
                mShareInfoToggles.HighlightNextItem();
                break;

            case KI_ACTION_ACCEPT:
            {
                // Answer the question with one flag per row: option 0 of the YES/NO pair is
                // YES, so a row still sitting on index 0 is an accept. The X360 samples row
                // 1 FIRST (@0x824D84F8-0x824D8518) and row 0 second, then passes row 0's
                // answer in r4 and row 1's in r5 (@0x824D852C / @0x824D853C) -- the sample
                // order is kept here, the argument mapping with it.
                //
                // The index itself is the row's embedded option selection's highlight
                // cursor, read by name: MenuToggle::mItemText is at +0xA8 and
                // SelectableGroup::miHighlightedIndex at +0xA5 within it, which is exactly
                // the console's `lbz +0x14D(row)`.
                MenuToggle* lpShare2Row =
                    static_cast<MenuToggle*>(mShareInfoToggles.SelectableGroup::GetSelectable(1));
                const bool lbAccept2 = (lpShare2Row->mItemText.miHighlightedIndex == 0);

                MenuToggle* lpShare1Row =
                    static_cast<MenuToggle*>(mShareInfoToggles.SelectableGroup::GetSelectable(0));
                const bool lbAccept = (lpShare1Row->mItemText.miHighlightedIndex == 0);

                AnswerLoginQuestion(lbAccept, lbAccept2);
                break;
            }

            case KI_ACTION_BACK:
            {
                // Backing out of the share-info question cancels the sign-in attempt. The
                // console never writes the record's payload byte -- it builds only the three
                // header words on the stack and hands AddEvent 16 bytes -- so the byte it
                // sends is whatever was left in that stack slot. The host sends 0.
                GuiEventCancelLoginWire lCancelLogin(0);
                mpStateInterface->OutputGuiEvent(lCancelLogin);   // X360 record size 16
                break;
            }

            default:
                break;
        }
    }
}

// FOLDED FROM BrnCrashNavEnterOnline_wI_04.cpp
// wave-I partfile

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnline.h"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"               // CgsGui::ELoginQuestion
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event

namespace BrnGui
{

// ===================================================================================
// BrnGui::CrashNavEnterOnlineBase -- wave-I partfile 04: answering the login question,
// and the two controller-input handlers that drive the single-menu question pages.
//   AnswerLoginQuestion       @0x824CAE00  cpp:1068
//   HandleLoginInput          @0x824D85F0  cpp:606
//   HandleLoginInputShowSignIn@0x824D8720  cpp:658
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + asm).
//
// AnswerLoginQuestion is the one exit every question page shares: it publishes the
// player's answer on the GUI out-queue, arms the "this disconnect is expected, don't
// popup" flag for the one question whose refusal deliberately drops the connection,
// forgets which question was on screen and drops the screen back into its connecting
// sub-state with every component hidden.
//
// The two handlers are the input front-ends for the pages that carry a single two-row
// button menu (mMessageButtons): the plain question pages go through HandleLoginInput,
// the "you are not signed in" page through HandleLoginInputShowSignIn, which answers
// YES by opening the platform sign-in UI instead of posting an answer. Both compile on
// the X360 to the same 10-entry jump table over (action - 41); only ids 41/42/49/50 have
// arms, everything in between (43..48) and outside the span falls through the default
// and is consumed.
// ===================================================================================
    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) --------------------------
        // KI_CHANNEL_GUI_OUT is declared above in this translation unit.

        // ---- controller action ids (the payload's second word) -----------------------
        // The same four ids the sibling question handler consumes
        // (BrnCarSelectLivery_Input.cpp names the identical set).





        // ---- in-queue payload view ----------------------------------------------------
        // ARTIST controller event payload: pad id at +0x00, the button/action id at +0x04.
        // Only the action word is read here (`lwz r11, 4(r4)` @0x824D8600 / @0x824D8730).
        // ControllerInputPayload is declared above in this translation unit.





        // ---- out-queue wire records ---------------------------------------------------
        // Both header words are HOST expressions, never the console's baked immediates:
        // the first is the host size of the payload tail, the second the host size of the
        // three-word GuiEvent header (12 on the X360, and 12 here too because
        // CgsModule::Event is empty -- written as sizeof so it tracks the host layout).

        // The answer record. X360 stack-builds { 8, 48, 12 } followed by the 8 payload
        // bytes it assembles in a scratch slot first (@0x824CAE28-0x824CAE48: stw the
        // question at +0, stb the two answers at +4 / +5, then one ld/std pair copies the
        // whole 8 bytes into the record) and AddEvent()s it with size 20 on channel 40.
        // The DWARF name for id 48 is CgsGui::GuiEventAnswerLoginQuestion.
        typedef CgsGui::GuiEvent<48> AnswerLoginQuestionHeader;

        struct GuiEventAnswerLoginQuestionWire : public AnswerLoginQuestionHeader
        {
            CgsGui::ELoginQuestion meLoginQuestion;   // +0x0C -- the question being answered
            bool                   mbAccept;          // +0x10 -- first answer (row 0 / "yes")
            bool                   mbAccept2;         // +0x11 -- second answer (share-info row 1)
            u16                    muPad12;           // +0x12 -- X360 ships the scratch slot's
                                                      //          two trailing bytes as-is; zeroed here

            GuiEventAnswerLoginQuestionWire(CgsGui::ELoginQuestion leLoginQuestion,
                                            bool lbAccept,
                                            bool lbAccept2)
                : meLoginQuestion(leLoginQuestion)
                , mbAccept(lbAccept)
                , mbAccept2(lbAccept2)
                , muPad12(0)
            {
            }
            GuiEventAnswerLoginQuestionWire() {}
        };

        // The "cancel the sign-in attempt" request. X360 stack-builds { 1, 49, 12 } and
        // AddEvent()s it with size 16 on channel 40 (@0x824D8690-0x824D86BC), leaving the
        // single payload byte on the stack UNWRITTEN -- so its value is whatever the frame
        // held. It is written as 0 here (a host must not publish an indeterminate byte).
        // The DWARF name for id 49 is CgsGui::GuiEventCancelLogin (CgsGuiEventTypeDefs.h:427,
        // reached through GuiEventWrapper<CgsGui::GuiEvent<49>,40>); the event carries no
        // payload of its own, which is why the payload-size word is the empty struct's 1.
        struct CancelLoginAnswerWire : public CgsGui::GuiEventCancelLogin
        {
            u8 muPayload;   // +0x0C -- X360 leaves this byte uninitialised

            CancelLoginAnswerWire()
                : muPayload(0)
            {
            }
        };
    }

    // ---- AnswerLoginQuestion @ 0x824CAE00 (cpp:1068) -------------------------------
    // Publish the player's answer to whichever login question is currently on screen and
    // hand the screen back to the connecting sub-state.
    //
    // The one piece of state that outlives the answer is mbIgnoreDisconnectedPopup: the
    // NO_AGREEMENT question is the one whose refusal is expected to tear the session down,
    // so declining it arms the flag and the disconnect that follows leaves quietly instead
    // of raising the "connection failed" overlay.
    void CrashNavEnterOnlineBase::AnswerLoginQuestion(bool lbAccept, bool lbAccept2)
    {
        // The question is captured into the record BEFORE it is reset below (the X360
        // reads this+0xC8 @0x824CAE28, well ahead of the `li r10, 7` store @0x824CAEA4).
        GuiEventAnswerLoginQuestionWire lAnswer(meCurrentLoginQuestion, lbAccept, lbAccept2);
        mpStateInterface->OutputGuiEvent(lAnswer);      // X360 record size 20

        mfTimeInState = 0.0f;                        // flt_82001CC0 == 0.0f

        // Only a DECLINED no-agreement question suppresses the disconnect popup
        // (@0x824CAE6C-0x824CAE98: question == 4 and the accept byte zero).
        mbIgnoreDisconnectedPopup =
            (meCurrentLoginQuestion == CgsGui::E_LOGIN_QUESTION_NO_AGREEMENT) && !lbAccept;

        meCurrentLoginQuestion = CgsGui::E_LOGIN_QUESTION_COUNT;   // no question on screen
        meSubState             = E_SUBSTATE_CONNECTING;

        HideAllComponents();
    }

    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) --------------------------

        // ---- controller action ids (the payload's second word) -----------------------

        // ---- in-queue payload view ----------------------------------------------------
        // ---- out-queue wire record ----------------------------------------------------
        // The "cancel the sign-in attempt" request (CgsGui::GuiEventCancelLogin, id 49) is
        // declared once above and reused here; both header words are HOST expressions,
        // never the console's baked immediates.

    }

    // ---- HandleLoginInput @ 0x824D85F0 (cpp:606) -----------------------------------
    // The ordinary question page's input: the two menu-step actions walk the button row,
    // GUI_SELECT answers the question with "did the player leave the highlight on row 0",
    // and GUI_CANCEL cancels the sign-in attempt.
    void CrashNavEnterOnlineBase::HandleLoginInput(const CgsModule::Event* lpEvent)
    {
        const s32 liAction =
            reinterpret_cast<const ControllerInputPayload*>(lpEvent)->miButtonId;

        switch (liAction)
        {
            case KI_ACTION_TOGGLE_PREV:
                mMessageButtons.HighlightPrevious();   // vtable slot 14 (lwz 0x38)
                break;

            case KI_ACTION_TOGGLE_NEXT:
                mMessageButtons.HighlightNext();       // vtable slot 13 (lwz 0x34)
                break;

            case KI_ACTION_ACCEPT:
                // Row 0 is the affirmative option on every page that lands here; the
                // second answer belongs to the share-info page and is always false.
                AnswerLoginQuestion(mMessageButtons.miHighlightedIndex == 0, false);
                break;

            case KI_ACTION_BACK:
                // The chat-restriction page has no way out but forward -- it is a notice,
                // not a question, so backing out of it is ignored entirely.
                if (meCurrentLoginQuestion != CgsGui::E_LOGIN_QUESTION_CHAT_RESTRICTION)
                {
                    CancelLoginAnswerWire lCancelLogin;
                    mpStateInterface->OutputGuiEvent(lCancelLogin);   // X360 record size 16

                    // Backing out of the no-agreement question is the expected way to end
                    // the session, so the disconnect it causes must not raise the popup.
                    mbIgnoreDisconnectedPopup =
                        (meCurrentLoginQuestion == CgsGui::E_LOGIN_QUESTION_NO_AGREEMENT);
                }
                break;

            default:
                break;
        }
    }

    namespace
    {
        // ---- controller action ids (the payload's second word) -----------------------

        // ---- in-queue payload view ----------------------------------------------------
        // ARTIST controller event payload: pad id at +0x00, the button/action id at +0x04.
        // Only the action word is read here (`lwz r11, 4(r4)` @0x824D8730).
    }

    // ---- HandleLoginInputShowSignIn @ 0x824D8720 (cpp:658) -------------------------
    // The "you are not signed in -- sign in now?" page. Saying YES opens the platform
    // sign-in UI rather than answering the question; saying NO, and backing out, both take
    // the same exit: answer the question negatively and arm the ignore-popup flag, because
    // the sign-in that never happened is about to disconnect the session on purpose.
    //
    // The X360 reaches that shared exit by jumping from the GUI_SELECT arm straight into
    // the GUI_CANCEL arm's body (@0x824D8790 bne -> loc_824D87A8); it is written here as
    // the two ids sharing one arm with an explicit test.
    void CrashNavEnterOnlineBase::HandleLoginInputShowSignIn(const CgsModule::Event* lpEvent)
    {
        const s32 liAction =
            reinterpret_cast<const ControllerInputPayload*>(lpEvent)->miButtonId;

        switch (liAction)
        {
            case KI_ACTION_TOGGLE_PREV:
                mMessageButtons.HighlightPrevious();   // vtable slot 14 (lwz 0x38)
                break;

            case KI_ACTION_TOGGLE_NEXT:
                mMessageButtons.HighlightNext();       // vtable slot 13 (lwz 0x34)
                break;

            case KI_ACTION_ACCEPT:
            case KI_ACTION_BACK:
                if (liAction == KI_ACTION_ACCEPT && mMessageButtons.miHighlightedIndex == 0)
                {
                    // YES: clear the page and hand over to the platform sign-in UI (the
                    // base's pure virtual, X360 vtable slot 10). The X360 tail-calls it and
                    // returns its result; nothing here consumes that result.
                    HideAllComponents();
                    ShowSignInUI();
                }
                else
                {
                    AnswerLoginQuestion(false, false);
                    mbIgnoreDisconnectedPopup = true;
                }
                break;

            default:
                break;
        }
    }
}

// FOLDED FROM BrnCrashNavEnterOnline_wI_05.cpp
// wave-I partfile

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnline.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"               // CgsGui::ELoginQuestion
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache

namespace BrnGui
{

    namespace
    {
        // ---- axis ids (the axis payload's first word) ---------------------------------
        // The TOS page scrolls off the two right-hand-stick axes; axis 0 (the left stick)
        // is ignored, and so is anything else (@0x824B652C-0x824B653C).
        const s32 KI_AXIS_RIGHT_STICK_A = 1;
        const s32 KI_AXIS_RIGHT_STICK_B = 2;

        // CgsGui::GuiEventControllerAxis, minus the 12-byte GuiEvent header
        // (BrnCarSelectVehicle_Input.cpp carries the identical view for the same event).
        struct ControllerAxisPayload : public CgsModule::Event
        {
            s32 miAxis;       // +0x00
            f32 mfXAxis;      // +0x04 (not read by this screen)
            f32 mfYAxis;      // +0x08
        };
    }

    // ---- HandleControllerAxis @ 0x824B6480 (cpp:1032) ------------------------------
    // Sample the analogue stick for the TOS page's text scroll. The value is latched raw
    // (the per-frame Update turns it into scroll lines); anything inside the dead zone
    // latches zero, and so does every sub-state / question that is not the TOS page.
    //
    // The parameter type is the DWARF's (h:436) -- unlike its HandleControllerInput
    // sibling this one is declared over the concrete event type. What the in-queue
    // actually hands it is still the HEADER-STRIPPED payload, so the reads below are at
    // +0 and +8 (`lwz r11, 0(r28)` @0x824B652C, `lfs f0, 8(r28)` @0x824B6544) and NOT at
    // GuiEventControllerAxis's own miAxis +0x0C / mfYAxis +0x14. That is why the pointer
    // goes through the local payload view instead of being dereferenced directly; the
    // sibling screen BrnOnlineNews::HandleControllerAxis does the same thing.
    void CrashNavEnterOnlineBase::HandleControllerAxis(const CgsGui::GuiEventControllerAxis* lpEvent)
    {
        // cpp:1032. Streamed into the assert buffer by the X360; non-fatal (the sampling
        // below runs either way), and the "SelectRoutes" in the text is the original's own
        // copy-paste from the route-select screen -- kept verbatim.
        CGS_ASSERT(lpEvent != 0, "Invalid event in SelectRoutes::HandleControllerAxis");

        if (meSubState == E_SUBSTATE_LOGIN_QUESTION &&
            meCurrentLoginQuestion == CgsGui::E_LOGIN_QUESTION_TOS)
        {
            const ControllerAxisPayload* lpAxis =
                reinterpret_cast<const ControllerAxisPayload*>(lpEvent);

            // Any other axis leaves the latch ALONE -- the X360 branches past the store
            // entirely (@0x824B653C -> loc_824B6570), it does not zero it.
            if (lpAxis->miAxis != KI_AXIS_RIGHT_STICK_A &&
                lpAxis->miAxis != KI_AXIS_RIGHT_STICK_B)
            {
                return;
            }

            // The two compares are `fcmpu` + `blt` / `bgt` (@0x824B654C-0x824B6560), both
            // ORDERED branches, so an unordered (NaN) sample falls through to the zero
            // store -- which is exactly what plain C++ < and > do with a NaN operand, so
            // the direct transliteration keeps the console's NaN behaviour.
            const f32 lfYAxis = lpAxis->mfYAxis;
            if (lfYAxis < -KF_AXIS_DEAD_ZONE || lfYAxis > KF_AXIS_DEAD_ZONE)
            {
                mfLastAxisValue = lfYAxis;
            }
            else
            {
                mfLastAxisValue = 0.0f;
            }
        }
        else
        {
            mfLastAxisValue = 0.0f;
        }
    }

    namespace
    {
        // ---- controller action ids (the press payload's second word) ------------------
        // EGameInputActions GUI_SELECT / GUI_BACK. The X360 tests the login-error arm as
        // the unsigned range `(id - 0x31) <= 1` (@0x824DFF10), i.e. exactly these two.



        // CgsGui::GuiEventControllerInputPressed, minus the 12-byte GuiEvent header
        // (the in-queue hands the state the header-stripped payload).
        // ControllerInputPayload is declared above in this translation unit.




    }

    // ---- HandleControllerInput @ 0x824DFEE0 (cpp:488) ------------------------------
    // Route a controller press to the handler for the sub-state that is up. Only two of
    // the six sub-states take presses at all: the login-question pages (routed a second
    // time by which question is showing) and the login-error page (where either the
    // accept or the back button acknowledges the failure and bails out of the flow).
    //
    // Note there is no assert here -- unlike its sibling handlers the X360 body opens
    // straight on the collision-world gate byte (`lbz r11, 0x37EA(r3)`), and returns
    // without touching the event while the crash-nav collision world is being rebuilt.
    void CrashNavEnterOnlineBase::HandleControllerInput(const CgsModule::Event* lpEvent)
    {
        if (mbIsInvalidatingCollisionWorld)
        {
            return;
        }

        if (meSubState == E_SUBSTATE_LOGIN_QUESTION)
        {
            // The share-info page and the "shall I show the sign-in UI" page each own
            // their button layout; every other question is the plain two-button page.
            if (meCurrentLoginQuestion == CgsGui::E_LOGIN_QUESTION_SHARE)
            {
                HandleLoginInputShareInfo(lpEvent);
            }
            else if (meCurrentLoginQuestion == CgsGui::E_LOGIN_QUESTION_SHOW_SIGN_IN)
            {
                HandleLoginInputShowSignIn(lpEvent);
            }
            else
            {
                HandleLoginInput(lpEvent);
            }
        }
        else if (meSubState == E_SUBSTATE_LOGIN_ERROR)
        {
            const s32 liButtonId =
                reinterpret_cast<const ControllerInputPayload*>(lpEvent)->miButtonId;

            if (liButtonId == KI_ACTION_ACCEPT || liButtonId == KI_ACTION_BACK)
            {
                // Acknowledging the error takes the same route out as a real disconnect,
                // minus the popup -- the flag is raised first so the shared handler takes
                // its quiet arm.
                mbIgnoreDisconnectedPopup = true;

                // The X360 hands the handler r4 = &var_10, a stack slot it never wrote
                // (@0x824DFF20): on the ignore-popup arm the callee reads nothing out of
                // the record, so the console gets away with an uninitialised pointer.
                // A zeroed local stands in for it here.
                CgsGui::GuiEventNetworkDisconnected lDummyDisconnect;
                lDummyDisconnect.meLastError  = 0;
                lDummyDisconnect.macReason[0] = 0;

                HandleDisconnectedEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lDummyDisconnect));
            }
        }
    }

    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) --------------------------
        // 42 == OutputInternalState. HandleEnteringJunkyard is the only site in this TU
        // that hides the HUD on the INTERNAL channel (the login-question site posts the
        // same record on 40) -- the X360 loads `li r5, 0x2A` @0x824CB038.
        const s32 KI_CHANNEL_GUI_INTERNAL = 42;

        // ---- out-queue wire record ----------------------------------------------------
        // The { 1, 148, 12, flag } show/hide-HUD command (BrnPauseScreen.cpp's
        // GuiEventHudComponentsCommand twin: flag 0 shuts the HUD components down). The
        // X360 stack-builds it at 0x824CB040-0x824CB060 and pushes 16 bytes on channel 42;
        // it writes only the flag byte, so the pad is modelled zeroed.
        //
        // Both header words are HOST expressions, never the console's baked immediates:
        // the payload size is sizeof(the flag byte) and the payload offset is the host
        // sizeof of the 3-word GuiEvent header (12 on the X360, and 12 here too because
        // CgsModule::Event is empty -- but written as sizeof so it tracks the host layout).
        typedef CgsGui::GuiEvent<148> ShowHideHudHeader;

        struct EnterJunkyardShowHudWire : public ShowHideHudHeader
        {
            u8 mu8Show;      // +0x0C

            explicit EnterJunkyardShowHudWire(u8 lu8Show)
                : mu8Show(lu8Show)
            {
            }
            EnterJunkyardShowHudWire() {}
        };
    }

    // ---- HandleEnteringJunkyard @ 0x824CAFD8 (cpp:1418) ----------------------------
    // Leave the sign-in screen straight into the junkyard. The cache's in-event colouring
    // gate doubles as the "an event is still running" latch here, so entry is refused
    // while it is up; otherwise the HUD components go down and the state event picks the
    // car-unlock flavour of car select over the plain one.
    void CrashNavEnterOnlineBase::HandleEnteringJunkyard()
    {
        // cpp:1418. Plain (unstreamed) FireAssert on the X360; non-fatal, and the body
        // re-tests the pointer afterwards (@0x824CB018) rather than early-returning.
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        if (mpGuiCache != 0 && !mpGuiCache->GetInEventColouringGate())
        {
            EnterJunkyardShowHudWire lHideHud(0);
            mpStateInterface->OutputInternalState(lHideHud);          // X360 record size 16

            SendStateEvent(mpGuiCache->IsJunkyardCarUnlockPending() ? "TO_CUNLOCK"
                                                                    : "TO_CSELECT");
        }
    }
}

// FOLDED FROM BrnCrashNavEnterOnline_wI_06.cpp
// wave-I partfile

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnline.h"
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"               // CgsGui::ELoginQuestion
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface (PlayAptMovie / OutputGuiEvent)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // BrnGui::GuiEventNetworkConnect
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiFlow (E_GUIFLOW_SCREEN)
#include "GameShared/GameClasses/Core/CgsAssert.h"                // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::Event
#include "GameSource/Gui/BrnGuiCache.h"                           // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                   // BrnGui::GuiFlow (E_GUIFLOW_SCREEN)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::RegisterForEvents

namespace BrnGui
{

    namespace
    {
        // The apt movie the screen puts up once its resources are in, and the level it
        // goes to. The name is a pooled literal (off_82F27B74) shared with other screens,
        // not a class static of this one.
        const s32  KI_CONNECTING_MOVIE_LEVEL = 3;

        // The GuiEventNetworkConnect payload word this screen produces. FLAG: the
        // producer-side enum is not recovered -- these are the two raw values the X360
        // stores (@0x824DD684 `li r10, 2`, reusing the register the sub-state store left
        // set, and @0x824DD6A8 `li r11, 0`), named after the sign-in flavour each goes
        // with.
        const s32 KI_CONNECT_TYPE_FULL     = 0;
        const s32 KI_CONNECT_TYPE_NO_TITLE = 2;


    }

    // ================================================================================
    // @0x824DD570 (cpp:746) -- polled from the screen's Update while it is still coming
    // up. Two sub-states: wait for the resource list, then wait for the apt components
    // HandleGuiCacheEvent registered. Once both are in, the screen either replays the
    // disconnect it had to shelve during the load or asks the network layer to connect.
    // ================================================================================
    void CrashNavEnterOnlineBase::CheckForCompletedLoads()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:748

        if (meSubState == E_SUBSTATE_LOADING_SCREEN)
        {
            if (mpGuiCache->EnsureResourcesAreLoaded(maResourceTuplesToLoad,
                                                     static_cast<u32>(miNumResourcesToLoad)))
            {
                mpStateInterface->PlayAptMovie("ON_CONN", KI_CONNECTING_MOVIE_LEVEL);
                meSubState = E_SUBSTATE_LOADING_COMPONENTS;
            }
        }
        else if (meSubState == E_SUBSTATE_LOADING_COMPONENTS)
        {
            // The X360 re-tests the cache here (@0x824DD614) even though the assert above
            // already complained about it -- the assert does not stop the function.
            if (mpGuiCache != 0 && mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
            {
                mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
                HideAllComponents();

                meCurrentLoginQuestion = CgsGui::E_LOGIN_QUESTION_COUNT;
                meSubState             = E_SUBSTATE_CONNECTING;

                // A disconnect that arrived mid-load was shelved rather than acted on;
                // now that the sub-state has moved past the loading pair (both stores
                // above precede this call in the X360 too, @0x824DD654/658 before the
                // `bl` at @0x824DD668) it can be run for real. The handler is given the
                // stored payload record directly.
                if (mbCachedDisconnectEventToHandle)
                {
                    HandleDisconnectedEvent(
                        reinterpret_cast<const CgsModule::Event*>(&mCachedHandleDisconnectEvent));
                    return;
                }

                s32 liConnectType;
                if (meSignInType == E_SIGN_IN_TYPE_NO_TITLE)
                {
                    liConnectType = KI_CONNECT_TYPE_NO_TITLE;
                }
                else
                {
                    // cpp:794 -- anything that is not one of the two known flavours
                    // complains and then carries on into the full-sign-in value, which is
                    // exactly what the X360 does (the assert arm at @0x824DD68C runs
                    // straight on into `li r11, 0` at @0x824DD6A8).
                    if (meSignInType != E_SIGN_IN_TYPE_FULL)
                    {
                        CGS_ASSERT(false, "Unknown sign in type");
                    }
                    liConnectType = KI_CONNECT_TYPE_FULL;
                }

                // The console posts { 4, 272, 12, type } on channel 40 (16 bytes): the
                // inlined OutputGuiEvent<GuiEventNetworkConnect>.
                GuiEventNetworkConnect lConnect;
                lConnect.miConnectType = liConnectType;
                mpStateInterface->OutputGuiEvent(lConnect);
            }
        }
    }

    namespace
    {
        // ---- in-queue payload view -----------------------------------------------------
        // The gui-cache event (id 64). The queue hands the state the HEADER-STRIPPED
        // payload, so the cache pointer sits at +0x00 -- the X360 reads it with a bare
        // `lwz r11, 0(r27)` (@0x824BC43C) and again at @0x824BC4CC.
        struct GuiCachePayload : public CgsModule::Event
        {
            GuiCache* mpCache;   // +0x00
        };
    }

    // ================================================================================
    // @0x824BC428 (cpp:451) -- the gui-cache event (id 64). The screen adopts the cache
    // the first time it is offered one, and immediately tells it which apt components it
    // is going to wait on before it will consider itself loaded.
    // ================================================================================
    void CrashNavEnterOnlineBase::HandleGuiCacheEvent(const CgsModule::Event* lpEvent)
    {
        const GuiCachePayload* lpPayload = reinterpret_cast<const GuiCachePayload*>(lpEvent);

        // cpp:451 -- the streamed-message form of the assert in the X360 image. The test
        // is on the CACHE the event carries, not on the event pointer, and it is
        // non-fatal: the adoption below runs either way.
        CGS_ASSERT(lpPayload->mpCache != 0, CGS_ASSERT_STREAM_MESSAGE_AT("Invalid cache in HandleGuiCacheEvent::Update", CGS_ARTIST_FILE, 451));

        // Only the first cache offered is taken; a screen that already has one ignores
        // the event entirely (@0x824BC4C8 branches straight to the epilogue).
        if (mpGuiCache != 0)
        {
            return;
        }

        mpGuiCache = lpPayload->mpCache;
        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

        // The nine name-taking registrations, in the X360's order (@0x824BC4DC onwards).
        // Each `bl sub_824F87C0` is handed the component's own name buffer directly
        // (r5 = this + component offset + 4, and +4 within a GuiComponent is macName),
        // i.e. GuiCache::AppendExpectedAptComponent(GuiFlow, const char*).
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mMessageText.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mTOSQuestion.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mTOSText.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mMessageAnimation.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mMessageButtonsAnimation.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mTOSDisplayAnimation.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mShareInfoTogglesAnimation.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mButtonPromptAnimation.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mSignInBackgroundAnimation.GetName());

        // The two containers register their own rows (each knows how many it built).
        mShareInfoToggles.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache, true);
        mMessageButtons.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
    }

    namespace
    {
        // The "this group/menu has no apt id" sentinel the two selectable containers get
        // at Construct time. The X360 materialises it as `li r8, -1` followed by
        // `clrldi r8, r8, 32` (@0x824C2B98/@0x824C2BA4 and @0x824C2C04/@0x824C2C10), i.e.
        // the zero-extended 32-bit -1 in a 64-bit register -- NOT a sign-extended -1LL.
        // Same value as SelectableGroup::KU64_NO_ID; spelled out here the way the sibling
        // BrnOnlineGameRoomPlayerInfo_wH_01.cpp Construct sites spell it.
        const u64 KU64_NO_APT_ID = 0xFFFFFFFFull;

        // The number of rows both selectable containers are built with (`li r6, 2`).
        const s32 KI_NUM_SELECTABLE_ROWS = 2;
    }

    // ================================================================================
    // @0x824C2A70 (cpp:148) -- vtable slot 0. Subscribe to the screen's twelve events,
    // build all eleven components, then reset the whole sub-state machine and wire up
    // the per-login-question show functions.
    // ================================================================================
    void CrashNavEnterOnlineBase::OnEnter()
    {
        // The X360 folds the count static to `li r5, 12`; it is the same value.
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // The eleven Construct calls, in the X360's own order (@0x824C2A98 onwards). The
        // nine plain components go through GuiComponent::Construct's vtable slot with a
        // null parent name; the two selectable containers take the row count / parent /
        // apt-id form.
        mMessageAnimation.Construct(KAC_MESSAGE_ANIMATION_COMPONENT, mpStateInterface, 0);
        mMessageButtonsAnimation.Construct(KAC_MESSAGE_BUTTONS_ANIMATION_COMPONENT, mpStateInterface, 0);
        mTOSDisplayAnimation.Construct(KAC_TOS_DISPLAY_ANIMATION_COMPONENT, mpStateInterface, 0);
        mShareInfoTogglesAnimation.Construct(KAC_SHARE_INFO_ANIMATION_COMPONENT, mpStateInterface, 0);
        mButtonPromptAnimation.Construct(KAC_BUTTON_PROMPT_ANIMATION_COMPONENT, mpStateInterface, 0);
        mSignInBackgroundAnimation.Construct(KAC_SIGN_IN_BACKGROUND_ANIMATION_COMPONENT, mpStateInterface, 0);
        mMessageText.Construct(KAC_TEXTFIELD_COMPONENT, mpStateInterface, 0);
        mMessageButtons.Construct(KAC_MESSAGE_BUTTONS_COMPONENT, mpStateInterface,
                                  KI_NUM_SELECTABLE_ROWS, 0, KU64_NO_APT_ID);
        mTOSQuestion.Construct(KAC_TOS_QUESTION_COMPONENT, mpStateInterface, 0);
        mTOSText.Construct(KAC_TOS_TEXT_COMPONENT, mpStateInterface, 0);
        mShareInfoToggles.Construct(KAC_SHARE_INFO_TOGGLES_COMPONENT, mpStateInterface,
                                    KI_NUM_SELECTABLE_ROWS, 0, KU64_NO_APT_ID);

        // The thirteen initialisation stores (@0x824C2C24 onwards), in the X360's store
        // order. The three floats all come off the one pooled 0.0f (flt_82001CC0).
        mfTimeInState          = 0.0f;
        meSubState             = E_SUBSTATE_LOADING_SCREEN;
        mfLastAxisValue        = 0.0f;
        meCurrentLoginQuestion = CgsGui::E_LOGIN_QUESTION_COUNT;   // COUNT == no question showing
        mfScrollAmount         = 0.0f;
        mpGuiCache             = 0;
        mbJunkyardEntered      = false;
        meSignInType           = E_SIGN_IN_TYPE_COUNT;             // COUNT == not chosen yet
        mbIgnoreDisconnectedPopup      = false;
        mbIsInvalidatingCollisionWorld = false;

        // The cached disconnect record is emptied by blanking its reason text and
        // clearing the error code -- the X360 writes only those two fields (`stb 0x3754`
        // and `stw 0x3750`), not the whole 132 bytes.
        mCachedHandleDisconnectEvent.macReason[0] = 0;
        mCachedHandleDisconnectEvent.meLastError  = 0;
        mbCachedDisconnectEventToHandle           = false;

        // NOTE: miNumFilesToLoad is deliberately NOT initialised here -- the X360 leaves
        // it alone in OnEnter.

        InitialiseFunctionArrays();
    }
}

// FOLDED FROM BrnCrashNavEnterOnline_wI_07.cpp
// wave-I partfile

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnline.h"
#include <cstring>                                                       // std::memcpy
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                      // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // StateInterface / GuiEventNetworkSuspension
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                  // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                          // GuiOverlayRequest
#include "GameShared/GameClasses/Core/CgsID.h"                           // CgsID / CgsIDCompress
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                          // GuiOverlayCompleteEvent::LeaveMethod
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                          // GuiOverlayRequest / GuiEventActivateCrashNav / GuiOverlayCompleteEvent

namespace BrnGui
{

    // MERGE NOTE: this anonymous-namespace block is the SUBSET of
    // _07_HandleShowLoginQuestion.cpp's that this body uses. Drop it when merging.
    namespace
    {


        // { 8, 188, 16, <pad>, the compressed overlay id }, channel 40, 24 bytes.
        struct DisconnectWaitFinishWire
        {
            GuiOverlayWaitFinishRequest mRequest;   // the OutputGuiEvent payload

            DisconnectWaitFinishWire()
            {
            }
        };

        // { 288, 184, 16, <pad>, the 288-byte request }, channel 40, 304 bytes -- the
        // shape the out-of-line OutputGuiEvent<GuiOverlayRequest> instantiation
        // @0x82436BE0 builds (memcpy 0x120 into record+0x10, then the three header
        // words, then AddEvent(queue, record, 0x28, 0x130)).
        // GuiOverlayRequestWire is declared above in this translation unit.








        // Record-size pins: every payload here is pointer-free, so the X360's AddEvent
        // size immediates must survive unchanged on the x64 host.
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiOverlayWaitFinishRequest>) == 24), "wait-finish record is 24 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiOverlayRequest>) == 304), "overlay request record is 304 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<CgsGui::GuiEventNetworkSuspension>) == 16), "suspension record is 16 bytes");


        // The message-param slot this handler fills with the disconnect reason text
        // (the X360's `li r4, 1` at 0x824D89E0).
        const CgsGui::PopupParamTypes KU_DISCONNECT_REASON_MESSAGE_PARAM = CgsGui::E_POPUPPARAMTYPES_STRING;
    }

    // ---- HandleDisconnectedEvent @ 0x824D8830 (cpp:873) -----------------------------
    // In-queue event 44. The sign-in attempt fell over. While the screen is still
    // bringing itself up there is nothing to show the message on, so the whole payload
    // is cached and replayed by CheckForCompletedLoads once the components exist.
    // Otherwise the disconnect either backs the player out without a popup (the paths
    // that set mbIgnoreDisconnectedPopup asked for the disconnect themselves) or raises
    // the failure overlay / the login-error page.
    void CrashNavEnterOnlineBase::HandleDisconnectedEvent(const CgsModule::Event* lpDisconnectedEvent)
    {
        CGS_ASSERT(lpDisconnectedEvent != 0, "lpDisconnectedEvent");   // cpp:878

        // The in-queue hands out CgsGui::GuiEventNetworkDisconnected itself: the publisher
        // @0x825660B0 posts the object raw (size 0x84), so there is no header to step over
        // and meLastError really is at payload+0.
        const CgsGui::GuiEventNetworkDisconnected* lpPayload =
            reinterpret_cast<const CgsGui::GuiEventNetworkDisconnected*>(lpDisconnectedEvent);

        // Two separate equality tests in the asm (beq on 0 then beq on 1 at
        // 0x824D8874..0x824D8880), not an unsigned `< 2` range check.
        if (meSubState == E_SUBSTATE_LOADING_SCREEN ||
            meSubState == E_SUBSTATE_LOADING_COMPONENTS)
        {
            // Stash the record verbatim -- the X360's memcpy of 0x84 bytes; the payload
            // is pointer-free so the host sizeof is the same 132.
            std::memcpy(&mCachedHandleDisconnectEvent, lpPayload,
                        sizeof(CgsGui::GuiEventNetworkDisconnected));
            mbCachedDisconnectEventToHandle = true;
            return;
        }

        {
            DisconnectWaitFinishWire lWaitConnecting;
            lWaitConnecting.mRequest.Construct(KAC_CONNECTING_STRING_ID);

            mpStateInterface->OutputGuiEvent(lWaitConnecting.mRequest);
        }

        // cpp:892 -- the second, streamed assert on the same pointer (the X360 fires
        // both, the bare one above and this one after the overlay handshake).
        CGS_ASSERT(lpDisconnectedEvent != 0,
                   "Invalid event sent to CrashNavEnterOnlineBase::HandleDisconnectedEvent");

        if (mbIgnoreDisconnectedPopup)
        {
            // The disconnect was asked for: leave without raising a popup.
            if (mbJunkyardEntered)
            {
                HandleEnteringJunkyard();
            }
            else if (mpGuiCache->IsOnlineStartPending())
            {
                // A queued online start is still pending -- park the network side and
                // take the cheap route back out. The X360 stores one CLEAR payload word
                // (stw r27, with r27 == 0, at 0x824D8964) into the record it hands the
                // out-of-line OutputGuiEvent<GuiEventNetworkSuspension> instantiation,
                // which posts { 4, 45, 12, 0 } on channel 40.
                CgsGui::GuiEventNetworkSuspension lSuspension;
                lSuspension.meSuspensionType = CgsGui::GuiEventNetworkSuspension::E_SUSPENSION_TYPE_SUSPEND;

                mpStateInterface->OutputGuiEvent(lSuspension);

                mpGuiCache->SetOnlineStartPending(false);
                SendStateEvent("GO_BACK_EASY");
            }
            else
            {
                SendStateEvent("GO_BACK");
            }

            // All three arms drop the cached record (the X360 repeats the same three
            // stores per arm -- stb +0x3754, stw +0x3750, stb +0x37EB).
            mCachedHandleDisconnectEvent.macReason[0] = 0;
            mCachedHandleDisconnectEvent.meLastError  = 0;
            mbCachedDisconnectEventToHandle           = false;
            return;
        }

        GuiOverlayRequestWire lWire;
        if (lpPayload->macReason[0] != 0)
        {
            // The server sent wording of its own: show it as the overlay's message.
            lWire.mRequest.Construct(KAC_DISCONNECTED_WITH_REASON_STRING_ID);
            lWire.mRequest.AddMessageParam(KU_DISCONNECT_REASON_MESSAGE_PARAM, lpPayload->macReason);
        }
        else if (lpPayload->meLastError != 0)
        {
            // A coded failure goes to the dedicated login-error page instead of an
            // overlay -- and the X360 returns straight out of here (0x824D8A04), so the
            // cached record is deliberately NOT cleared on this arm.
            ShowLoginError(lpPayload->meLastError);
            return;
        }
        else
        {
            lWire.mRequest.Construct(KAC_DISCONNECTED_STRING_ID);
        }

        mpStateInterface->OutputGuiEvent(lWire.mRequest);

        // Everything the sign-in flow had on screen goes away behind the overlay, in the
        // X360's own call order (0x824D8A40 onwards). off_82F26E68 is
        // KAPC_ANIMATION_STATES[1] == "Invisible".
        mShareInfoTogglesAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mButtonPromptAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mSignInBackgroundAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mTOSDisplayAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mTOSText.SetText("");
        mMessageAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mMessageButtonsAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);

        mCachedHandleDisconnectEvent.macReason[0] = 0;
        mCachedHandleDisconnectEvent.meLastError  = 0;
        mbCachedDisconnectEventToHandle           = false;
        meSubState                                = E_SUBSTATE_DISCONNECTED;
    }

    // MERGE NOTE: this anonymous-namespace block is the SUBSET of
    // _07_HandleShowLoginQuestion.cpp's that this body uses. Drop it when merging.
    namespace
    {

        // { 8, 188, 16, <pad>, the compressed overlay id }, channel 40, 24 bytes.
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiOverlayWaitFinishRequest>) == 24), "wait-finish record is 24 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<CgsGui::GuiEventNetworkSuspension>) == 16), "suspension record is 16 bytes");

        // Event 189 ("overlay complete") payload view: the compressed overlay id then how
        // the overlay was left. The X360 compares the id with a FULL 64-bit `cmpld` after
        // an `ld r11, 0(r30)` (0x824D8B50 / 0x824D8B68) -- the pseudocode's 32-bit
        // `*(a2 + 4)` read is a Hex-Rays artifact. The queue hands out the
        // HEADER-STRIPPED payload, so the view starts at the id.

    }

    // ---- HandleOverlayCompleteEvent @ 0x824D8B00 (cpp:981) --------------------------
    // In-queue event 189. Only the two connection-failure overlays this screen raises
    // are of interest, and only when the player acknowledged them: that is the cue to
    // leave the sign-in flow the same way the no-popup disconnect path does.
    void CrashNavEnterOnlineBase::HandleOverlayCompleteEvent(const CgsModule::Event* lpOverlayCompleteEvent)
    {
        CGS_ASSERT(lpOverlayCompleteEvent != 0, "lpOverlayCompleteEvent");   // cpp:981

        const GuiOverlayCompletePayload* lpPayload =
            reinterpret_cast<const GuiOverlayCompletePayload*>(lpOverlayCompleteEvent);

        // Full 64-bit id compares (cmpld); the second CgsIDCompress is only evaluated
        // when the first misses, exactly as the asm chains them.
        const bool lbOurOverlay =
            lpPayload->mOverlayId == CgsIDCompress(KAC_DISCONNECTED_STRING_ID) ||
            lpPayload->mOverlayId == CgsIDCompress(KAC_DISCONNECTED_WITH_REASON_STRING_ID);

        if (!lbOurOverlay || lpPayload->meLeaveMethod != GuiOverlayCompleteEvent::E_LEAVEMETHOD_OK)
        {
            return;
        }

        {
            DisconnectWaitFinishWire lWaitConnecting;
            lWaitConnecting.mRequest.Construct(KAC_CONNECTING_STRING_ID);

            mpStateInterface->OutputGuiEvent(lWaitConnecting.mRequest);
        }

        // The same leave tail HandleDisconnectedEvent's ignore-popup path runs -- but
        // without its cached-record clears (the X360 has none on this path).
        if (mbJunkyardEntered)
        {
            HandleEnteringJunkyard();
        }
        else if (mpGuiCache->IsOnlineStartPending())
        {
            // One CLEAR payload word (stw r30, with r30 == 0, at 0x824D8BFC) handed to
            // the out-of-line OutputGuiEvent<GuiEventNetworkSuspension> instantiation,
            // which posts { 4, 45, 12, 0 } on channel 40.
            CgsGui::GuiEventNetworkSuspension lSuspension;
            lSuspension.meSuspensionType = CgsGui::GuiEventNetworkSuspension::E_SUSPENSION_TYPE_SUSPEND;

            mpStateInterface->OutputGuiEvent(lSuspension);

            mpGuiCache->SetOnlineStartPending(false);
            SendStateEvent("GO_BACK_EASY");
        }
        else
        {
            SendStateEvent("GO_BACK");
        }
    }

// ===================================================================================
// BrnGui::CrashNavEnterOnlineBase -- wave-I partfile 07: the three in-queue handlers
// that drive the sign-in screen out of its "connecting" phase.
//   b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnline_wI_07.cpp
//
//   HandleShowLoginQuestion    @0x824CAC70  (cpp:833)  in-queue event 47
//   HandleDisconnectedEvent    @0x824D8830  (cpp:873)  in-queue event 44
//   HandleOverlayCompleteEvent @0x824D8B00  (cpp:981)  in-queue event 189
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode arbitrated by
// the raw asm in .ida-exports/BURNOUT_X360_ARTIST.XEX/0x824CAC70.json / 0x824D8830.json
// / 0x824D8B00.json). Every console offset below resolves BY NAME through the owning
// header; the offsets in the comments are documentary only.
//   this+0x1C   -> mpStateInterface (CgsGui::State)   this+0xC4   -> mpGuiCache
//   this+0xC8   -> meCurrentLoginQuestion             this+0xCC   -> meSignInType
//   this+0xD0   -> maShowControlsFunc[7]              this+0x3750 -> mCachedHandleDisconnectEvent
//   this+0x37E4 -> meSubState                         this+0x37E8 -> mbJunkyardEntered
//   this+0x37E9 -> mbIgnoreDisconnectedPopup          this+0x37EB -> mbCachedDisconnectEventToHandle
// and on the cache side cache+0x4B53 -> mbOnlineStartPending.
//
// OUT-QUEUE RECORDS. The X360 reaches them two ways -- by stack-building
// { payload size, event id, payload offset } + payload inline and calling
// VariableEventQueue::AddEvent (the wait-finish / activate-crashnav / show-hide-hud
// sites), or by an out-of-line `bl` to a StateInterface::OutputGuiEvent<T>
// instantiation (GuiOverlayRequest @0x82436BE0, GuiEventNetworkSuspension
// @0x82493A88). Both emit the identical record, and BOTH instantiations post it on
// CHANNEL 40 (`li r5, 0x28` at 0x82436C08 / 0x82493A9C) -- whereas the committed
// CgsGuiStateInterface.h OutputGuiEvent template passes lrEvent.GetEventType() as the
// channel, which is not what the X360 does. So every site below posts the wire record
// directly through mpStateInterface->GetOutputEventQueue()->AddEvent(..., 40,
// sizeof(wire)), exactly as the committed BrnCarSelectMain_wG_02.cpp /
// BrnOnlineGameRoomPlayerInfo_wH_14.cpp do. (GuiOverlayRequest cannot go through the
// template at all -- it is not CgsModule::Event-derived, measured C2664.)
//
// X360 LITERALS. The console's baked AddEvent sizes (0x18 / 0x14 / 0x10) and the
// memcpy size (0x84) are written as host sizeof() expressions. Every payload involved
// is pointer-free, so the static_asserts below pin the host records back onto the
// console sizes instead of letting a widened layout drift onto the wire unnoticed.
// ===================================================================================
    namespace
    {

        // ---- out-queue wire records ------------------------------------------------

        // The OutputGuiEvent<BrnGui::GuiOverlayWaitFinishRequest> record:
        // { 8, 188, 16, <pad>, the compressed overlay id }, channel 40, 24 bytes. All
        // three handlers post it for the "OnConnecting" overlay to tear the connecting
        // spinner down before they act.
        typedef CgsGui::GuiEventOut<GuiEventShowHideHud> GuiShowHideHudWire;

        // The OutputGuiEvent<BrnGui::GuiOverlayRequest> record: { 288, 184, 16, <pad>,
        // the 288-byte request }, channel 40, 304 bytes -- the shape the out-of-line
        // instantiation @0x82436BE0 builds (memcpy 0x120 into record+0x10, then the
        // three header words, then AddEvent(queue, record, 0x28, 0x130)).
        // Record-size pins: every payload here is pointer-free, so the X360's AddEvent
        // size immediates must survive unchanged on the x64 host.
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiOverlayWaitFinishRequest>) == 24), "wait-finish record is 24 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiOverlayRequest>) == 304), "overlay request record is 304 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<CgsGui::GuiEventNetworkSuspension>) == 16), "suspension record is 16 bytes");
        CGS_STATIC_ASSERT((sizeof(GuiShowHideHudWire) == 16), "show/hide-hud record is 16 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiEventActivateCrashNav>) == 20), "activate-crashnav record is 20 bytes");

        // ---- in-queue payload views (the queue hands out the HEADER-STRIPPED payload,
        //      so each view starts at the first field) ---------------------------------

        // Event 47 ("show this login question"): the one word the handler both stores
        // into meCurrentLoginQuestion and re-reads to index maShowControlsFunc.
        struct GuiEventShowLoginQuestionPayload : public CgsModule::Event
        {
            CgsGui::ELoginQuestion meLoginQuestion;   // +0x00
        };

        // Event 189 ("overlay complete"): the compressed overlay id then how the overlay
        // was left. The X360 compares the id with a FULL 64-bit `cmpld` after an
        // `ld r11, 0(r30)` (0x824D8B50 / 0x824D8B68) -- the pseudocode's 32-bit
        // `*(a2 + 4)` read is a Hex-Rays artifact.
        // ---- the overlay / state-event vocabularies (the asm's rodata literals) ------

        // The message-param slot HandleDisconnectedEvent fills with the disconnect
        // reason text (the X360's `li r4, 1` at 0x824D89E0).

        // The second payload word of the deactivate-CrashNav record the no-title sign-in
        // posts (the X360's `li r11, 2` at 0x824CAD98). FLAG: the committed
        // GuiEventActivateCrashNav ctor leaves muParam 0 (the pause screen posts 0) and
        // the word's role is not recovered, so it is written explicitly here.
        // KU_ACTIVATE_CRASHNAV_PARAM_SIGN_IN is declared above in this translation unit.
    }

    // ---- HandleShowLoginQuestion @ 0x824CAC70 (cpp:833) -----------------------------
    // In-queue event 47. The network layer has decided which sign-in page the player has
    // to answer: drop the connecting overlay, remember the question, and run the page's
    // own show-controls member through maShowControlsFunc. On the no-title flavour of
    // the flow the screen additionally takes the front end over -- it deactivates the
    // CrashNav map flow and hides the HUD behind itself.
    void CrashNavEnterOnlineBase::HandleShowLoginQuestion(const CgsModule::Event* lpShowLoginQuestionEvent)
    {
        // cpp:833 -- the X360 streams this text into the assert buffer. Non-fatal: the
        // dispatch below runs regardless (the asm falls straight through EndAssert).
        CGS_ASSERT(lpShowLoginQuestionEvent != 0,
                   "Invalid event sent to CrashNavEnterOnlineBase::HandleShowLoginQuestion");

        const GuiEventShowLoginQuestionPayload* lpPayload =
            reinterpret_cast<const GuiEventShowLoginQuestionPayload*>(lpShowLoginQuestionEvent);

        // The connecting spinner has served its purpose -- tell the overlays director to
        // finish waiting on it.
        {
            DisconnectWaitFinishWire lWaitConnecting;
            lWaitConnecting.mRequest.Construct(KAC_CONNECTING_STRING_ID);

            mpStateInterface->OutputGuiEvent(lWaitConnecting.mRequest);
        }

        meCurrentLoginQuestion = lpPayload->meLoginQuestion;
        meSubState             = E_SUBSTATE_LOGIN_QUESTION;

        // The X360 re-loads the question out of the payload for the dispatch
        // (lwz r11, 0(r26) at 0x824CAD60) rather than re-reading the member it has just
        // written -- same value, kept as written.
        (this->*maShowControlsFunc[lpPayload->meLoginQuestion])();

        if (meSignInType == E_SIGN_IN_TYPE_NO_TITLE)
        {
            // Put the CrashNav map flow to sleep: this screen owns the front end until
            // the sign-in resolves.
            GuiEventActivateCrashNav lDeactivateCrashNav;
            lDeactivateCrashNav.muActivate = 0u;
            lDeactivateCrashNav.muParam = 0;
            lDeactivateCrashNav.muParam = KU_ACTIVATE_CRASHNAV_PARAM_SIGN_IN;

            mpStateInterface->OutputGuiEvent(lDeactivateCrashNav);

            GuiEventShowHideHud lHideHud;
            lHideHud.maData[0] = 0;
            mpStateInterface->OutputGuiEvent(lHideHud);
        }
    }
}

#undef CGS_ARTIST_FILE
