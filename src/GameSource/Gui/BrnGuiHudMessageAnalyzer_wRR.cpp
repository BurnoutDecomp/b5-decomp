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
// ---------------------------------------------------------------------------
// EVENT 427 (GuiEventRoadRageTimeExtended) -- re-verified against the image 2026-09-03
// (lane H). Every claim below is from the asm, not the pseudocode.
//
// (a) HudMessageAnalyzer::Update @0x82525FC0 has NO 427 arm. Its third dispatch table
//     is `addi r11, r3, -0x1A3 ; cmplwi r11, 0xB1` @0x82526BD0 (base id 419, 178
//     cases). 427 is case 8, and case 8 is in jpt_82526BF0's DEFAULT list
//     (cases 3,5,7-9,13,15-19,22-25,...). The 43 populated cases are 0,1,2,4,6,10,11,
//     12,14,20,21,26-36,63-72,119,121,123,129-133,157,159,177 -- i.e. ids 419..596.
// (b) mbTimeExtMsgPending (X360 +0x3D8) has a CONSUMER but no SETTER:
//       reader  HandleCrashedEvent @0x8251C7C0 (0x8251CA28 `lbz 0x3D8`): on crash-bar
//               states 1/3 (LEAVE_CRASHED / LEAVE_TAKEDOWN) it clears the flag and
//               triggers GuiHudMessage "EventRRTime" (_wB_02.cpp:46 has this arm).
//       writers Construct @0x82509110 (0), Update case 322 @0x82526AC8 (0, with 0x3D9),
//               HandleCrashedEvent @0x8251CA38 (0). All three store ZERO.
//     The analyzer is embedded at GuiModule+0xA1600 (GuiModule::Construct
//     @0x8251872C `addis r3, r31, 0xA ; addi r3, r3, 0x1600`); a scan of every
//     GuiModule-namespace export for a store to +0xA19D8 found none. So on the retail
//     X360 the "EventRRTime" message is dead code, and the DWARF-only
//     HandleRoadRageTimeExtension (h:388) has no body in the image.
// (c) THE REAL CONSOLE CONSUMER OF 427 IS THE ABOVE-CAR RENDERER, not this analyzer:
//       CustomRendererManager routes 427 (and 377) to AboveCarRenderer (the tree's
//       BrnCustomRenderer.cpp:422-432 already does).
//       AboveCarRenderer::RecvEvent @0x824544D8:
//         id 427 @0x8245499C..0x824549B0: `stb 1, 0x6B2(this)` mbTimeExtensionPending
//                                          `lwz r11, 0(ev) ; sth r11, 0x6B0(this)`
//                                          miTimeExtension (s16 <- the 4-byte seconds)
//         id 377 @0x824548FC..0x82454988: if (payload word == 3 /*LEAVE_TAKEDOWN*/ &&
//                 mbTimeExtensionPending) { if (!maBankingScores.IsFull()) Append(
//                 BankingScore{ mv2ScreenSpacePosition = (0.0f, flt_82F257D0 = 400.0f),
//                 world pos = 0, miBaseScore = miTimeExtension, miComboBonus = 0,
//                 mbIsRoadRageTimeExtension = true }); } mbTimeExtensionPending = false
//                 (the clear @0x82454988 runs even when the array is full).
//       RenderBankingScores @0x8245BF28 draws that entry through
//       LanguageManager::FindString("TIME_EXTENSION_SECONDS") @0x8245C5E4 +
//       FormatCurrencyString -- the floating "+N s" the player sees after the takedown
//       camera drops. Construct @0x82454478 `stb r30(=0), 0x6B2` clears the BOOL
//       (the tree's BrnAboveCarRenderer.cpp:107 attributes that store to the s16
//       miTimeExtension -- swapped; see the lane-H report). None of this is live in
//       this build: AboveCarRenderer.cpp is unmounted (RenderComponent /
//       RenderBankingScores declaration-only, RecvEvent undeclared) and
//       mapCustomRenderComponents[E_ABOVECAR] is 0 (BrnCustomRenderer.cpp:199).
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
