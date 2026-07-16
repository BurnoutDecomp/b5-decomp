#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"            // CgsIDConvertToString
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf
#include "GameSource/Gui/BrnGuiCache.h"                   // GuiCache accessors

// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Rival event-flow chatter (wave-B group 5): the three functions assigned to this
// partfile are all BLOCKED by the same frozen-header defect that already blocked
// HandleTraitorousTakedown (group 1, BrnGuiHudMessageAnalyzer_wB_01.cpp):
//
//   HudMessageAnalyzer::HandleJoinedEvent      @0x8251D078
//   HudMessageAnalyzer::HandleEliminatedEvent  @0x8251D0E8
//   HudMessageAnalyzer::HandleNetworkTailing   @0x8251CCD8
//
// The frozen header declares all three `const` (h:223/226/229). Each one's faithful
// X360 body ends in `bl sub_82517AF8` == TriggerMessage(const GuiHudMessage*) to fire
// the message it just built with AddParam. That overload is declared NON-const
// (h:139); the only const TriggerMessage overload (h:181) takes a CgsID hash, not a
// prebuilt message with params, so it cannot stand in. A const method cannot call the
// non-const overload -> MSVC C2663 (verified: qualifiers lost on `this`).
//
// This is a header-level const-correctness gap, not something a partfile can fix
// without editing the frozen header (forbidden) or substituting a different call than
// the asm makes (unfaithful). The one-line fix -- const-qualify
// TriggerMessage(const GuiHudMessage*) in the header -- unblocks this whole family of
// const message-firing handlers (HandleTraitorousTakedown, HandleImpact,
// HandleRivalryChangeEvent, HandleRivalFleeingEvent, HandleSignatureStunt,
// HandleTookLead, HandleTookLast, HandleRemotePlayerDisconnect, plus these three).
//
// No bodies are emitted here so the partfile stays gate-clean; the three functions are
// reported in funcs_blocked with this exact gap.

namespace BrnGui
{
}   // namespace BrnGui
