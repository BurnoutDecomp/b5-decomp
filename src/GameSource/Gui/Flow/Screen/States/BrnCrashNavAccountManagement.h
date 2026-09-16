#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuToggleGroup.h"   // MenuToggleGroupVarSize<3> @+0x40
#include "GameSource/Gui/BrnGuiTextField.h"                             // TextField mTOSText @+0x2E48

namespace CgsModule { struct Event; }

// ===================================================================================
// BrnGui::CrashNavAccountManagement -- CN_ACCT_MAN, the crash-nav online-account
// management tab (settings row 5). Terms-of-service text plus three YES/NO rows.
//
// ⭐⭐ WHY THIS CLASS GREW. Until now it was the LAST header-only SHELL in the
// reachable pause ring: it declared OnEnter / OnLeave / Update and defined none of
// them, so the tab registered for no events, received none, drew no rows and could
// not be left. Every sibling in the ring (MapMain, Settings, Profile, Options, Trax,
// Credits, ColourCalibrate, DriverDetails, Stats) already had its TU.
//
// Bodies reconstructed from BURNOUT_X360_ARTIST.XEX -- the class's WHOLE ledger set,
// 16 of 16:
//   OnEnter                     @0x824CE110   OnLeave                     @0x824CE228
//   Update                      @0x824E00A0   UpdateInitSetup             @0x824C1A58
//   UpdateLoading               @0x824CE2D0   UpdateWFInit                @0x824BFC88
//   UpdatePermanent             @0x824DE818   HandleControllerInput       @0x824D9698
//   HandleControllerAxis        @0x824B8090   HandleTriggers              @0x824B8180
//   HandleAccountSettings       @0x824B8218   HandleAccountUpdateComplete @0x824CE3D0
//   HandleNewsAndTOSEvent       @0x824BCFB0   ApplyAndSaveSettings        @0x824CE4C8
//   TriggerSound                @0x824CE698   GetResourcesToLoad          @0x82508B20
//
// X360 member offsets (all pinned by the asm, see the .cpp's per-body notes):
//   meState             +0x38     mpGuiCache        +0x3C     mMenuToggleGroup +0x40
//   mTOSText            +0x2E48   meTOSTextState    +0x2F70
//   mfScrollAccumulator +0x2F74   mfScrollAxis      +0x2F78
//   mbShareInfo1        +0x2F7C   mbShareInfo2      +0x2F7D   mbTelemetry      +0x2F7E
// (+0x40 + sizeof(MenuToggleGroupVarSize<3>) == 0x40 + 0x2E08 == 0x2E48, which is
//  what puts mTOSText there; the three bools are the three rows' answers.)
// ===================================================================================
namespace BrnGui
{
    class GuiCache;

    struct CrashNavAccountManagement : public CgsGui::State
    {
        // Update's switch subject (X360 this+0x38). The jump table @0x824E00D8 has
        // exactly six arms and the default asserts, so only these values are stored.
        enum EState
        {
            E_STATE_INIT_SETUP = 0,   // drain the cache event, latch mpGuiCache
            E_STATE_LOADING    = 1,   // wait for the resources, declare the apt components
            E_STATE_WF_INIT    = 2,   // wait for those components, then build the rows
            E_STATE_MAIN       = 3,   // interactive
            E_STATE_SAVING     = 4,   // settings published; waiting on the account update
            E_STATE_LEAVING    = 5,   // the only arm that suppresses UpdatePermanent
        };

        // The three toggle rows, in the order UpdateWFInit builds them and
        // ApplyAndSaveSettings reads them back.
        enum EToggleRow
        {
            E_TOGGLEROW_SHARE_INFO_1 = 0,
            E_TOGGLEROW_SHARE_INFO_2 = 1,
            E_TOGGLEROW_TELEMETRY    = 2,
            E_TOGGLEROW_COUNT        = 3,
        };

        // Index into KAPC_TOS_TEXTS (X360 .rdata @0x82F26FFC). HandleNewsAndTOSEvent
        // maps the news/TOS status word onto these.
        enum ETOSTextState
        {
            E_TOSTEXT_DOWNLOADING = 0,   // "$ONLINE_NEWS_DOWNLOADING_TOS"
            E_TOSTEXT_LOADED      = 1,   // "~TOS_TEXT"
            E_TOSTEXT_FAILED      = 2,   // "$ONLINE_NEWS_FAILED_DOWNLOAD_TOS"
            E_TOSTEXT_COUNT       = 3,
        };

        CrashNavAccountManagement();

        virtual void OnEnter();     // @0x824CE110
        virtual void OnLeave();     // @0x824CE228
        virtual void Update();      // @0x824E00A0

        // @ 0x82508B20 - hands the account-management state's static resource list to the
        // loader (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        bool UpdateInitSetup();                                            // @0x824C1A58
        bool UpdateLoading();                                              // @0x824CE2D0
        bool UpdateWFInit();                                               // @0x824BFC88
        void UpdatePermanent();                                            // @0x824DE818
        void HandleControllerInput(const CgsModule::Event* lpEvent);       // @0x824D9698
        void HandleControllerAxis(const CgsModule::Event* lpEvent);        // @0x824B8090
        void HandleTriggers(const CgsModule::Event* lpEvent);              // @0x824B8180
        void HandleAccountSettings(const CgsModule::Event* lpEvent);       // @0x824B8218
        void HandleAccountUpdateComplete(const CgsModule::Event* lpEvent); // @0x824CE3D0
        void HandleNewsAndTOSEvent(const CgsModule::Event* lpEvent);       // @0x824BCFB0
        void UpdateTOSScroll(const CgsModule::Event* lpEvent);             // UpdatePermanent's event-26 arm
        void ApplyAndSaveSettings();                                       // @0x824CE4C8
        void TriggerSound(s32 liAction);                                   // @0x824CE698
        // The shared 112-byte audio-trigger post the X360 inlines in TriggerSound.
        void PostAudioTrigger(s32 liAction, const char* lpacLabel);
        // Refresh mTOSText from meTOSTextState (the X360 inlines this three-statement
        // run at both of its sites -- UpdateWFInit's tail and HandleNewsAndTOSEvent's).
        void RefreshTOSText();

        static const CgsGui::sResourceTuple maResourcesToLoad[1];  // X360 @0x82F26FDC -- { 142, 4 }
        static const u32                    muNumResourcesToLoad;  // X360 @0x82F26FE4 -- 1

        EState                                  meState;              // X360 this+0x38
        GuiCache*                               mpGuiCache;           // X360 this+0x3C
        MenuToggleGroupVarSize<E_TOGGLEROW_COUNT> mMenuToggleGroup;   // X360 this+0x40
        TextField                               mTOSText;             // X360 this+0x2E48 ("TOSText")
        ETOSTextState                           meTOSTextState;       // X360 this+0x2F70
        f32                                     mfScrollAccumulator;  // X360 this+0x2F74
        f32                                     mfScrollAxis;         // X360 this+0x2F78
        bool                                    mbShareInfo1;         // X360 this+0x2F7C
        bool                                    mbShareInfo2;         // X360 this+0x2F7D
        bool                                    mbTelemetry;          // X360 this+0x2F7E
    };
}
