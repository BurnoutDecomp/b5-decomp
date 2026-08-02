// wave-I partfile

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnline.h"

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
