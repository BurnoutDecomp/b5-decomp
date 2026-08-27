#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::CrashedStuntHudState - the CRASHEDSTNT HUD flow state: the crash screen shown when the
// player crashes DURING a stunt run (mode 7). Built by BrnHudFlow::Prepare as the lapStates[11]
// slot and Construct'd with CgsIDCompress("CRASHEDSTNT"). Derives from CgsGui::State.
// Distinct from BrnGui::CrashedHudState, which is the FREEBURN crash state.
//
// Class shape, member names/order and both enums are from the DecFIGS DWARF
// (BrnCrashedStuntHudState.h / .cpp). The X360 attests these guest byte offsets (the asm's base
// pointer is a u8*, so its load/store displacements are true byte offsets):
//   +0x38  meInternalState           (Update's switch selector)
//   +0x3C  meRunningState            (OnEnter zeroes it; UpdateSetupState sets TRANSIN)
//   +0x40  mpCache                   (filled by the GUI-64 cache event in UpdatePermenant)
//   +0x44  mbHudMessages             (OnEnter sets 1; UpdateLoading gates the controller leg on it)
//   +0x45  mbBoostBar                (OnEnter sets 1; UpdateSetupState clears it)
//   +0x48  mHudMessageComponent      (InGameMessagesComponent)
//   +0x494 mStuntScoreAnimator       (AnimationComponent; the three animators are 140 bytes each,
//   +0x520 mStuntMultiplierAnimator   which the 1172/1312/1452/1592 stride in OnEnter confirms)
//   +0x5AC mScoreTallyAnimator
//   +0x638 mStuntRunScoreText        (TextField; 296 bytes, confirmed by the 1592->1888->2184 stride)
//   +0x760 mStuntRunMultiplierText   (TextField)
//   +0x888 mfTallyScoreStartTime
//   +0x88C miStartMultiplier   +0x890 miStartScore
//   +0x894 miFinishMultiplier  +0x898 miFinishScore
//   +0x89C miCurrentScore      +0x8A0 miCurrentMultiplier
//   +0x8A4 mCrashHudAnimator         (MovieClipRef; the 8-byte pair OnEnter stores from
//                                     FindChildMovieClip("CrashHUD_mc"), and OnLeave's a1[553])
//
// The six score words are pinned by UpdateSetupState @0x8247D9E0, which reads the run's score from
// cache+40916 into +0x890 and its multiplier from cache+40920 into +0x88C, then sets +0x894 = 1 and
// +0x898 = score * multiplier -- i.e. the tally animates from (score, multiplier) to (score*mult, 1).
// That is what fixes which of the two "start"/"finish" pairs is the score and which the multiplier.
//
// PHASE NOTE: this header declares the DWARF members this wave actually touches, in DWARF order, and
// reserves the GUEST span of every member it does not model as opaque storage, so the offsets above
// stay checkable against the asm. The host layout still diverges from the guest one (pointer
// widening 4->8 in the base and in mpCache), and that is SAFE here because BrnHudFlow's
// NewPoolState<T> allocates sizeof(T) on the host rather than a hardcoded guest size
// (BrnHudFlow.cpp:44-49 says so explicitly). The five component sub-objects get their faithful
// layout when their TUs are homed; until then nothing in this tree reads inside their spans.
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
        // These reuse existing base vtable slots and add no data, so sizeof and every guest
        // offset recorded above are unchanged.
        virtual void OnEnter();   // @0x82476318 - PARTIAL, see the .cpp banner
        virtual void OnLeave();   // @0x8247DF68 - PARTIAL, see the .cpp banner
        virtual void Update();    // @0x82481CF0 - PARTIAL, see the .cpp banner

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

        // guest +0x48 : mHudMessageComponent (BrnGui::InGameMessagesComponent). Opaque this phase.
        u8  maHudMessageComponent[0x494 - 0x48];

        // guest +0x494 / +0x520 / +0x5AC : the three BrnGui::AnimationComponent members
        // (mStuntScoreAnimator / mStuntMultiplierAnimator / mScoreTallyAnimator). Opaque this phase.
        u8  maStuntScoreAnimator[0x520 - 0x494];
        u8  maStuntMultiplierAnimator[0x5AC - 0x520];
        u8  maScoreTallyAnimator[0x638 - 0x5AC];

        // guest +0x638 / +0x760 : the two BrnGui::TextField members. Opaque this phase -- note
        // OnEnter's byte store at guest +1886 lands INSIDE mStuntRunScoreText's span, so that
        // store is deferred with the component rather than guessed at.
        u8  maStuntRunScoreText[0x760 - 0x638];
        u8  maStuntRunMultiplierText[0x888 - 0x760];

        f32 mfTallyScoreStartTime;   // guest +0x888
        s32 miStartMultiplier;       // guest +0x88C
        s32 miStartScore;            // guest +0x890
        s32 miFinishMultiplier;      // guest +0x894
        s32 miFinishScore;           // guest +0x898
        s32 miCurrentScore;          // guest +0x89C
        s32 miCurrentMultiplier;     // guest +0x8A0

        // guest +0x8A4 : mCrashHudAnimator (BrnFlapt::MovieClipRef, the 8-byte {instance, ref}
        // pair). Opaque this phase -- its only producer (OnEnter's FindChildMovieClip leg) is
        // deferred, so nothing may read it. See the OnLeave banner.
        u8  maCrashHudAnimator[8];

    private:
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x82F26488 (.rdata)
        static const u32                    muNumResourcesToLoad; // @ 0x82F264A8 (.rdata)
    };
}
