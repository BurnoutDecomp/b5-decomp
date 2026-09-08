#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"       // CgsGui::State (base)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // CgsGui::sResourceTuple (maResourcesToLoad)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"   // CgsGui::GuiComponent (mImpactTimePageChanger drives its AddOutputAptViewState)
#include "GameSource/Gui/Flow/Shared/Components/BrnButtonIcon.h"      // BrnGui::ButtonIconComponent (mImpactTimeButton)

#include "GameSource/Gui/Flow/HUD/Components/BrnInGameMessagesComponent.h"
#include "GameSource/Gui/Flow/HUD/Components/BrnRoadRuleShotComponent.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h"
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptHelpItem.h"

// Member order follows DecFIGS, with native host component types.
namespace BrnGui
{
    class GuiCache;
    struct CrashedHudState : public CgsGui::State
    {
        // The internal update state machine (DWARF BrnCrashedHudState.h:80).
        enum CrashInternalState
        {
            E_CRASHINTERNALSTATE_GETCACHE   = 0,
            E_CRASHINTERNALSTATE_LOADING    = 1,
            E_CRASHINTERNALSTATE_WF_INIT    = 2,
            E_CRASHINTERNALSTATE_SETUPSTATE = 3,
            E_CRASHINTERNALSTATE_RUNNING    = 4,
            E_CRASHINTERNALSTATE_IDLE       = 5,
            E_CRASHINTERNALSTATE_COUNT      = 6,
        };

        // Max expected apt-init components (DWARF KU_MAX_INIT_COMPONENTS_NUM = 8).
        static const u32 KU_MAX_INIT_COMPONENTS_NUM = 8;

        // ---- X360 vtable overrides (CgsGui::State virtuals) -------------------------
        virtual void OnEnter();   // @0x82475DD0
        virtual void OnLeave();   // @0x8247D308
        virtual void Update();    // @0x82481B88

        // @ 0x825084F0 -- hands the crashed HUD state's static resource list to the loader
        // (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad -- seven instructions,
        // the exact shape of the CrashedStuntHudState twin at 0x82508510).
        //
        // ⭐ WHY THIS MATTERS BEFORE OnEnter DOES. Without this override the flow's loader is
        // never told CRASHED needs anything, so B5CrashedHud is never requested -- and
        // OnEnter's FindChildMovieClip("CrashHUD_mc") would then resolve against a FLAPT file
        // that does not contain the clip, leaving mpMovieClipInst null right before the
        // console's unconditional ResetTimeline. The declaration has to precede the body.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

        // ---- Drain the state in-queue (UpdatePermenant @ 0x824812A0). ----
        // Non-virtual on X360 too; Update calls it every frame in every phase. PARTIAL:
        // online mugshot and challenge handling remain deferred.
        void UpdatePermenant();

        // ---- Store an expected apt-component id (SetExpectedComponent @ 0x82473780). ----
        void SetExpectedComponent(const char* lpacComponentName);

        // ---- Enter the "impact time" apt page (EnterImpactTimeScreen @ 0x824738C0). ----
        void EnterImpactTimeScreen();

        // ---- Enter the "steer wreck" apt page (EnterSteerWreckScreen @ 0x82473868). ----
        void EnterSteerWreckScreen();

        void UpdateGetCache();
        bool UpdateLoading();
        bool UpdateWFInit();
        bool UpdateSetupState();
        void UpdateRunning();
        void SetExpectedAptComponentList();

        // The 21 GUI event ids OnEnter registers. The table is .rdata @0x8205B070; the IDA
        // export set carries no data symbols, so the address was decoded from OnEnter's own
        // `lis r11, ...@ha` / `addi r4, r11, ...@l` pair at 0x82475E6C/0x82475E78 and the 21
        // words were read out of the image. (The `li r5, 0x15` between them is the count.)
        // Statics: no effect on sizeof or on any guest offset below.
        static const s32 maiEventToObserve[21];
        static const s32 miNumEventsObserved;

        // The four FLAPT bundles CRASHED loads. Table @0x82F263A0, count @0x82F263C0 -- both
        // addresses decoded from GetResourcesToLoad's own instruction bytes, not guessed:
        //   0x825084F0  3D6082F2  lis  r11, 0x82F2
        //   0x825084F4  396B63A0  addi r11, r11, 0x63A0   -> 0x82F263A0
        //   0x82508500  816B63C0  lwz  r11, 0x63C0(r11)   -> 0x82F263C0
        // and the extent is self-confirming exactly as the CrashedStunt twin's is: four 8-byte
        // tuples end at 0x82F263C0, which is where the count word (reading 4) begins.
        static const CgsGui::sResourceTuple maResourcesToLoad[];   // @ 0x82F263A0 (.rdata)
        static const u32                    muNumResourcesToLoad;  // @ 0x82F263C0 (.rdata)

        // --- members (DWARF order; base CgsGui::State occupies guest +0x00..+0x38) ---

        // guest +0x38 : the expected apt-component id array.
        u32 mauExpectedComponentIds[KU_MAX_INIT_COMPONENTS_NUM];
        // guest +0x58 : number of ids currently stored in mauExpectedComponentIds.
        u32 muNumExpectedComponents;

        CrashInternalState meInternalState;
        bool mbInImpactTime;
        GuiCache* mpCache;
        bool mbHudMessages;
        bool mbBoostBar;
        InGameMessagesComponent mHudMessageComponent;
        AnimationComponent mImpactTimePageChanger;
        ButtonIconComponent mImpactTimeButton;
        bool mbImpactTimer;
        ButtonIconComponent mShowTimeButton1;
        ButtonIconComponent mShowTimeButton2;
        AnimationComponent mShowTimeAnimator;
        bool mbShowTime;
        AnimationComponent mMudAnimator;
        FlaptIconComponent mMugShotComponent;
        BrnFlapt::TextFieldRef mMugshotOpponentGamertag;
        RoadRuleShotComponent mRoadRuleShotComponent;
        bool mbSkipPrompt;
        FlaptAnimatorComponent mSkipPromptAnimator;
        FlaptHelpItem mSkipPromptButton;
        bool mbCrashIsSkippable;
        BrnFlapt::MovieClipRef mCrashHudAnimator;
    };
}
