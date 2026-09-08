#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

#include "GameSource/Gui/Flow/HUD/Components/BrnInGameMessagesComponent.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h"
#include "GameSource/Gui/BrnGuiTextField.h"
// Native component members in DecFIGS order, verified by ARTIST call sites.
namespace BrnGui
{
    class GuiCache;   // GameSource/Gui/BrnGuiCache.h (held by pointer only)

    struct CrashedStuntHudState : public CgsGui::State
    {
        // Update's phase machine (DWARF BrnCrashedStuntHudState.h:62). NOTE the values differ from
        // the freeburn CrashedHudState's same-named enum: there is no GETCACHE phase here, so
        // LOADING is 0 and the run ends at IDLE == 4. Update's switch cases 0..4 match exactly.
        enum CrashInternalState
        {
            E_CRASHINTERNALSTATE_LOADING    = 0,
            E_CRASHINTERNALSTATE_WF_INIT    = 1,
            E_CRASHINTERNALSTATE_SETUPSTATE = 2,
            E_CRASHINTERNALSTATE_RUNNING    = 3,
            E_CRASHINTERNALSTATE_IDLE       = 4,
            E_CRASHINTERNALSTATE_COUNT      = 5,
        };

        // The score-tally sub-machine UpdateRunning drives (DWARF BrnCrashedStuntHudState.h:73).
        enum CrashRunningState
        {
            E_CRASHRUNNINGSTATE_NONE      = 0,
            E_CRASHRUNNINGSTATE_TRANSIN   = 1,
            E_CRASHRUNNINGSTATE_TALLYSCORE = 2,
            E_CRASHRUNNINGSTATE_TRANSOUT  = 3,
            E_CRASHRUNNINGSTATE_COUNT     = 4,
        };

        // ---- X360 vtable overrides (CgsGui::State virtuals) --------------------------
        virtual void OnEnter();   // @0x82476318
        virtual void OnLeave();   // @0x8247DF68
        virtual void Update();    // @0x82481CF0

        // @ 0x82508510 - hands the crashed-stunt HUD state's static resource list to the
        // loader (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

        // ---- Drain the state in-queue (UpdatePermenant @ 0x82476A00). ----
        // Non-virtual on X360 too; Update calls it every frame in every phase. COMPLETE -- all
        // five console arms are reconstructed, including the END_CSTNT exit.
        void UpdatePermenant();

        // The 12 GUI event ids OnEnter registers / OnLeave unregisters. The table is .rdata
        // @0x8205B17C (both call sites pass `li r5, 12`); the IDA export set carries no data
        // symbols, so the 12 words were read out of the XEX image. Statics: no effect on sizeof
        // or on any guest offset above.
        static const s32 maiEventToObserve[12];
        static const s32 miNumEventsObserved;

        // --- members (DWARF order; base CgsGui::State occupies guest +0x00..+0x38) ---

        CrashInternalState meInternalState;   // guest +0x38
        CrashRunningState  meRunningState;    // guest +0x3C
        GuiCache*          mpCache;           // guest +0x40 (filled from the GUI-64 cache event)
        bool               mbHudMessages;     // guest +0x44
        bool               mbBoostBar;        // guest +0x45

        InGameMessagesComponent mHudMessageComponent;
        AnimationComponent mStuntScoreAnimator;
        AnimationComponent mStuntMultiplierAnimator;
        AnimationComponent mScoreTallyAnimator;
        TextField mStuntRunScoreText;
        TextField mStuntRunMultiplierText;

        f32 mfTallyScoreStartTime;   // guest +0x888
        s32 miStartMultiplier;       // guest +0x88C
        s32 miStartScore;            // guest +0x890
        s32 miFinishMultiplier;      // guest +0x894
        s32 miFinishScore;           // guest +0x898
        s32 miCurrentScore;          // guest +0x89C
        s32 miCurrentMultiplier;     // guest +0x8A0

        BrnFlapt::MovieClipRef mCrashHudAnimator;
        bool UpdateLoading();
        bool UpdateWFInit();
        bool UpdateSetupState();
        bool UpdateRunning();
        void UpdateCrashRunningState();
        void SetExpectedAptComponentList();

    private:
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x82F26488 (.rdata)
        static const u32                    muNumResourcesToLoad; // @ 0x82F264A8 (.rdata)
    };
}
