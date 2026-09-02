#include "GameSource/Gui/Flow/HUD/Components/BrnEventInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"     // CgsCore::SPrintf
#include "GameSource/Gui/BrnGuiCache.h"                     // GuiCache::GetCurrentTimeInEvent / Get*TakedownsInEvent
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"      // TextFieldRef::SetText / SetLocalisedText(s32, format)

// ============================================================================
// GameSource/Gui/Flow/HUD/Components/BrnEventInfo_wRR.cpp
//
// BrnGui::EventInfoComponent -- THE ROAD RAGE READOUT. Reconstructed from
// BURNOUT_X360_ARTIST.XEX (road-rage wave, 2026-09-02). Bodied here:
//
//   UpdateRoadRage          @0x82429A48   (DWARF BrnEventInfo.h:460) -- Update's case 3:
//                                         the event clock + the "N / target" takedown digits
//   SetTakedownsTextState   @0x82421758   (DWARF BrnEventInfo.h:368) -- Normal / Warning
//                                         text state off current-vs-target takedowns
//
// Partfile sibling of BrnEventInfo.cpp / _wS1 / _wS2 (one class, one header; must be
// mounted with them). Nothing here defines a .rdata table; every string below is a
// literal the X360 body loads directly.
//
// ---------------------------------------------------------------------------
// LAYOUT / CALLEE PINS (all from the asm @0x82429A48):
//   0x82429A9C  lfs  f13, 0x3FC(r31)   mfTimeLeft            (change test vs GuiCache::mfEventTime)
//   0x82429B14  addi r30, r31, 0x1C    maTextField[0]        (the clock; "%dm%02ds" via SetText)
//   0x82429B44  lbz  r11, 0x3E4(r31)   mbTimeRemainingFlashing
//   0x82429B5C  addi r3, r31, 0x1D8    mTimeAnimatorRRage    Run("flashing")     -- below 10 s
//   0x82429B7C  addi r3, r31, 0x210    mTimeAnimatorBRoute   Run("notFlashing")  -- at/above 10 s
//   0x82429B98  lwz  r11, 0x400(r31)   miCurrentTakedowns
//   0x82429BB4  addi r3, r31, 0x34     maTextField[2]        SetLocalisedText(current, 11)
//   0x82429BC8  lwz  r11, 0x404(r31)   miTargetTakedowns
//   0x82429BE4  addi r3, r31, 0x40     maTextField[3]        SetLocalisedText(target, 11)
//   0x82429BBC / 0x82429BEC  bl sub_8246CF18 == TextFieldRef::SetLocalisedText(s32, format)
//                                         (BrnFlaptTextFieldRef.cpp:137 assert, FormatText(s32),
//                                          SetText(buf, true)); `li r5, 0xB` == E_FORMAT_INTEGER.
//
// ⚠ THE ANIMATOR ASYMMETRY IS CONSOLE-FAITHFUL. The "flashing" edge runs the road-rage
//   time animator (+0x1D8) but the "notFlashing" edge runs the BURNING-ROUTE time animator
//   (+0x210) -- and PrepareComponentsForGameMode's road-rage arm (BrnEventInfo_wS1.cpp:309)
//   prepares only mTimeAnimatorRRage. FlaptAnimatorComponent::Run on an unprepared animator
//   iterates zero child clips (BrnGuiFlaptIconComponent.cpp:213), so on the console the
//   road-rage clock, once it starts flashing under 10 s, never visibly stops flashing when a
//   time extension lifts it back above 10 s (only mbTimeRemainingFlashing is cleared). The
//   sibling UpdateStuntAttack @0x82429F58-0x82429F98 drives ONE animator (+0x248) for both
//   edges; this arm does not. Transcribed as the binary has it -- retail behaviour is the
//   target, not a fix.
//
// The clock arithmetic is the same shape as UpdateStuntAttack's: `fmuls` by flt_820139F8
// (1/60) then `fctiwz` (TRUNCATION), then `fnmsubs f0, (f32)minutes, 60.0f, time` and a
// second `fctiwz`. Both conversions truncate; neither rounds.
//
// GuiCache::mfEventTime is read TWICE on the console -- once through the inlined
// GetCurrentTimeInEvent() (which carries the "0.0f <= mfEventTime" assert,
// BrnGuiCache.h:2962) for the change test, and once through the real call to refresh
// mfTimeLeft. Both are restored as getter calls; the getter carries the assert.
// ============================================================================

namespace BrnGui
{

namespace
{
    // SetTextFieldDangerColour's ramp start, and the threshold that gates the timer's
    // flashing animator (flt_82004A20, `fcmpu` @0x82429B48). DWARF names it
    // KF_DANGERTIME_START (BrnEventInfo.h). Same value the sibling partfiles spell.
    const f32 KF_DANGERTIME_START = 10.0f;                    // 0x41200000

    // The clock split. flt_820139F8 is the reciprocal the console multiplies by
    // (`fmuls` @0x82429AD8); flt_82004C6C is the minute it subtracts back out
    // (`fnmsubs` @0x82429B00).
    const f32 KF_ONE_OVER_SECONDS_PER_MINUTE = 0.016666668f;  // 0x3C888889
    const f32 KF_SECONDS_PER_MINUTE          = 60.0f;         // 0x42700000

    // CgsCore::SPrintf's stack buffer (v21[160] on the frame; `li r4, 0x80` is the cap).
    const s32 KI_TEXT_BUFFER_SIZE = 128;

    // CgsLanguage::LanguageManager::ParameterFormatType, as the raw integer the X360
    // passes (`li r5, 0xB` @0x82429BB0 / @0x82429BE0) -- the convention
    // BrnFlaptTextFieldRef.h documents.
    const s32 KI_FORMAT_INTEGER = 11;                         // E_FORMAT_INTEGER

    // The takedown-counter text state SetTakedownsTextState drives on
    // mTextStateAnimatorRRage.
    const char* const KPC_TAKEDOWNS_STATE_NORMAL  = "Normal";
    const char* const KPC_TAKEDOWNS_STATE_WARNING = "Warning";

    // The clock's flashing states.
    const char* const KPC_TIME_FLASHING     = "flashing";
    const char* const KPC_TIME_NOT_FLASHING = "notFlashing";
}

// ---------------------------------------------------------------------------
// @0x82429A48 (DWARF BrnEventInfo.h:460) -- the road-rage event readout, called every
// frame by Update @0x82435430 for mode 3.
//
//   1. the clock          -> maTextField[0]   (only on a change of mfEventTime)
//   2. current takedowns  -> maTextField[2]   (only on a change)
//   3. target takedowns   -> maTextField[3]   (only on a change)
//   4. text state + digit layout, unconditionally.
// ---------------------------------------------------------------------------
void EventInfoComponent::UpdateRoadRage(GuiCache* lpCache)
{
    char lacNewText[KI_TEXT_BUFFER_SIZE];

    // ---- 1. the clock @0x82429A58-0x82429B8C -------------------------------
    if (lpCache->GetCurrentTimeInEvent() != mfTimeLeft)
    {
        mfTimeLeft = lpCache->GetCurrentTimeInEvent();

        const s32 liMinutes = static_cast<s32>(mfTimeLeft * KF_ONE_OVER_SECONDS_PER_MINUTE);
        const s32 liSeconds = static_cast<s32>(mfTimeLeft
                                             - static_cast<f32>(liMinutes) * KF_SECONDS_PER_MINUTE);

        CgsCore::SPrintf(lacNewText, KI_TEXT_BUFFER_SIZE, "%dm%02ds", liMinutes, liSeconds);
        maTextField[0].SetText(lacNewText, false);

        SetTextFieldDangerColour(&maTextField[0], mfTimeLeft);

        if (mfTimeLeft >= KF_DANGERTIME_START)
        {
            if (mbTimeRemainingFlashing)
            {
                // +0x210 -- the burning-route animator, as the binary has it (see banner).
                mTimeAnimatorBRoute.Run(KPC_TIME_NOT_FLASHING);
                mbTimeRemainingFlashing = false;
            }
        }
        else if (!mbTimeRemainingFlashing)
        {
            mTimeAnimatorRRage.Run(KPC_TIME_FLASHING);                       // +0x1D8
            mbTimeRemainingFlashing = true;
        }
    }

    // ---- 2. current takedowns -> maTextField[2] @0x82429B90-0x82429BBC ---
    if (lpCache->GetCurrentTakedownsInEvent() != miCurrentTakedowns)
    {
        miCurrentTakedowns = lpCache->GetCurrentTakedownsInEvent();
        maTextField[2].SetLocalisedText(miCurrentTakedowns, KI_FORMAT_INTEGER);
    }

    // ---- 3. target takedowns -> maTextField[3] @0x82429BC0-0x82429BEC ----
    if (lpCache->GetTargetTakedownsInEvent() != miTargetTakedowns)
    {
        miTargetTakedowns = lpCache->GetTargetTakedownsInEvent();
        maTextField[3].SetLocalisedText(miTargetTakedowns, KI_FORMAT_INTEGER);
    }

    // ---- 4. the two animator states, every frame @0x82429BF0-0x82429BFC --
    SetTakedownsTextState();
    SetTakedownsDigitsState();
}

// ---------------------------------------------------------------------------
// @0x82421758 (DWARF BrnEventInfo.h:368; asserts at BrnEventInfo.cpp:2201/2202) --
// pick the takedown counter's text state: "Normal" once the target is met,
// "Warning" while it is not. Drives mTextStateAnimatorRRage (+0x130,
// `addi r3, r31, 0x130` @0x82421868).
//
// The console's two asserts stream the offending value after the message
// ("Invalid takedown count provided - " << miCurrentTakedowns, via the StrStream
// helper sub_821F0E50); the sibling SetTakedownsDigitsState in BrnEventInfo.cpp keeps
// the message text only, and this body follows that convention.
// ---------------------------------------------------------------------------
void EventInfoComponent::SetTakedownsTextState()
{
    CGS_ASSERT(miCurrentTakedowns >= 0,
               "Invalid takedown count provided - ");
    CGS_ASSERT(miTargetTakedowns > 0,
               "Invalid takedown target provided - ");

    if (miCurrentTakedowns >= miTargetTakedowns)
    {
        mTextStateAnimatorRRage.Run(KPC_TAKEDOWNS_STATE_NORMAL);
    }
    else
    {
        mTextStateAnimatorRRage.Run(KPC_TAKEDOWNS_STATE_WARNING);
    }
}

}
