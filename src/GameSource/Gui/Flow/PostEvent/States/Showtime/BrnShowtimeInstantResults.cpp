// ===================================================================================
// BrnGui::ShowtimeInstantResultsState  -- implementation
//   class:BrnGui::ShowtimeInstantResultsState
//
//   ResetStateTimer   @ 0x824B3BD0 -- reload mfTimeRemaining for the current sub-state.
//   SetMultiplierText @ 0x824B3C38 -- push the score-multiplier presentation to its fields.
// Reconstructed store-for-store from the X360 asm.
// ===================================================================================
#include "GameSource/Gui/Flow/PostEvent/States/Showtime/BrnShowtimeInstantResults.h"
#include "GameSource/Gui/BrnGuiTextField.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGui
{
    // X360 .rdata durations (loaded by ResetStateTimer for the TOTALLING/SUMMARY/LEAVING cases).
    const f32 ShowtimeInstantResultsState::KF_TOTALLING_DURATION = 10.0f;   // cpp:33
    const f32 ShowtimeInstantResultsState::KF_SUMMARY_DURATION   =  3.0f;   // cpp:34
    const f32 ShowtimeInstantResultsState::KF_TRANS_OUT_DURATION =  1.0f;   // cpp:35

    // External helper: formats an int into a TextField (max 11 chars). Owned by its own TU;
    // forward-declared at file scope (the de-inlined form of the X360 `bl 0x824E7708`).
    extern int sub_824E7708(BrnGui::TextField* lpField, s32 liValue, s32 liMaxChars);

    // @0x824B3BD0 -- reload mfTimeRemaining with the duration for the current sub-state.
    // If a sub-state "finishing" flag is already raised (mabSubStateFlags[0]) the timer is
    // zeroed; otherwise it is primed from the matching KF_*_DURATION. The X360 loads three
    // distinct .rdata float constants for the TOTALLING(1)/SUMMARY(2)/LEAVING(3) cases
    // (KF_TOTALLING_DURATION=10.0 / KF_SUMMARY_DURATION=3.0 / KF_TRANS_OUT_DURATION=1.0) and
    // flt_82001CC0 (0.0) for the gate/default path.
    void ShowtimeInstantResultsState::ResetStateTimer()
    {
        if (mabSubStateFlags[0])
        {
            mfTimeRemaining = 0.0f;
            return;
        }

        switch (meSubStateState)
        {
        case E_SUBSTATE_TOTALLING:
            mfTimeRemaining = KF_TOTALLING_DURATION;
            break;
        case E_SUBSTATE_SUMMARY:
            mfTimeRemaining = KF_SUMMARY_DURATION;
            break;
        case E_SUBSTATE_LEAVING:
            mfTimeRemaining = KF_TRANS_OUT_DURATION;
            break;
        default:
            mfTimeRemaining = 0.0f;
            break;
        }
    }

    // @0x824B3C38 -- push the score-multiplier presentation to its two fields. A multiplier
    // of 1 or less means "no multiplier": both the value field and the 'X' symbol field are
    // cleared (the X360 inlines macText[0]=0 + OutputAptData(); mirrored here as SetText("")
    // + OutputAptData(), matching committed BrnDriveThruMapPanel). Otherwise the value is
    // formatted into the multiplier field (max 11 chars) via the shared int->TextField
    // formatter at 0x824E7708 and the symbol field shows "X".
    void ShowtimeInstantResultsState::SetMultiplierText()
    {
        if (miCurrentMultiplier <= 1)
        {
            mMultiplierText.SetText("");
            mMultiplierText.OutputAptData();
            mMultSymbolText.SetText("");
            mMultSymbolText.OutputAptData();
        }
        else
        {
            sub_824E7708(&mMultiplierText, miCurrentMultiplier, 11);
            mMultSymbolText.SetText("X");
        }
    }

    // @0x824B3B30 -- return the next active event-sub-state to advance to. Scans mabSubStateFlags
    // forward from just past meActiveSubState for the first raised flag and returns its index; if
    // none is raised (or meActiveSubState is already the last), returns E_ACTIVE_SUBSTATE_EVENT_DONE.
    // Asserts the current sub-state is in range (non-fatal in this build).
    ShowtimeInstantResultsState::EResultsActiveSubStates
    ShowtimeInstantResultsState::GetNextSubstate()
    {
        CGS_ASSERT(meActiveSubState < E_ACTIVE_SUBSTATE_EVENT_COUNT,
                   "meActiveSubState < E_ACTIVE_SUBSTATE_EVENT_COUNT");

        for (s32 liIndex = meActiveSubState + 1; liIndex < E_ACTIVE_SUBSTATE_EVENT_COUNT; ++liIndex)
        {
            if (mabSubStateFlags[liIndex] == 1)
                return static_cast<EResultsActiveSubStates>(liIndex);
        }
        return E_ACTIVE_SUBSTATE_EVENT_DONE;
    }
}
