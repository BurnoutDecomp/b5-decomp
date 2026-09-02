#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"

#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"        // GuiEventPlayerReachedRoadRageTarget / GuiEventRoadRagePlayerDamage (raw-record placeholders)

// ============================================================================
// GameSource/Gui/BrnGuiHudMessageAnalyzer_wRR.cpp
//
// BrnGui::HudMessageAnalyzer -- the two ROAD RAGE event handlers. Reconstructed from
// BURNOUT_X360_ARTIST.XEX (road-rage wave, 2026-09-02).
//
//   HandleRoadRageTargetReached      DWARF BrnGuiHudMessageAnalyzer.h:478 / .cpp:1817
//   HandleRoadRagePlayerDamageEvent  DWARF BrnGuiHudMessageAnalyzer.h:296 / .cpp:2990
//
// ⚠ NEITHER HAS AN X360 SYMBOL. The ARTIST image carries no standalone
//   HandleRoadRage* function (zero rows in progress/identity.json / status.json); the
//   compiler folded both into HudMessageAnalyzer::Update @0x82525FC0's dispatch switch:
//
//     case 168 (GuiEventPlayerReachedRoadRageTarget), first jump table (id 64 + 104):
//         this->field_3DA = 1;                       -- mbRoadRageTargetReached = true
//     case 348 (GuiEventRoadRagePlayerDamage), second jump table (id 313 + 35):
//         if ( *(v10 + 4) ) this->field_3D9 = 1;     -- if (mbOneMoreCrashToTotalled)
//                                                       mbRoadOneMoreCrashToTotalledPending = true
//
//   BrnGuiHudMessageAnalyzer_wB_12.cpp reproduces both arms INLINE at :114 / :262, exactly
//   as the console has them, and is deliberately left untouched by this partfile. The
//   bodies below are the DWARF-attested member functions the console inlined -- the
//   inlining-reversal form of those two arms -- and are behaviourally identical to them.
//   They are NOT called from the switch today; wiring them in place of the inline
//   statements is a pure refactor the conductor may opt into (two one-line edits in
//   _wB_12.cpp), and would make this partfile a link requirement of that TU.
//
// The two pending flags are CONSUMED elsewhere, and both consumers already exist:
//   * mbRoadRageTargetReached -> Update's per-frame tail (_wB_12.cpp:714): after
//     KF_MIN_ROADRAGE_TARGET_ACHIEVED_DELAY, and only in crash-entry state 1/3,
//     TriggerMessage("RRTargGot").
//   * mbRoadOneMoreCrashToTotalledPending -> HandleCrashBarStateChange (_wB_02.cpp:52):
//     on E_CRASHBARSTATE_LEAVE_CRASHED / _LEAVE_TAKEDOWN, TriggerMessage("RRDamCrit").
//
// Event 427 (GuiEventRoadRageTimeExtended) has NO arm on the console: the third jump
// table (id 419 + case) covers 419/420/421/423/425/429 only, and mbTimeExtMsgPending
// (X360 0x3D8) is never SET anywhere in Update -- only cleared at case 322. The
// DWARF-only HandleRoadRageTimeExtension (h:388) is likewise absent from the image.
// ============================================================================

namespace BrnGui
{

// DWARF .h:478 -- the road-rage target has just been met; arm the delayed "RRTargGot"
// message. The event carries no payload the console reads (`lpEvent` is unused on the
// X360 as well: case 168 is a bare `stb 1, 0x3DA`).
void HudMessageAnalyzer::HandleRoadRageTargetReached(const GuiEventPlayerReachedRoadRageTarget* /*lpEvent*/)
{
    mbRoadRageTargetReached = true;
}

// DWARF .h:296 -- a road-rage damage report. Only the "one more crash totals the car"
// flag is consulted: DWARF GuiEventRoadRagePlayerDamage (BrnGuiEventTypeDefs.h:2546-2548)
// is { f32 mfHowCloseToTotalled (+0); bool mbOneMoreCrashToTotalled (+4);
// bool mbPlayerTotalled (+5) }, and the console tests byte +4 (`lbz 4(r?)` in case 348).
// The tree's home for the record is still the raw-byte placeholder, so the byte is read
// through maData[4] exactly as _wB_12.cpp:263 does; upgrading the placeholder to the
// named DWARF shape belongs to whoever homes the producer.
void HudMessageAnalyzer::HandleRoadRagePlayerDamageEvent(const GuiEventRoadRagePlayerDamage* lpEvent)
{
    if (lpEvent->maData[4])                      // mbOneMoreCrashToTotalled
    {
        mbRoadOneMoreCrashToTotalledPending = true;
    }
}

}
