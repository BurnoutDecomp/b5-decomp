#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"              // CgsGui::ELoginQuestion
#include "GameSource/Gui/BrnGuiTextField.h"                              // BrnGui::TextField (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h" // BrnGui::AnimationComponent (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuComponent.h"      // BrnGui::MenuComponent (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuToggleGroup.h"    // BrnGui::MenuToggleGroupVarSize<2> (by value)

// BrnGui::CrashNavEnterOnlineBase - base of the crash-navigation "enter online" sign-in
// screen states: the connect / terms-of-service / create-account / share-info / login-error
// flow, driven by one ESubState machine plus a per-login-question show-function table.
//
// CLASS SHAPE + MEMBER ORDER: DecFIGS DWARF (BrnCrashNavEnterOnline.h).
// MEMBER PLACEMENT: X360 ARTIST asm (wave I; see scratchpad/waveI/CrashNavEnterOnline.spec.md
// for the vtable dump @0x82074818, the decoded rodata and the per-function triage).
// The X360 byte offsets in the trailing comments are DOCUMENTATION ONLY -- the x64 host
// layout is name-based and several of these members contain pointers that widen.
namespace CgsModule { struct Event; }

namespace BrnGui
{
    class GuiCache;   // pointer member only (BrnMenuToggle.h precedent)

    struct CrashNavEnterOnlineBase : public CgsGui::State
    {
        // DWARF BrnCrashNavEnterOnline.h -- which sign-in flavour the screen runs.
        enum ESignInType
        {
            E_SIGN_IN_TYPE_FULL     = 0,
            E_SIGN_IN_TYPE_NO_TITLE = 1,
            E_SIGN_IN_TYPE_COUNT    = 2,
        };

        // DWARF BrnCrashNavEnterOnline.h:156 -- the screen's sub-state machine (X360 +0x37E4).
        enum ESubState
        {
            E_SUBSTATE_LOADING_SCREEN     = 0,
            E_SUBSTATE_LOADING_COMPONENTS = 1,
            E_SUBSTATE_CONNECTING         = 2,
            E_SUBSTATE_LOGIN_QUESTION     = 3,
            E_SUBSTATE_DISCONNECTED       = 4,
            E_SUBSTATE_LOGIN_ERROR        = 5,
            E_SUBSTATE_COUNT              = 6,
        };

        // @0x825010E8 -- body owned by the class:BrnGui::CrashNavEnterOnlineBase TU.
        CrashNavEnterOnlineBase();

        // @ 0x82501170 - hands the enter-online screen's static resource list to the
        // loader (X360: *r4 = &maResourceTuplesToLoad; *r5 = miNumResourcesToLoad, count = 1).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = (u32)miNumResourcesToLoad;
        }

        // ---- lifecycle virtuals (X360 vtable slots 0/1/2, dumped @0x82074818) --------
        virtual void OnEnter();   // @0x824C2A70 (this TU)
        virtual void OnLeave();   // @0x824CAB88 -- FOREIGN ledger TU; declared for the vtable + the X360 flavour's base-chain call
        virtual void Update();    // @0x824E0680 -- FOREIGN ledger TU

        // X360 vtable slot 10 -- pure in the base table (_purecall), overridden by the
        // X360 flavour @0x82488010. u32 per the committed override (the DWARF says void;
        // the override tail-returns XShowSigninUI's result).
        virtual u32 ShowSignInUI() = 0;

    protected:
        void AnswerLoginQuestion(bool lbAccept, bool lbAccept2);   // @0x824CAE00 cpp:1068

    private:
        // X360 vtable slot 9 -- EMPTY body on X360 (ICF-folded @0x8284CB38, not a ledger
        // function; Update's event-493 arm calls it virtually). DWARF cpp:967.
        virtual void HandleCollisionWorldEvent(const CgsModule::Event* lpEvent) {}

        // ---- this TU's 21 ledger functions (X360 addresses; cpp:<line> = DWARF) ------
        void InitialiseFunctionArrays();                                 // @0x824C1748 cpp:201
        void CheckForCompletedLoads();                                   // @0x824DD570 cpp:746
        void HandleGuiCacheEvent(const CgsModule::Event* lpEvent);       // @0x824BC428 cpp:451
        void HandleControllerInput(const CgsModule::Event* lpEvent);     // @0x824DFEE0 cpp:488
        void HandleLoginInput(const CgsModule::Event* lpEvent);          // @0x824D85F0 cpp:606
        void HandleLoginInputShareInfo(const CgsModule::Event* lpEvent); // @0x824D8490 cpp:547
        void HandleLoginInputShowSignIn(const CgsModule::Event* lpEvent);// @0x824D8720 cpp:658
        void HandleShowLoginQuestion(const CgsModule::Event* lpEvent);   // @0x824CAC70 cpp:833
        void HandleDisconnectedEvent(const CgsModule::Event* lpEvent);   // @0x824D8830 cpp:873
        void HandleOverlayCompleteEvent(const CgsModule::Event* lpEvent);// @0x824D8B00 cpp:981
        // DWARF h:436 types this parameter GuiEventControllerAxis, not CgsModule::Event
        // (contrast HandleControllerInput at h:409, which really is the bare Event). The
        // in-queue still delivers the HEADER-STRIPPED payload, so the body reads +0/+8
        // rather than the type's own +0x0C/+0x14 -- see the note on the body.
        void HandleControllerAxis(const CgsGui::GuiEventControllerAxis* lpEvent); // @0x824B6480 cpp:1032
        void ShowTOS();                                                  // @0x824BC598 cpp:1095
        void ShowShareInfo();                                            // @0x824BEF68 cpp:1158
        void ShowCreateAccount();                                        // @0x824BC7C8 cpp:1185
        void ShowOpenAccountInUS();                                      // @0x824BC8E8 cpp:1217
        void ShowNoAgreement();                                          // @0x824BCA08 cpp:1249
        void ShowLoginError(s32 liError);                                // @0x824BCC08 cpp:1312
        void ShowSignIn();                                               // @0x824BC6B8 cpp:1127
        void HandleEnteringJunkyard();                                   // @0x824CAFD8 cpp:1418

        // ---- methods of THIS class owned by FOREIGN ledger TUs. Declared so this TU's
        //      bodies can call them; none is defined in the tree yet, so the screen will
        //      not link until those TUs land. Deliberately NOT stubbed.
        void HideAllComponents();                                        // @0x824B6578 cpp:1399
        void ShowChatRestrictedPopup();                                  // @0x824BCB18 cpp:1280 (PTMF slot 6)
        void ShowConnectingMessage();                                    // @0x824CAEC8 cpp:1370

        // ---- statics (dumped X360 rodata; definitions live in the .cpp) -------------
        static const CgsGui::sResourceTuple maResourceTuplesToLoad[]; // @ 0x82066114 (unk_82066114, .rdata)
        static const s32                    miNumResourcesToLoad;     // @ 0x8206611C (dword_8206611C, .rdata) == 1

        static const s32 maiEventToObserve[12];                     // @0x820660E0
        static const s32 miNumEventsObserved;                       // @0x82066110 == 12
        static const f32 KF_AXIS_DEAD_ZONE;                         // @0x82066260 == 0.25f
        static const char KAC_TEXTFIELD_COMPONENT[12];              // "MessageText"
        static const char KAC_MESSAGE_BUTTONS_COMPONENT[7];         // "Button"
        static const char KAC_TOS_QUESTION_COMPONENT[12];           // "TOSQuestion"
        static const char KAC_TOS_TEXT_COMPONENT[8];                // "TOSText"
        static const char KAC_TOS_MENU_COMPONENT[10];               // "TOSButton"   @0x8206614C
        static const char KAC_SHARE_INFO_TOGGLES_COMPONENT[10];     // "ShareInfo"
        static const char KAC_SHARE_INFO_OK_COMPONENT[13];          // "ShareInfo_OK" @0x82066164
        static const char KAC_SHARE_INFO_ANIMATION_COMPONENT[20];   // "ShareInfoTransition"
        static const char KAC_MESSAGE_ANIMATION_COMPONENT[22];      // "MessageTextTransition"
        static const char KAC_MESSAGE_BUTTONS_ANIMATION_COMPONENT[25]; // "MessageButtonsTransition"
        static const char KAC_TOS_DISPLAY_ANIMATION_COMPONENT[14];  // "TOSTransition"
        static const char KAC_BUTTON_PROMPT_ANIMATION_COMPONENT[23];// "ButtonPromptTransition"
        static const char KAC_SIGN_IN_BACKGROUND_ANIMATION_COMPONENT[27]; // "SignInBackgroundTransition"
        static const char KAC_CONNECTING_STRING_ID[13];             // "OnConnecting"
        static const char KAC_DISCONNECTED_STRING_ID[10];           // "OnConFail"
        static const char KAC_DISCONNECTED_WITH_REASON_STRING_ID[13]; // "OnConFailRsn"
        static const char KAC_TOS_TEXT_STRING_ID[10];               // "~TOS_TEXT"
        static const char KAC_CREATE_ACCOUNT_TITLE_STRING_ID[35];   // "$ONLINE_LOGIN_CREATE_ACCOUNT_TITLE"
        static const char* const KAPC_LOGIN_QUESTION_STRING_ID[7];  // @0x82F26D94 (by ELoginQuestion)
        static const char* const KAPC_TOS_BUTTON_STRING_ID[2];      // @0x82F26E40
        static const char* const KAPC_YES_NO_BUTTON_STRING_ID[2];   // @0x82F26E48
        static const char* const KAPC_RETRY_CANCEL_BUTTON_STRING_ID[2];  // @0x82F26E50
        static const char* const KAPC_SUBMIT_CANCEL_BUTTON_STRING_ID[2]; // @0x82F26E58
        static const char* const KAPC_OK_BUTTON_STRING_ID[1];       // @0x82F26E60
    protected:
        static const char* const KAPC_ANIMATION_STATES[4];          // @0x82F26E64 {"Visible","Invisible","Visible_tos","Visible_no_back"}

        // ---- data members (DWARF order; the X360 offsets are documentation) ---------
    private:
        AnimationComponent mSignInBackgroundAnimation;   // h:132  X360 +0x38  "SignInBackgroundTransition"
    protected:
        GuiCache*          mpGuiCache;                   // h:134  X360 +0xC4
        CgsGui::ELoginQuestion meCurrentLoginQuestion;   // h:135  X360 +0xC8 (COUNT == none)
        // FLAG: the member the Mod TU writes (DWARF meSignInType; X360 this+0xCC, set to
        // NO_TITLE by CrashNavEnterOnlineNoTitle::OnEnter @0x824B6628, the stw @0x824B6644).
        ESignInType meSignInType;                        // h:137  X360 +0xCC
    private:
        // h:208 X360 +0xD0, indexed by ELoginQuestion. The console collapses a PTMF to
        // 4 bytes; the host PTMF is wider, which is fine -- access is by name.
        void (CrashNavEnterOnlineBase::*maShowControlsFunc[7])();
        AnimationComponent mMessageAnimation;            // h:211  X360 +0xEC  "MessageTextTransition"
        AnimationComponent mMessageButtonsAnimation;     // h:212  X360 +0x178 "MessageButtonsTransition"
        AnimationComponent mTOSDisplayAnimation;         // h:213  X360 +0x204 "TOSTransition"
        AnimationComponent mShareInfoTogglesAnimation;   // h:214  X360 +0x290 "ShareInfoTransition"
        AnimationComponent mButtonPromptAnimation;       // h:215  X360 +0x31C "ButtonPromptTransition"
        TextField          mMessageText;                 // h:217  X360 +0x3A8 "MessageText"
        MenuComponent      mMessageButtons;              // h:218  X360 +0x4D0 "Button" (2 rows)
        TextField          mTOSQuestion;                 // h:221  X360 +0x1590 "TOSQuestion"
        TextField          mTOSText;                     // h:222  X360 +0x16B8 "TOSText"
        // h:225 "ShareInfo". The DWARF type is MenuToggleGroup; the X360 calls the
        // VarSize<2> instantiation throughout.
        MenuToggleGroupVarSize<2> mShareInfoToggles;     //        X360 +0x17E0
        // h:346 X360 +0x3750 (132 bytes). The DWARF member type verbatim -- the record is
        // stashed whole (the X360's memcpy of 0x84 bytes @0x824D8AE0) while the screen is
        // still loading and replayed by CheckForCompletedLoads. CgsGui::
        // GuiEventNetworkDisconnected is header-free, so the object the in-queue hands the
        // handler and the object cached here are the same 132 bytes.
        CgsGui::GuiEventNetworkDisconnected mCachedHandleDisconnectEvent;
        s32  miNumFilesToLoad;                           // h:229  X360 +0x37D4 (not touched by this TU's 21)
        f32  mfTimeInState;                              // h:230  X360 +0x37D8
        f32  mfScrollAmount;                             // h:232  X360 +0x37DC
        f32  mfLastAxisValue;                            // h:233  X360 +0x37E0
        ESubState meSubState;                            // h:235  X360 +0x37E4
        bool mbJunkyardEntered;                          // h:237  X360 +0x37E8
        bool mbIgnoreDisconnectedPopup;                  // h:238  X360 +0x37E9
        bool mbIsInvalidatingCollisionWorld;             // h:239  X360 +0x37EA
        bool mbCachedDisconnectEventToHandle;            // h:240  X360 +0x37EB

        // Compile-time layout oracle for the pointer-free scalar tail: these relative
        // deltas are the X360's and survive the host's pointer widening because every
        // member here is a 4-byte scalar or a byte. A member-function body is a complete
        // -class context, so offsetof on private members is legal here.
        static void _AssertLayout()
        {
            // The cached 132-byte disconnect record plus miNumFilesToLoad exactly fill the
            // X360 gap from +0x3750 to mfTimeInState at +0x37D8 -- every byte in it is a
            // scalar or a char, so the console delta must survive the host.
            static_assert(offsetof(CrashNavEnterOnlineBase, mfTimeInState)
                        - offsetof(CrashNavEnterOnlineBase, mCachedHandleDisconnectEvent) == 136,
                          "cached disconnect record + miNumFilesToLoad == 136 bytes");
            static_assert(offsetof(CrashNavEnterOnlineBase, mfScrollAmount)
                        - offsetof(CrashNavEnterOnlineBase, mfTimeInState) == 4, "scroll after time");
            static_assert(offsetof(CrashNavEnterOnlineBase, mfLastAxisValue)
                        - offsetof(CrashNavEnterOnlineBase, mfTimeInState) == 8, "axis after scroll");
            static_assert(offsetof(CrashNavEnterOnlineBase, meSubState)
                        - offsetof(CrashNavEnterOnlineBase, mfTimeInState) == 12, "substate after axis");
            static_assert(offsetof(CrashNavEnterOnlineBase, mbJunkyardEntered)
                        - offsetof(CrashNavEnterOnlineBase, mfTimeInState) == 16, "junkyard byte");
            static_assert(offsetof(CrashNavEnterOnlineBase, mbIgnoreDisconnectedPopup)
                        - offsetof(CrashNavEnterOnlineBase, mfTimeInState) == 17, "ignore-popup byte");
            static_assert(offsetof(CrashNavEnterOnlineBase, mbIsInvalidatingCollisionWorld)
                        - offsetof(CrashNavEnterOnlineBase, mfTimeInState) == 18, "invalidating byte");
            static_assert(offsetof(CrashNavEnterOnlineBase, mbCachedDisconnectEventToHandle)
                        - offsetof(CrashNavEnterOnlineBase, mfTimeInState) == 19, "cached-disconnect byte");
        }
    };
}
