#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/BrnGuiTextField.h"   // BrnGui::TextField (mMultiplierText / mMultSymbolText, embedded by value)

// BrnGui::ShowtimeInstantResultsState - the post-event Showtime instant-results
// presentation flow state. This header carries the class shape and the inline resource
// accessor (the one ledger function attributed to the header); the large
// component/substate machinery and the out-of-line state/virtual machinery are
// reconstructed with the class TU. Layout/virtuals from the DecFIGS DWARF
// (BrnShowtimeInstantResults.h).
namespace BrnGui
{
    struct ShowtimeInstantResultsState : public CgsGui::State
    {
        // DWARF BrnShowtimeInstantResults.h -- the results presentation sub-state machine.
        // meSubStateState selects which KF_*_DURATION ResetStateTimer primes mfTimeRemaining with.
        enum EResultsSubStateStates
        {
            E_SUBSTATE_NONE      = 0,
            E_SUBSTATE_TOTALLING = 1,
            E_SUBSTATE_SUMMARY   = 2,
            E_SUBSTATE_LEAVING   = 3,
        };

        // Per-sub-state timer durations (X360 .rdata; defined in the .cpp, cpp:33/34/35).
        static const f32 KF_TOTALLING_DURATION;   // 10.0f
        static const f32 KF_SUMMARY_DURATION;     //  3.0f
        static const f32 KF_TRANS_OUT_DURATION;   //  1.0f

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // Reconstructed in this TU (BrnShowtimeInstantResults.cpp):
        //   ResetStateTimer   @ 0x824B3BD0 -- reload mfTimeRemaining for the current sub-state.
        //   SetMultiplierText @ 0x824B3C38 -- push the score-multiplier presentation to its fields.
        void ResetStateTimer();
        void SetMultiplierText();

        // @ 0x82500930 - hands the Showtime instant-results state's static resource list
        // to the loader (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x82F26BB8 (.rdata)
        static const u32                    muNumResourcesToLoad; // @ 0x82F26BD0 (.rdata)

        // ---- instance members reached by the two bodied functions ---------------------------
        // X360 byte offsets (from `this`) are documentary: the CgsGui::State base pointer prefix
        // widens on the x64 gate, so absolute offsets are not pinned; members are reached BY NAME
        // (the faithful de-inlined form of the X360 direct load/store). Untouched spans between the
        // named members are modelled as correctly-sized opaque padding (FLAG: the intervening
        // component/substate machinery is owned by the class TU, not reconstructed here).
        u8               mPadToMultiplierText[0x78C];   // [State base .. +0x78C) (opaque prefix)
        BrnGui::TextField mMultiplierText;              // +0x78C (h:162)
        BrnGui::TextField mMultSymbolText;              // +0x8B4 (h:163, stride 0x128 == sizeof(TextField))
        u8               mPad9DC[0xA78 - 0x9DC];        // +0x9DC .. +0xA78 (opaque)
        bool             mabSubStateFlags[2];           // +0xA78 (h:172)
        u8               mPadA7A[0xA80 - 0xA7A];        // +0xA7A .. +0xA80 (align)
        EResultsSubStateStates meSubStateState;         // +0xA80 (h:174)
        u8               mPadA84[0xB48 - 0xA84];        // +0xA84 .. +0xB48 (opaque)
        f32              mfTimeRemaining;               // +0xB48 (h:180)
        u8               mPadB4C[0xB68 - 0xB4C];        // +0xB4C .. +0xB68 (opaque)
        s32              miCurrentMultiplier;           // +0xB68 (h:189, last member)
    };
}
